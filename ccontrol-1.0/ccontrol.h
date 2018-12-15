/* Ccontrol goodness. */
#ifndef _CCONTROL_H
#define _CCONTROL_H
#include <stdbool.h>

struct string
{
	/* Start and end of this string, initial is pointer to beginning. */
	char *start, *end;
	char *start_of_file;
};

enum type
{
	TYPE_CC,
	TYPE_CPLUSPLUS,
	TYPE_LD,
	TYPE_MAKE,
	TYPE_OTHER,
	/* TYPE_OTHER is special, it doesn't get a slot in tables */
	LAST_TYPE = TYPE_MAKE,
};

struct add
{
	struct add *next;
	char *arg;
};

struct section
{
	struct string name;

	/* Run verbose (debugging) */
	bool verbose;

	/* Priority level */
	int nice;

	/* How many in parallel? */
	unsigned int cpus;

	/* What (if any) targets do we suppress parallel make? */
	char *no_parallel;

	/* Name of CC, C++, LD and MAKE replacements. */
	char *names[LAST_TYPE + 1];

	/* Path of ccache, distcc */
	char *ccache, *distcc;

	/* Distcc hosts. */
	char *distcc_hosts;
	bool distcplusplus_hosts_set;
	char *distcplusplus_hosts;

	/* Things to add to make line. */
	struct add *make_add;

	/* Things to add to environment. */
	struct add *env_add;
	
	/* lock file */
	int lock_fd;	   
	char *lock_file;	   
};

/* ccontrol.c */
#define verbose(v, fmt, ...) \
	do { if (v) __verbose((fmt), ## __VA_ARGS__, NULL); } while(0) 
void __verbose(const char *fmt, ...) __attribute__((sentinel));

/* ccontrol-parse.c */
struct section read_config(const char *configname, const char *dir, int fd);
char *resolve_path(const char *configured_path, const char *cmdname);

/* ccontrol-lock.c */
typedef void (*undofn_t)(void);
undofn_t grab_lock(int fd, struct section sec, enum type type);
void drop_slow_lock(void);

/* ccontrol-identify.c */
enum type what_am_i(char *argv[]);
bool can_distcc(char *argv[]);
#endif /* _CCONTROL_H */
