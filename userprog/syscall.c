#include "userprog/syscall.h"
#include <stdio.h>
#include <list.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/flags.h"
#include "userprog/gdt.h"
/*------- Project 2-3. syscall  -------*/
#include "threads/palloc.h" // exec
#include "threads/vaddr.h" // exec
#include "userprog/process.h" // fork
#include "filesys/filesys.h" // create
#include "filesys/file.h"
#include <string.h>
/*------- Project 2-3. end -------*/
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

// Project2-4 File descriptor
static struct file *find_file_by_fd(int fd);
// Project2-extra
const int STDIN = 1;
const int STDOUT = 2;

/* Project 2-3 syscall */
void check_address(uaddr);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
int _write(int fd, const void *buffer, unsigned size); //temp
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
int dup2(int oldfd, int newfd);
/* Project 2-3 end */

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. 
 * Model Specific Register는 control register로서 대표적으로 test나 debugging에 쓰임.*/

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	// Project 2-4. File descriptor
	lock_init(&file_rw_lock);
}

/* The main system call interface */
/* system call handler가 control을 가질 때,
 * system call number 가 rax에 있고 %rdi, %rsi, %rdx, %r10, %r8 and %r9 순서로 arguments가 전달됨
 * caller's register은 struct intr_frame에 접근 가능. (intr_frame 은 kernel stack에 있음) */
// Process related : halt, exit, exec, wait
// File related : create, remove, open, filesize, read, write, seek, tell, close
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	/*------- Project 2-3. syscalls ------*/
	char *fn_copy;
	int siz;

	switch (f->R.rax)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC:
			if (exec(f->R.rdi) == -1)
				exit(-1);
			break;
		case SYS_WAIT:
			f->R.rax = process_wait(f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN: // file
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE: // file
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ: // file
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE: // file
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK: // file
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL: // file
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		case SYS_DUP2:
			f->R.rax = dup2(f->R.rdi, f->R.rsi);
			break;
		default:
			exit(-1);
			break;
	}
	/*------- Project 2-3. end ------*/

	// thread_exit ();
}

/*------- Project 2-3. syscalls ------*/

// Check validity of given user virtual address. Exits if any of below condition is met
// 1. Null pointer
// 2. A pointer to kernel virtual address space (above KERN_BASE)
// 3. A pointer to unmapped virtual memory (causes page_fault)
void check_address(const uint64_t *uaddr)
{
	struct thread *cur = thread_current(); // pml4 : page map level 4
	if (uaddr == NULL || !(is_user_vaddr(uaddr)) || pml4_get_page(cur->pml4, uaddr) == NULL)
	{
		exit(-1);
	}
}
// project 2-3 syscall 할 때 일단 구현
// Project 2-4. File descriptor
// Check if given fd is valid, return cur->fdTable[fd]
static struct file *find_file_by_fd(int fd)
{
	struct thread *cur = thread_current();
	// Error - invalid fd
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		
		return NULL;
	return cur->fdTable[fd]; // automatically returns NULL if empty
}

// Find open spot in current thread's fdt and put file in it. Returns the fd.
int add_file_to_fdt(struct file *file)
{
	struct thread *cur = thread_current();
	struct file **fdt = cur->fdTable; // file descriptor table

	// Project2-extra - (multi-oom) Find open spot from the front
	while (cur->fdIdx < FDCOUNT_LIMIT &&fdt[cur->fdIdx]) // fdt[] = 0인 곳 찾음
		cur->fdIdx++;

	// Error - fdt full
	if (cur->fdIdx >= FDCOUNT_LIMIT)
		return -1;

	fdt[cur->fdIdx] = file; // file descriptor table의 빈 곳에 file을 넣음
	return cur->fdIdx;
}

// Check for valid fd and do cur->fdTable[fd] = NULL. Returns nothing
void remove_file_from_fdt(int fd)
{
	struct thread *cur = thread_current();

	// Error - invalid fd
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	cur ->fdTable[fd] = NULL;
}


// Terminates Pintos by calling power_off(). No return
void halt(void)
{
	power_off();
}

// Terminates the current user program, returning status to the kernel
// End current thread, record exit status
// conventionally, status = 0은 성공, 0아닌 경우 에러를 나타냄
void exit(int status) // syscall에서 호출시 status = -1 <- 가능...?
{
	struct thread *cur = thread_current();
	cur->exit_status = status;
	
	printf("%s: exit(%d)\n", thread_name(), status); // Process Termination Message
	thread_exit();
}


// Create a new file called 'file' initially ;initial_size' bytes in size.
// Creating a new file does not open it: opening the new file is a separate
// operation which would require a 'open' system call
// Returns true if successful, false otherwise.
bool create (const char *file, unsigned initial_size)
{
	check_address(file);
	return filesys_create(file, initial_size);
}

// Delete the file called 'file'. Returns true if successful, falase otherwise.
// File is removed regardless of whether it is open or closed, and removing
// an open file does not close it.
bool remove (const char *file)
{
	check_address(file);
	return filesys_remove(file);
}


// parent : returns pid of child on success or -1 on fail
// child : Returns 0
// %RBX, %RSP, %RBP, %R12, %R15 들은 callee-saved registers이므로 clone할 필요 없다. (?)
tid_t fork(const char *thread_name, struct intr_frame *f) // 다시 마저 하기...
{
	return process_fork(thread_name, f);
}

// Run new 'executable(실행 파일)' from current process
// Don't confuse with open! 'open' just opens up any file (txt, executable), 'exec' runs only executable
// Never returns on success. Returns -1 on fail.
int exec(char *file_name)
{
	struct thread *cur = thread_current();
	check_address(file_name);

	// 문제점. SYS_EXEC - process_exec의 process_cleanup 때문에 f->R.rdi 날아감.
	// 여기서 file_name 동적할당해서 복사한 뒤, 그걸 넘겨주기
	int siz = strlen(file_name) + 1; // 문자열 끝에 NULL까지 포함한 길이 (+1)
	char *fn_copy = palloc_get_page(PAL_ZERO);
	if (fn_copy == NULL)
		exit(-1);
	strlcpy(fn_copy, file_name, siz); // strlcpy(char *dst, const char *src, size_t size);

	if (process_exec(fn_copy) == -1)
		return -1;
	
	// Not reachable
	NOT_REACHED();
	return 0;
}

// Opens the file called 'file'. return fd or -1 if the file could not be opened.
// 
int open(const char *file)
{
	check_address(file);
	struct file *fileobj = filesys_open(file);
	if (fileobj == NULL)
		return -1;
	int fd = add_file_to_fdt(fileobj);

	// FD table full
	if (fd == -1)
		file_close(fileobj);

	return fd;
}

// Returns the size, in bytes, of the file open as fd.
int filesize(int fd)
{
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj == NULL)
		return -1;
	return file_length(fileobj);
}

// Read size bytes from the file open as fd into buffer
// Returns the number of bytes actually read (0 at end of file), or -1 if the file could not be read
// STDIN 일 때, 키보드의 데이터를 읽어 버퍼에 저장 (input_getc() 이용)
int read(int fd, void *buffer, unsigned size) // buffer : 읽은 데이터 저장할 버퍼 주소 값
{
	check_address(buffer);
	int ret;
	struct thread *cur = thread_current();

	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj == NULL)
		return -1;

	if (fileobj == STDIN)
	{
		if (cur->stdin_count == 0)
		{
			// Not reachable
			NOT_REACHED();
			remove_file_from_fdt(fd);
			ret = -1;
		}
		else
		{
			int i;
			unsigned char *buf = buffer;
			for (i = 0; i <size; i++)
			{
				char c = input_getc(); // 키보드 입력 받은거 버퍼
				*buf++ = c;
				if (c == '\0')
				break;
			}
			ret = i;
		}
	}
	else if (fileobj == STDOUT)
	{
		ret = -1;
	}
	else
	{
		// Q. read는 동시접근 허용해도 되지 않음?
		lock_acquire(&file_rw_lock);
		ret = file_read(fileobj, buffer, size); // return bytes read
		lock_release(&file_rw_lock);
	}
	return ret;
}

// Writes size bytes from buffer to the open file fd.
// Returns the number of bytes actually written, or -1 if the file could not be written
int write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);
	int ret;
	
	struct file *fileobj = find_file_by_fd(fd);
	
	if (fileobj == NULL)
		
		return -1;
	
	struct thread *cur = thread_current();

	if (fileobj == STDOUT)
	{
		if (cur->stdout_count == 0)
		{
			// Not reachable
			NOT_REACHED();
			remove_file_from_fdt(fd);
			ret = -1;
		}
		else
		{
			putbuf(buffer, size); // 문자열을 화면에 출력해주는 함수
			ret = size;
		}
	}
	else if (fileobj == STDIN)
	{
		ret = -1;
	}
	else
	{
		lock_acquire(&file_rw_lock);
		ret = file_write(fileobj, buffer, size);
		lock_release(&file_rw_lock);
	}
	// printf("write _ right before return"); // 들어옴
	return ret;
}

// Changes the next byte to be read or written in open file to position,
// expredd in bytes from the beginning of the file (Thus, a position of 0 is the file's start)
// 파일의 위치(offset)를 이동하는 함수
void seek(int fd, unsigned position)
{
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj <= 2)
		return;
	fileobj->pos = position; // file 시작지점이 0이므로 position만큼 offset 기록
}

// Returns the position of the next byte to be read or written in open file fd,
// expressed in bytes from the beginning of the file.
// 파일의 위치(offset)를 알려주는 함수
unsigned tell(int fd)
{
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj <= 2)
		return;
	return file_tell(fileobj);
}

// Close file descriptor fd. Ignores NULL file. Returns nothing
void close(int fd)
{
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj == NULL)
	return;

	struct thread *cur = thread_current();

	if (fd == 0 || fileobj == STDIN)
	{
		cur->stdin_count--;
	}
	else if (fd == 1 || fileobj == STDOUT)
	{
		cur->stdout_count--;
	}

	remove_file_from_fdt(fd);
	if (fd <= 1 || fileobj <= 2)
		return;

	if (fileobj->dupCount == 0)
		file_close(fileobj);
	else
		fileobj->dupCount--;
}

// Creates 'copy' of oldfd into newfd. If newfd is open, close it. Returns newfd on success, -1 on fail (invalid oldfd)
// After dup2, oldfd and newfd 'shares' struct file, but closing newfd should not close oldfd (important)!
int dup2(int oldfd, int newfd)
{
	struct file *fileobj = find_file_by_fd(oldfd);
	if (fileobj == NULL)
		return -1;

	struct file *deadfile = find_file_by_fd(newfd);

	if (oldfd == newfd)
		return newfd;

	struct thread *cur = thread_current();
	struct file **fdt = cur->fdTable;

	// Don't literally copy, but just increase its count and share the same struct file
	// [syscall close] Only close it when count == 0

	// Copy stdin or stdout to another fd
	if (fileobj == STDIN)
		cur->stdin_count++;
	else if (fileobj == STDOUT)	
		cur->stdout_count++;
	else
		fileobj->dupCount++;

	close(newfd);
	fdt[newfd] = fileobj;
	return newfd;
}

// temp. argument passing 확인용 write
int _write (int fd UNUSED, const void *buffer, unsigned size) {
	// temporary code to pass args related test case
	putbuf(buffer, size);
	return size;
}

/*------- Project 2-3. end ------*/