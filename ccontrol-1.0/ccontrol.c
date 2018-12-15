#define _GNU_SOURCE
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "stdrusty.h"
#include "ccontrol.h"

static void insert_arg(char *argv[], unsigned argc, unsigned pos, char *arg)
{
	memmove(argv+pos+1, argv+pos, (argc+1) * sizeof(char *));
	argv[pos] = arg;
}

static void write_string(int fd, const char *str)
{
	write(fd, str, strlen(str));
}

void __verbose(const char *msg, ...)
{
	va_list arglist;
	const char *str;

	write_string(STDERR_FILENO, "   ccontrol(");
	write_string(STDERR_FILENO, int_to_string(getpid()));
	write_string(STDERR_FILENO, ") ");
	write_string(STDERR_FILENO, msg);

	va_start(arglist, msg);
	while ((str = va_arg(arglist, char *)) != NULL)
		write_string(STDERR_FILENO, str);
	va_end(arglist);

	write_string(STDERR_FILENO, "\n");
}

static bool make_target_match(char *argv[], char *targets)
{
	char *p, *target = "";
	unsigned int i;

	if (!targets)
		return false;

	if (!targets[0])
		return true;

	/* Heuristic: choose last arg which is neither option nor variable */
	for (i = 0; argv[i]; i++) {
		if (argv[i][0] == '-')
			continue;
		if (strchr(argv[i], '='))
			continue;
		target = argv[i];
	}

	for (p = strtok(targets, " \t"); p; p = strtok(NULL, " \t"))
		if (fnmatch(p, target, 0) == 0)
			return true;
	return false;
}

static bool already_has_dash_j(char *argv[])
{
	unsigned int i;
	const char *makeflags;

	for (i = 0; argv[i]; i++)
		if (strstarts(argv[i], "-j"))
			return true;

	/* Top-level -j<n> sets --jobserver-fds for kids. */
	makeflags = getenv("MAKEFLAGS");
	return makeflags && strstr(makeflags, "--jobserver-fds") != NULL;
}

/* Earlier arg parsing might have revealed distcc is useless. */
static void adjust_args_and_environment(struct section sec,
					enum type type,
					char *argv[], unsigned argc)
{
	struct add *add;

	if (type != TYPE_OTHER)
		argv[0] = sec.names[type];

	switch (type) {
	case TYPE_MAKE:
		/* Ensure make calls us for children, not /usr/bin/make. */
		setenv("MAKE", "make", 1);

		/* We insert -j to make in parallel. */
		if (!make_target_match(argv, sec.no_parallel)
		    && !already_has_dash_j(argv)) {
			const char *cpus = int_to_string(sec.cpus*20);
			char *arg = malloc(strlen(cpus) + 3);

			strcpy(arg, "-j");
			strcpy(arg + strlen(arg), cpus);
			insert_arg(argv, argc++, 1, arg);
		}
		while (sec.make_add) {
			insert_arg(argv, argc++, 1, sec.make_add->arg);
			sec.make_add = sec.make_add->next;
		}
		break;
	case TYPE_CC:
	case TYPE_CPLUSPLUS:
		if (sec.ccache) {
			verbose(sec.verbose, "Prefixing ccache ", sec.ccache);
			insert_arg(argv, argc++, 0, sec.ccache);
		} else if (sec.distcc) {
			char *hosts;

			if (type == TYPE_CC)
				hosts = sec.distcc_hosts;
			else
				hosts = sec.distcplusplus_hosts;
			verbose(sec.verbose, "Setting DISTCC_HOSTS=", hosts);
			setenv("DISTCC_HOSTS", hosts, 1);
			verbose(sec.verbose, "Prefixing arg ", sec.distcc);
			insert_arg(argv, argc++, 0, sec.distcc);
		}

	case TYPE_LD:
	case TYPE_OTHER:
		break;
	}

	for (add = sec.env_add; add; add = add->next) {
		char *eq = strchr(add->arg, '=');
		if (!eq)
			fatal("Environment variable needs '=': ", 0, add->arg,
			      NULL);
		*eq = '\0';
		setenv(add->arg, eq+1, 1);
	}
}

#ifndef TESTING
static void __attribute__((noreturn)) run_command(bool verbose, char *argv[])
{
	if (verbose) {
		unsigned int i;
		write_string(STDERR_FILENO, "  ccontrol(");
		write_string(STDERR_FILENO, int_to_string(getpid()));
		write_string(STDERR_FILENO, ") execing ");
		for (i = 0; argv[i]; i++) {
			write_string(STDERR_FILENO, "'");
			write_string(STDERR_FILENO, argv[i]);
			write_string(STDERR_FILENO, "' ");
		}
		write_string(STDERR_FILENO, "\n");
	}
	execv(argv[0], argv);
	fatal("failed to exec '", errno, argv[0], "'", NULL);
}
#else
static void __attribute__((noreturn)) run_command(bool verbose, char *argv[])
{
	/* Print out all TESTING= env vars. */
	if (getenv("TESTING1"))
		printf("TESTING1='%s' ", getenv("TESTING1"));
	if (getenv("TESTING2"))
		printf("TESTING2='%s' ", getenv("TESTING2"));

	if (strcmp(getenv("FAILDISTCC") ?: "", "1") == 0)
		exit(103);

	if (getenv("DISTCC_HOSTS")) {
		printf("DISTCC_HOSTS='%s' ", getenv("DISTCC_HOSTS"));
	}

	while (argv[0]) {
		printf("%s ", argv[0]);
		argv++;
	}
	printf("\n");

	exit(atoi(getenv("EXITCODE") ?: "0"));
}
#endif

/* Fork command. */
static void fork_command(bool verbose, char *argv[])
{
	int pid = fork();

	if (pid < 0)
		fatal("failed to fork", errno, NULL);
	if (pid == 0)
		run_command(verbose, argv);
	verbose(verbose, "Forked ", int_to_string(pid));
}

/* Wait for forked command */
static int wait_for_child(bool verbose)
{
	int status;

	wait(&status);
	status = WIFEXITED(status) ? WEXITSTATUS(status) : 255;
	verbose(verbose, "Child returned ", int_to_string(status));
	return status;
}

static bool file_altered(const char *configname, const struct stat *st)
{
	struct stat st2;

	stat(configname, &st2);
	return st2.st_ctime != st->st_ctime || st2.st_mtime != st->st_mtime
		|| st2.st_ino != st->st_ino;
}

static void write_item(const char *a, const char *b)
{
	if (b) {
		write_string(STDOUT_FILENO, "\t");
		write_string(STDOUT_FILENO, a); write_string(STDOUT_FILENO, b);
		write_string(STDOUT_FILENO, "\n");
	}
}

/* This list is backwards.  Print it forwards. */
static void print_add(const char *str, struct add *add)
{
	if (!add)
		return;
	print_add(str, add->next);
	write_item(str, add->arg);
}

static void print_section(const char *dir, struct section sec)
{
	write_string(STDOUT_FILENO, "[");
	write_string(STDOUT_FILENO, dir);
	write_string(STDOUT_FILENO, "]\n");
	write_item("cc = ", sec.names[TYPE_CC]);
	write_item("c++ = ", sec.names[TYPE_CPLUSPLUS]);
	write_item("make = ", sec.names[TYPE_MAKE]);
	write_item("ld = ", sec.names[TYPE_LD]);
	write_item("ccache = ", sec.ccache);
	if (sec.distcc && (sec.distcc_hosts || sec.distcplusplus_hosts)) {
		write_item("distcc = ", sec.distcc);
		write_item("distcc-hosts = ", sec.distcc_hosts);
		if (sec.distcplusplus_hosts_set)
			write_item("distc++-hosts = ",sec.distcplusplus_hosts);
	}
	print_add("add make = ", sec.make_add);
	print_add("add env = ", sec.env_add);
	if (sec.no_parallel && streq(sec.no_parallel, ""))
		write_item("no-parallel", "");
	else
		write_item("no-parallel = ", sec.no_parallel);
	if (sec.cpus != 1)
		write_item("cpus = ", int_to_string(sec.cpus));
	if (sec.verbose)
		write_item("verbose", "");
	if (sec.nice != 10)
		write_item("nice = ", int_to_string(sec.nice));
	write_item("lock-file = ", sec.lock_file);
}

static bool is_preprocess(char *argv[])
{
	char **arg;
	for (arg = argv; *arg; arg++) {
		if (streq(*arg, "-E"))
			return true;
	}
	return false;
}

int main(int orig_argc, char *orig_argv[])
{
	enum type type;
	struct section sec;
	int fd, ret, argc;
	bool nodistcc = false, noccache = false, ccache_then_distcc = false;
	struct stat st;
	char configname[PATH_MAX];
	char dirname[PATH_MAX];
	char *new_argv[orig_argc + 23], **argv;
	undofn_t undo;

	/* Have we already done ccache? */
	if (getenv("CCONTROL_LOCK_FD")) {
		/* Ccache calls us once to pre-process. */
		if (is_preprocess(orig_argv+1))
			run_command(false, orig_argv+1);
		/* Write to fd to make grandparent (ccontrol) drop lock */
		if (write(atoi(getenv("CCONTROL_LOCK_FD")), "#", 1) != 1)
			fatal("Could not write to ccontrol lock fd",
			      errno, NULL);
		unsetenv("CCACHE_PREFIX");
		unsetenv("CCONTROL_LOCK_FD");
		noccache = true;
	}

	strcpy(configname, getenv("HOME"));
	strcat(configname, "/.ccontrol/config");

again_restore_args:
	/* Make room to add args. */
	argc = orig_argc;
	memcpy(new_argv, orig_argv, (argc + 1)*sizeof(argv[0]));
	argv = new_argv;

#ifdef TESTING
	if (getenv("ARGV0"))
		argv[0] = getenv("ARGV0");
#endif
	getcwd(dirname, sizeof(dirname));

	if (strends(argv[0], "ccontrol")) {
		if (argv[1] && strstarts(argv[1], "--section=")) {
			strcpy(dirname, argv[1] + strlen("--section="));
			argv++;
			argc--;
		}
		if (argv[1] && (streq(argv[1], "--version")
				|| streq(argv[1], "-V"))) {
			
			__verbose("version " VERSION, NULL);
			exit(0);
		}
		if (!argv[1]) {
			print_section(dirname,
				      read_config(configname, dirname,
						  open(configname, O_RDWR)));
			exit(0);
		}
		argv++;
		argc--;
	}
	type = what_am_i(argv);

again:
	/* Since we later grab an exclusive lock on this, must be writable */
	fd = open(configname, O_RDWR);

	/* This handles open failure if fd < 0. */
	sec = read_config(configname, dirname, fd);

    /* Ensure we exec the right command */
    {
        char *cmd = resolve_path(sec.names[type], argv[0]);
        free(sec.names[type]);
        sec.names[type] = cmd;
    }

	fstat(fd, &st);

	/* Run low priority; people like to use apps while compiling. */
	setpriority(PRIO_PROCESS, 0, sec.nice);

	if (noccache) {
		verbose(sec.verbose, "ccache suppressed this time");
		sec.ccache = NULL;
	}

	/* Check we really distcc if asked to. */
	if (sec.distcc) {
		if (nodistcc || type == TYPE_LD || type == TYPE_MAKE)
			sec.distcc = NULL;
		else if (sec.ccache) {
			/* Don't put this in command line, but see below */
			sec.distcc = NULL;
			ccache_then_distcc = true;
		} else {
			if (type == TYPE_CC && !sec.distcc_hosts)
				sec.distcc = NULL;
			else if (type == TYPE_CPLUSPLUS
				 && !sec.distcplusplus_hosts)
				sec.distcc = NULL;
			else if (!can_distcc(argv)) {
				verbose(sec.verbose, "Cannot distcc this");
				sec.distcc = NULL;
			}
		}
	}

	/* Grab lock returns undofn if it slept: restat config file.  This
	 * decreases latency when config file changed. */
	undo = grab_lock(sec.lock_fd, sec, type);
	if (undo && file_altered(configname, &st)) {
		/* If we had distcc disabled, try again. */
		nodistcc = 0;
		close(sec.lock_fd);
		undo();
		goto again;
	}

	adjust_args_and_environment(sec, type, argv, argc);

	if (ccache_then_distcc) {
		/* We need to hold slow lock until either ccache pulls
		   from cache and exits, OR ccontrol is invoked from
		   ccache (cache miss).
		 */
		char c;
		int fd[2];

		if (pipe(fd) != 0)
			fatal("Failed to create pipe", errno, NULL);
		verbose(sec.verbose, "Setting CCACHE_PREFIX=ccontrol");
		setenv("CCONTROL_LOCK_FD", int_to_string(fd[1]), 1);
		setenv("CCACHE_PREFIX", "ccontrol", 1);
		fork_command(sec.verbose, argv);
		close(fd[1]);
		/* This will return when child exits, OR writes manually. */
		if (read(fd[0], &c, 1) == 1)
			verbose(sec.verbose, "Grandchild said to drop lock");
		else
			verbose(sec.verbose, "I guess ccache exited");
		undo();
		exit(wait_for_child(sec.verbose));
	}

	if (!sec.distcc)
		run_command(sec.verbose, argv);

	/* We have to wait, to make sure distcc actually distributes,
	 * otherwise we end up running 20 compiles locally. */
	setenv("DISTCC_FALLBACK", "0", 1);

	fork_command(sec.verbose, argv);
	ret = wait_for_child(sec.verbose);
	if (ret == 103 || ret == 116 || ret == 107) {
		/* distcc failed to distribute: run single-thread. */
		verbose(sec.verbose, "distcc failed, retrying without");
		close(sec.lock_fd);
		undo();
		nodistcc = 1;
		unsetenv("CCACHE_PREFIX");
#ifdef TESTING
		unsetenv("FAILDISTCC");
#endif
		while (sec.env_add) {
			unsetenv(sec.env_add->arg);
			sec.env_add = sec.env_add->next;
		}
		goto again_restore_args;
	}
	return ret;
}
