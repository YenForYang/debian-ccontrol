#ifndef _STDRUSTY_H
#define _STDRUSTY_H
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* Is A == B ? */
#define streq(a,b) (strcmp((a),(b)) == 0)

/* Does A start with B ? */
#define strstarts(a,b) (strncmp((a),(b),strlen(b)) == 0)

/* Does A end in B ? */
static inline bool strends(const char *a, const char *b)
{
	if (strlen(a) < strlen(b))
		return false;

	return streq(a + strlen(a) - strlen(b), b);
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

/* Upper bound to sprintf this simple type?  Each 3 bits < 1 digit. */
#define CHAR_SIZE(type) (((sizeof(type)*CHAR_BIT + 2) / 3) + 1)

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

/* Convenient wrappers for malloc and realloc.  Use them. */
#define new(type) ((type *)malloc_nofail(sizeof(type)))
#define new_array(type, num) realloc_array((type *)0, (num))
#define realloc_array(ptr, num) ((__typeof__(ptr))_realloc_array((ptr), sizeof((*(ptr))), (num)))

void *malloc_nofail(size_t size);
void *realloc_nofail(void *ptr, size_t size);
void *_realloc_array(void *ptr, size_t size, size_t num);

const char *int_to_string(int val);
void fatal(const char *msg, int err, ...) __attribute__((noreturn));

/* This version adds one byte (for nul term) */
void *suck_file(int fd, unsigned long *size);
void release_file(void *data, unsigned long size);

/* For writing daemons, based on Stevens. */
void daemonize(void);

/* Signal handling: returns fd to listen on. */
int signal_to_fd(int signal);
void close_signal(int fd);

#endif /* _STDRUSTY_H */
