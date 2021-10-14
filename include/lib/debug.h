#ifndef __LIB_DEBUG_H
#define __LIB_DEBUG_H

/* GCC lets us add "attributes" to functions, function
 * parameters, etc. to indicate their properties.
 * See the GCC manual for details. 
 * attribute는 gcc에서 사용할 수 있는 확장 기능 */
#define UNUSED __attribute__ ((unused)) // unused 속성 부여시 사용하지 않는 변수(UNUSED)에 대한 컴파일러의 경고가 없음
/* NO_RETURN 함수 prototpye에 붙어서 해당 함수가 return 되지 않음을 명시
   단순히 return 값이 없다는 것 이상으로 이 함수를 call한 caller에게 control이 돌아가지 않음을 의미
   따라서 NO_RETURN 속성을 가지는 함수 이후의 코드는 컴파일러에 의해 도달할 수 없는 코드로 최적화되어 제거된다.*/
#define NO_RETURN __attribute__ ((noreturn))
/* NO_INLINE : 함수 prototype에 붙어서 해당 함수를 inline 처리 되지 않음을 명시.
 * inline -> 컴파일 할 때 함수가 사용되는 모든 곳에 함수의 코드를 복사하여 넣어줌 */
#define NO_INLINE __attribute__ ((noinline))
/*  */
#define PRINTF_FORMAT(FMT, FIRST) __attribute__ ((format (printf, FMT, FIRST)))

/* Halts the OS, printing the source file name, line number, and
 * function name, plus a user-specific message. */
#define PANIC(...) debug_panic (__FILE__, __LINE__, __func__, __VA_ARGS__)

void debug_panic (const char *file, int line, const char *function,
		const char *message, ...) PRINTF_FORMAT (4, 5) NO_RETURN;
/* kernel panic이 발생했을 때 프로그램이 오류가 발생한 지점의 address를 역추적하여 알려준다.
 * 역추적한 address들은 16진수 표현이다. 이를 위해핀토스에서 backtrace라는 명령어를 제공한다. */
void debug_backtrace (void);

#endif



/* This is outside the header guard so that debug.h may be
 * included multiple times with different settings of NDEBUG. */
#undef ASSERT
#undef NOT_REACHED


/* NDEBUG는 디버그 모드가 아닌경우(릴리즈 모드)를 말한다.(Not DEBUG)
 * #ifndef는 매크로가 정의되지 않은 경우에만 코드를 컴파일 함.
 * ASSERT 매크로는 조건이 맞지 않을 때 프로그램 중단
*/
#ifndef NDEBUG
#define ASSERT(CONDITION)                                       \
	if ((CONDITION)) { } else {                             \
		PANIC ("assertion `%s' failed.", #CONDITION);   \
	}
#define NOT_REACHED() PANIC ("executed an unreachable statement");
#else
#define ASSERT(CONDITION) ((void) 0)
#define NOT_REACHED() for (;;)
#endif /* lib/debug.h */
