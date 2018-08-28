#ifndef RHEA_LOGGING_H
#define RHEA_LOGGING_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef COLOR_CONSOLE
	#define INVERT(str)	"\e[7m" str "\e[0m"
	#define BOLD(str)	"\e[1m" str "\e[0m"
	#define BAD(str)	"\e[31;1m" str "\e[0m"
#else
	#define INVERT(str)	str
	#define BOLD(str)	str
	#define BAD(str)	str
#endif

#define FMT_BYTE "0x%02X"
#define FMT_WORD "0x%04X"
#define FMT_DWORD "0x%08X"

#define PANIC(fmt, ...) \
{ \
	fprintf(stderr, \
		BAD("[" __FILE__ "] ") fmt "\n" \
			"  --> File: " __FILE__ "\n" \
			"  --> Line: %d\n", \
		##__VA_ARGS__, __LINE__); \
	exit(EXIT_FAILURE); \
}

#define FLOG(f, fmt, ...) rhea_log(f, fmt, ##__VA_ARGS__)

#define LOG(fmt, ...) FLOG(stdout, fmt, ##__VA_ARGS__)

#define FLOGF(f, fmt, ...) FLOG(f, "[" __FILE__ "] " fmt, ##__VA_ARGS__)

#define LOGF(fmt,...) FLOGF(stdout,fmt, ##__VA_ARGS__)

#define ASM(fmt, ...) FLOG(stdout, fmt "\n", ##__VA_ARGS__)

#define ERROR(fmt, ...) \
	FLOG(stderr, \
		BAD(__FILE__) " " fmt "\n", \
		##__VA_ARGS__);

int rhea_log(FILE *stream, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
