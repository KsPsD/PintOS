#include <syscall.h>
#include <stdint.h>
#include "../syscall-nr.h"
//유저 프로그램에서 시스템 콜을 호출하면 여기로 옴 그래서 여기서 레지스터 설정하고 내가 설정한 함수로 넘어가
__attribute__((always_inline)) static __inline int64_t syscall(uint64_t num_, uint64_t a1_, uint64_t a2_,
															   uint64_t a3_, uint64_t a4_, uint64_t a5_, uint64_t a6_)
{
	int64_t ret;
	register uint64_t *num asm("rax") = (uint64_t *)num_;
	register uint64_t *a1 asm("rdi") = (uint64_t *)a1_;
	register uint64_t *a2 asm("rsi") = (uint64_t *)a2_;
	register uint64_t *a3 asm("rdx") = (uint64_t *)a3_;
	register uint64_t *a4 asm("r10") = (uint64_t *)a4_;
	register uint64_t *a5 asm("r8") = (uint64_t *)a5_;
	register uint64_t *a6 asm("r9") = (uint64_t *)a6_;
	//어셈블리코드:출력변수:입력변수:변경되는 레지스터
	//asm volatile 은 이 키워드를 사용하면 컴파일러는 프로그래머가 입력한 그래도 남겨두게 된다.
	// 즉 최적화 나 위치를 옮기는 등의 일을 하지 않는다.
	// 예를 들어 output 변수중 하나가 인라인 어셈블리엔 명시되어 있지만 다른 곳에서 사용되지 않는다고 판단되면 컴파일러는 이 변수를 알아서 잘 없애주기도 한다.
	//이런 경우 이런 것을 고려해 프로그램을 짰다면 상관 없겠지만 만에 하나 컴파일러가 자동으로 해준 일 때문에 버그가 발생할 수도 있다. 그러므로 __volatile__ 키워드를 사용해 주는 것이 좋다.
	//밑에꺼는 rax에 우리 syscall넘버를 넣어주고 나머지들도 순서에 맞게 넣어줌 그 뒤 syscall 호출해서 결과를 리턴함
	__asm __volatile(
		"mov %1, %%rax\n"
		"mov %2, %%rdi\n"
		"mov %3, %%rsi\n"
		"mov %4, %%rdx\n"
		"mov %5, %%r10\n"
		"mov %6, %%r8\n"
		"mov %7, %%r9\n"
		"syscall\n"
		: "=a"(ret)
		: "g"(num), "g"(a1), "g"(a2), "g"(a3), "g"(a4), "g"(a5), "g"(a6)
		: "cc", "memory");
	return ret;
}

/* Invokes syscall NUMBER, passing no arguments, and returns the
   return value as an `int'. */
#define syscall0(NUMBER) ( \
	syscall(((uint64_t)NUMBER), 0, 0, 0, 0, 0, 0))

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0) ( \
	syscall(((uint64_t)NUMBER),  \
			((uint64_t)ARG0), 0, 0, 0, 0, 0))
/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
   returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1) ( \
	syscall(((uint64_t)NUMBER),        \
			((uint64_t)ARG0),          \
			((uint64_t)ARG1),          \
			0, 0, 0, 0))

#define syscall3(NUMBER, ARG0, ARG1, ARG2) ( \
	syscall(((uint64_t)NUMBER),              \
			((uint64_t)ARG0),                \
			((uint64_t)ARG1),                \
			((uint64_t)ARG2), 0, 0, 0))

#define syscall4(NUMBER, ARG0, ARG1, ARG2, ARG3) ( \
	syscall(((uint64_t *)NUMBER),                  \
			((uint64_t)ARG0),                      \
			((uint64_t)ARG1),                      \
			((uint64_t)ARG2),                      \
			((uint64_t)ARG3), 0, 0))

#define syscall5(NUMBER, ARG0, ARG1, ARG2, ARG3, ARG4) ( \
	syscall(((uint64_t)NUMBER),                          \
			((uint64_t)ARG0),                            \
			((uint64_t)ARG1),                            \
			((uint64_t)ARG2),                            \
			((uint64_t)ARG3),                            \
			((uint64_t)ARG4),                            \
			0))
void halt(void)
{
	syscall0(SYS_HALT);
	NOT_REACHED();
}

void exit(int status)
{
	syscall1(SYS_EXIT, status);
	NOT_REACHED();
}

pid_t fork(const char *thread_name)
{
	return (pid_t)syscall1(SYS_FORK, thread_name);
}

int exec(const char *file)
{
	return (pid_t)syscall1(SYS_EXEC, file);
}

int wait(pid_t pid)
{
	return syscall1(SYS_WAIT, pid);
}

bool create(const char *file, unsigned initial_size)
{
	return syscall2(SYS_CREATE, file, initial_size);
}

bool remove(const char *file)
{
	return syscall1(SYS_REMOVE, file);
}

int open(const char *file)
{
	return syscall1(SYS_OPEN, file);
}

int filesize(int fd)
{
	return syscall1(SYS_FILESIZE, fd);
}

int read(int fd, void *buffer, unsigned size)
{
	return syscall3(SYS_READ, fd, buffer, size);
}

int write(int fd, const void *buffer, unsigned size)
{
	return syscall3(SYS_WRITE, fd, buffer, size);
}

void seek(int fd, unsigned position)
{
	syscall2(SYS_SEEK, fd, position);
}

unsigned
tell(int fd)
{
	return syscall1(SYS_TELL, fd);
}

void close(int fd)
{
	syscall1(SYS_CLOSE, fd);
}

int dup2(int oldfd, int newfd)
{
	return syscall2(SYS_DUP2, oldfd, newfd);
}

void *
mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	return (void *)syscall5(SYS_MMAP, addr, length, writable, fd, offset);
}

void munmap(void *addr)
{
	syscall1(SYS_MUNMAP, addr);
}

bool chdir(const char *dir)
{
	return syscall1(SYS_CHDIR, dir);
}

bool mkdir(const char *dir)
{
	return syscall1(SYS_MKDIR, dir);
}

bool readdir(int fd, char name[READDIR_MAX_LEN + 1])
{
	return syscall2(SYS_READDIR, fd, name);
}

bool isdir(int fd)
{
	return syscall1(SYS_ISDIR, fd);
}

int inumber(int fd)
{
	return syscall1(SYS_INUMBER, fd);
}

int symlink(const char *target, const char *linkpath)
{
	return syscall2(SYS_SYMLINK, target, linkpath);
}

int mount(const char *path, int chan_no, int dev_no)
{
	return syscall3(SYS_MOUNT, path, chan_no, dev_no);
}

int umount(const char *path)
{
	return syscall1(SYS_UMOUNT, path);
}
