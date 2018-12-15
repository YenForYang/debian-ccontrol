/* Simple code to grab a lock to restrict number of parallel processes. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ccontrol.h"
#include "stdrusty.h"

/* We place much looser limits on "fast" operations: distcc-ables and make */
#define DISTCC_LIMIT 20
/* Make is limited separately for each depth, since it can recurse. */
#define MAKE_LIMIT 3

#define IPC_KEY 0xCCD1ED

static void fcntl_lock(int fd, bool lock, unsigned int offset)
{
	struct flock fl;

	fl.l_type = lock ? F_WRLCK : F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = 1;

	if (fcntl(fd, F_SETLKW, &fl) < 0)
		fatal("cannot lock config file", errno, NULL);
}

/* To create an initialized semaphore, we need a lock.  Use fcntl lock. */
static int make_semaphore(int fd, struct section sec, int key)
{
	int id;

	fcntl_lock(fd, true, 0);

	id = semget(key, 1, IPC_CREAT|IPC_EXCL);
	if (id < 0) {
		int saved_errno = errno;
		/* We might have raced, try again. */
		id = semget(key, 1, 0);
		errno = saved_errno;
	} else {
		union semun {
			int val;
			struct semid_ds *buf;
			unsigned short *array;
			struct seminfo *__buf;
		} semctl_arg;
		struct semid_ds ds;

		verbose(sec.verbose, "Created slow lock");
		memset(&ds, 0, sizeof(ds));
		ds.sem_perm.uid = geteuid();
		ds.sem_perm.gid = getegid();
		ds.sem_perm.mode = 0600;
		semctl_arg.buf = &ds;
		
		if (semctl(id, 0, IPC_SET, semctl_arg) < 0)
			fatal("cannot set semaphore permissions",
			      errno, NULL);
		semctl_arg.val = sec.cpus;
		if (semctl(id, 0, SETVAL, semctl_arg) < 0)
			fatal("cannot set semaphore value", errno, NULL);
	}
	fcntl_lock(fd, false, 0);
	return id;
}

/* Semaphores give us exact control over rate, but SEM_UNDO space is
 * often limited (not on Linux tho AFAICT). */
static void grab_sem(int fd, struct section sec)
{
	struct sembuf sop;
	int id, key;

	key = IPC_KEY + geteuid();

	id = semget(key, 1, 0);
	if (id < 0 && errno == ENOENT)
		id = make_semaphore(fd, sec, key);
	if (id < 0)
		fatal("cannot get semaphore", errno, NULL);

again:
	sop.sem_num = 0;
	sop.sem_op = -1;
	sop.sem_flg = SEM_UNDO;

	if (semop(id, &sop, 1) != 0) {
		if (errno == EINTR)
			goto again;
		fatal("cannot decrement semaphore", errno, NULL);
	}
}

/* Since we have lots of these, we use fcntl locks as an approximate
 * means to limit them. */
static void grab_fcntl_lock(int fd, unsigned int base, unsigned int max)
{
	srand(getpid());

	fcntl_lock(fd, true, base + rand()%max);
}

static void set_lock_envvar(char depth)
{
	char locktype[2];
	locktype[0] = depth;
	locktype[1] = '\0';
	setenv("CCONTROL_LOCK", locktype, 1);
}

static void undo_decrement_envvar(void)
{
	char *depth = getenv("CCONTROL_LOCK");
	set_lock_envvar(depth[0] - 1);
}

void drop_slow_lock(void)
{
	struct sembuf sop;
	int id, key;

	assert(getenv("CCONTROL_LOCK"));
	assert(getenv("CCONTROL_LOCK")[0] == '0');

	key = IPC_KEY + geteuid();

	id = semget(key, 1, 0);
	if (id < 0)
		fatal("cannot re-get semaphore", errno, NULL);

	sop.sem_num = 0;
	sop.sem_op = 1;
	sop.sem_flg = SEM_UNDO;

	if (semop(id, &sop, 1) != 0)
		fatal("cannot increment semaphore", errno, NULL);

	unsetenv("CCONTROL_LOCK");
}

static void undo_unset_envvar(void)
{
	unsetenv("CCONTROL_LOCK");
}

static void undo_nothing(void)
{
}

undofn_t grab_lock(int fd, struct section sec, enum type type)
{
	char *lock = getenv("CCONTROL_LOCK");
	unsigned int distcc_lim, make_off, make_lim;

	verbose(sec.verbose, "Grabbing lock for ",
		type == TYPE_CC ? "CC"
		: type == TYPE_CPLUSPLUS ? "C++"
		: type == TYPE_MAKE ? "MAKE"
		: type == TYPE_LD ? "LD" : "UNKNOWN");

	/* If we already have slow lock, don't grab again (gcc calls ld). */
	if (lock && lock[0] == '0') {
		verbose(sec.verbose, "Already got it");
		return undo_nothing;
	}

	/* Position 0 is used to initialize slow semaphore.
	 * Next range is used by distcc-able builds.
	 * Then a series of ranges for each makefile depth. */
	make_off = distcc_lim = sec.cpus*DISTCC_LIMIT;
	make_lim = sec.cpus*MAKE_LIMIT;

	/* Make can run in parallel. */
	if (type == TYPE_MAKE) {
		unsigned int depth = 0;

		/* Each level of make limited separately (tends to recurse) */
		if (lock)
			depth = lock[0] - 'A' + 1;
		verbose(sec.verbose, "Getting fast lock for make");

		/* No locks for top-level make. */
		if (lock)
			grab_fcntl_lock(fd, 1 + make_off + depth * make_lim,
					make_lim);
		set_lock_envvar('A' + depth);
		return undo_decrement_envvar;
	} else if (!sec.distcc) {
		verbose(sec.verbose, "Getting slow lock for non-distcc");
		/* LD or non-distcc compile.  Single file! */
		grab_sem(fd, sec);
		set_lock_envvar('0');
		return drop_slow_lock;
	} else {
		/* gcc & g++ are limited together. */
		if (lock && lock[0] == '1')
			fatal("called myself?", 0, NULL);
		verbose(sec.verbose, "Getting fast lock for compile");
		grab_fcntl_lock(fd, 1, distcc_lim);
		set_lock_envvar('1');
		return undo_unset_envvar;
	}
}
