#include "ccontrol.h"
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stdrusty.h"

enum token_type {
	CC = TYPE_CC,
	CPLUSPLUS = TYPE_CPLUSPLUS,
	LD = TYPE_LD,
	MAKE = TYPE_MAKE,
	NONE,
	INDENT,
	NO_PARALLEL,
	CCACHE,
	DISTCC,
	DISTCC_HOSTS,
	DISTCPLUSPLUS_HOSTS,
	LEFT_BRACKET,
	RIGHT_BRACKET,
	EQUALS,
	NUMBER,
	STRING,
	VERBOSE,
	NICE,
	INCLUDE,
	CPUS,
	DISABLE,
	ADD,
	ENV,
	LOCK_FILE,
	END,
};

#include "keywords.c"

struct token
{
	enum token_type type;
	struct string string;
};

static unsigned int slen(struct string string)
{
	return string.end - string.start;
}

/* Length up to and including char. 0 == not found.  */
static unsigned int upto(struct string data, char c)
{
	char *end;

	end = memchr(data.start, c, slen(data));
	if (end)
		return end - data.start + 1;
	return 0;
}

static void __attribute__((noreturn))
parse_error(struct token tok, const char *str)
{
	unsigned int line = 1;
	char *input_head = tok.string.start_of_file;

	/* Count what line we're on. */
	while (input_head < tok.string.start) {
		if (*input_head == '\n')
			line++;
		input_head++;
	}
	fatal("parse error on line ", 0,
	      int_to_string(line), ": expected ", str, NULL);
}

static bool swallow_whitspace(struct string *data)
{
	bool ret = false;

	while (data->start[0] == ' ' || data->start[0] == '\t') {
		ret = true;
		data->start++;
		if (data->start == data->end)
			break;
	}
	return ret;
}

static struct string get_string(struct string *data)
{
	struct string str;
	str.start_of_file = data->start_of_file;
	str.start = data->start;
	while (data->start != data->end
	       && *data->start != '='
	       && *data->start != '['
	       && *data->start != ']'
	       && *data->start != ' '
	       && *data->start != '\n'
	       && *data->start != '\t')
		data->start++;
	str.end = data->start;
	return str;
}

/* Token takes one character. */
static struct token single_digit_token(struct string data,enum token_type type)
{
	return ((struct token) { type,
			{ data.start, data.start+1, data.start_of_file } });
}

static struct token peek_token(struct string data)
{
	struct token tok;
	unsigned int num;
	bool new_line = (data.start == data.start_of_file);

	/* We might need to skip over comments. */
	while (data.start != data.end) {
		const struct ccontrol_command *cmd;
		char *start;

		if (data.start[0] == '\n') {
			new_line = true;
			data.start++;
			continue;
		}

		/* Whitespace: only relevant if after nl, followed by stuff. */
		start = data.start;
		if (swallow_whitspace(&data) && new_line) {
			if (slen(data)) {
				if (data.start[0] == '\n' || data.start[0] == '#')
					continue;

				tok.type = INDENT;
				tok.string.start_of_file = data.start_of_file;
				tok.string.start = start;
				tok.string.end = data.start;
				return tok;
			}
		}

		/* Comment?  Ignore to end of line. */
		if (data.start[0] == '#') {
			num = upto(data, '\n');
			if (!num)
				break;
			data.start += num;
			new_line = true;
			continue;
		}

		if (data.start[0] == '[')
			return single_digit_token(data, LEFT_BRACKET);

		if (data.start[0] == ']')
			return single_digit_token(data, RIGHT_BRACKET);

		if (data.start[0] == '=')
			return single_digit_token(data, EQUALS);

		tok.string = get_string(&data);
		cmd = find_keyword(tok.string.start, slen(tok.string));
		if (cmd)
			tok.type = cmd->type;
		else {
			/* Number or string? */
			unsigned int i;

			for (i = 0; i < slen(tok.string); i++)
				if (!(isdigit(tok.string.start[i]) ||
				      tok.string.start[i] == '-'))
					break;
			if (i > 0 && i == slen(tok.string))
				tok.type = NUMBER;
			else
				tok.type = STRING;
		}
		return tok;
	}

	tok.type = END;
	tok.string = data;
	return tok;
}

static void swallow_token(struct string *data, struct token tok)
{
	data->start = tok.string.end;
}

static struct token get_token(struct string *data)
{
	struct token tok = peek_token(*data);
	swallow_token(data, tok);
	return tok;
}

/* Optional = <token>. */
static struct token get_value(struct string *data)
{
	struct token tok;

	tok = peek_token(*data);
	if (tok.type == EQUALS) {
		swallow_token(data, tok);
		return get_token(data);
	}
	tok.type = NONE;
	tok.string.start = data->start;
	tok.string.end = data->start;
	return tok;
}

static int to_int(struct token tok, int min, int max)
{
	int num;
	if (tok.type != NUMBER)
		parse_error(tok, "'= some-number'");

	num = atoi(tok.string.start);
	if (num < min || num > max)
		parse_error(tok, "'= number in valid range'");
	return num;
}

/**
 * Given a path and command name, returns the name of the file that
 * should be 'exec'ed. If the path is a directory, the command name
 * is appended. If it is an executable file, that is returned. If
 * the path is not absolute, it is returned as-is.
 *
 * All return values are 'malloc'ed strings.
 * On error, a copy of the configured_path is returned.
 */
char *resolve_path(const char *configured_path, const char *cmdname)
{
    struct stat st;
    const char *basename;
    char *execcmd;

    if (configured_path[0] != '/'
            || stat(configured_path, &st) == -1
            || !S_ISDIR(st.st_mode))
        return strdup(configured_path);

    basename = strrchr(cmdname, '/');
    if (basename)
        ++basename;
    else
        basename = cmdname;

    execcmd = malloc(strlen(configured_path) + strlen(basename) + 2);
    if (!execcmd)
        fatal("Cannot allocate memory for command", errno);

    /* +1 to include the NUL byte */
    memcpy(execcmd, configured_path, strlen(configured_path) + 1);
    strcat(execcmd, "/");
    strcat(execcmd, basename);

    return execcmd;
}

/* '=' <path> */
static char *get_path(struct string *data)
{
	char *p;
	const char *prefix = "";
	struct token tok;

	tok = get_value(data);
	if (tok.type != STRING)
		parse_error(tok, "'= some-path'");

	if (tok.string.start[0] == '~') {
		prefix = getenv("HOME") ?: "";
		tok.string.start++;
	}

	p = new_array(char, strlen(prefix) + slen(tok.string) + 1);
	memcpy(p, prefix, strlen(prefix));
	memcpy(p + strlen(prefix), tok.string.start, slen(tok.string));
	p[strlen(prefix)+slen(tok.string)] = '\0';

	return p;
}

static char *get_optional_path(struct string *data)
{
	struct token tok;

	tok = peek_token(*data);
	if (tok.type == DISABLE) {
		swallow_token(data, tok);
		return NULL;
	}

	return get_path(data);
}

/* 'disable' or '=' <values...> */
static char *get_to_eol(struct string *data, const char *expect)
{
	char *p;
	unsigned int len;
	struct token tok;

	tok = get_token(data);
	if (tok.type == DISABLE)
		return NULL;
	if (tok.type != EQUALS)
		parse_error(tok, expect);

	swallow_whitspace(data);
	len = upto(*data, '\n');
	if (!len) {
		struct token tok;

		tok.type = END;
		tok.string = *data;
		parse_error(tok, "something");
	}

	/* Turn \n into \0. */
	p = malloc(len);
	memcpy(p, data->start, len);
	p[len-1] = '\0';

	/* Leave \n so we can detect indent. */
	data->start += len-1;

	return p;
}

static struct add *new_add(char *arg, struct add *next)
{
	struct add *add = malloc(sizeof(struct add));
	add->arg = arg;
	add->next = next;
	return add;
}

static void read_section_file(const char *configname, struct section *sec);

/* With thanks to Joseph Heller. */
static void read_section_section(struct string *data, struct section *sec)
{
	struct token tok;
	struct add **add;
	char *p;

	while ((tok = peek_token(*data)).type == INDENT) {
		swallow_token(data, tok);
		tok = get_token(data);
		/* Lines are of form "x" or "x = value". */
		switch (tok.type) {
		case NO_PARALLEL:
			sec->no_parallel = get_to_eol(data, "= targets");
			/* In case parent set it, and we're disabling it. */
			if (!sec->no_parallel)
				unsetenv("CCONTROL_NO_PARALLEL");
			break;
		case CPUS:
			sec->cpus = to_int(get_value(data), 1, 1000000);
			break;
		case CCACHE:
			sec->ccache = get_optional_path(data);
			break;
		case DISTCC:
			sec->distcc = get_optional_path(data);
			break;
		case DISTCC_HOSTS:
			sec->distcc_hosts = get_to_eol(data, "= some-hosts");
			if (!sec->distcplusplus_hosts_set)
				sec->distcplusplus_hosts = sec->distcc_hosts;
			break;
		case DISTCPLUSPLUS_HOSTS:
			sec->distcplusplus_hosts = get_to_eol(data,
							      "= some-hosts");
			sec->distcplusplus_hosts_set = true;
			break;
		case VERBOSE:
			sec->verbose = true;
			break;
		case NICE:
			sec->nice = to_int(get_value(data), -19, 20);
			break;
		case CC:
		case CPLUSPLUS:
		case LD:
		case MAKE:
			sec->names[tok.type] = get_path(data);
			break;
		case INCLUDE:
			read_section_file(get_path(data), sec);
			break;
		case ADD:
			tok = get_token(data);
			switch (tok.type) {
			case MAKE:
				add = &sec->make_add;
				break;
			case ENV:
				add = &sec->env_add;
				break;
			default:
				parse_error(tok, "make or env");
			}
			p = get_to_eol(data, "= argument");
			if (!p)
				*add = NULL;
			else 
				*add = new_add(p, *add);
			break;
		case LOCK_FILE:
			sec->lock_file = get_path(data);
			sec->lock_fd = open(sec->lock_file, O_RDWR|O_CREAT, 
					    0600);
			if (sec->lock_fd < 0)
				fatal("could not open lock file: ", errno, 
				      sec->lock_file, NULL);
			break;
		default:
			parse_error(tok, "some instruction");
		}
	}
}

static void read_section_file(const char *configname, struct section *sec)
{
	unsigned long len;
	struct string data;

	data.start_of_file = suck_file(open(configname, O_RDONLY), &len);
	if (!data.start_of_file)
		fatal("reading included file ", errno, configname, NULL);
	data.start = data.start_of_file;
	data.end = data.start_of_file + len;

	read_section_section(&data, sec);
}

static bool read_section(struct string *data, struct section *sec)
{
	struct token tok = get_token(data);

	if (tok.type == END)
		return false;

	if (tok.type != LEFT_BRACKET)
		parse_error(tok, "'[' to start new section");

	tok = get_token(data);
	if (tok.type != STRING)
		parse_error(tok, "path after '[' in section start");
	sec->name = tok.string;
	
	tok = get_token(data);
	if (tok.type != RIGHT_BRACKET)
		parse_error(tok, "']' after name in section start");

	read_section_section(data, sec);

	return true;
}

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool section_matches(const char *dirname, struct section sec)
{
	unsigned int len = slen(sec.name);
	char pattern[len + 2];

	/* Default always matches. */
	if (len == 0)
		return true;

	memcpy(pattern, sec.name.start, len);
	/* Append a * if not there already. */
	if (pattern[len-1] == '*')
		pattern[len] = '\0';
	else {
		pattern[len] = '*';
		pattern[len+1] = '\0';
	}
	return (fnmatch(pattern, dirname, 0) == 0);
}

static struct section get_default(int fd)
{
	struct section def = { .cpus = 1,
			       .nice = 10,
			       .distcplusplus_hosts_set = false };
	def.lock_fd = fd;
	return def;
}

static void read_config_file(const char *configname, int fd,
			     const char *dirname,
			     struct section *result)
{
	unsigned long len;
	struct string data;

	data.start_of_file = suck_file(fd, &len);
	if (!data.start_of_file)
		fatal("reading ", errno, configname, NULL);
	data.start = data.start_of_file;
	data.end = data.start_of_file + len;

	/* Trivial parser. */
	for (;;) {
		struct section sec = *result;
		struct token tok;

		tok = peek_token(data);
		if (tok.type == INCLUDE) {
			char *included;
			int incfd;
			swallow_token(&data, tok);

			included = get_path(&data);
			incfd = open(included, O_RDONLY);
			read_config_file(included, incfd, dirname, result);
			close(incfd);
			continue;
		}

		if (!read_section(&data, &sec))
			break;

		if (section_matches(dirname, sec)) {
			if (sec.verbose) {
				unsigned int len = slen(sec.name);
				char str[len+1];
				memcpy(str, sec.name.start, len);
				str[len] = '\0';
				verbose(sec.verbose, "Using section ", str);
			}
			*result = sec;
		}
	}

    /* Resolve ccache */
    if (result->ccache) {
        char *rawccache = result->ccache;
        result->ccache = resolve_path(result->ccache, "ccache");
        free(rawccache);
    }

    /* Resolve distcc */
    if (result->distcc) {
        char *rawdistcc = result->distcc;
        result->distcc = resolve_path(result->distcc, "distcc");
        free(rawdistcc);
    }

	free(data.start_of_file);
}

struct section read_config(const char *configname, const char *dir, int fd)
{
	struct section result;

	result = get_default(fd);
	read_config_file(configname, fd, dir, &result);
	return result;
}
