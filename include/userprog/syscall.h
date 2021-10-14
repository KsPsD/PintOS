#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);
struct lock file_rw_lock; // prevent simultaneous read, write (race condition prevention?
struct lock load_lock;
// void check_address(void *uaddr);
// static struct file *find_file_by_fd(int fd); //파일 디스크립터로 파일 찾아냄
// int add_file_to_fdt(struct file *file);      //파일 테이블에 파일들 저장함
// void remove_file_from_fdt(int fd);           //파일 디스크립터 테이블에서 파일을 지움
#endif /* userprog/syscall.h */
