#include <syscall.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);

// user program의 entry point. wrapper 함수이다.
// 커널은 user program 시작 전 레지스터에 첫 함수의 arguments를 넣어야한다.
void
_start (int argc, char *argv[]) {
	exit (main (argc, argv));
}
