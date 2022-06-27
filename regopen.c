#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *argv0;
char *shell = "/bin/sh";

void defconfig(char buf[PATH_MAX]);
char *homedir(void);
int matches(char *item, char *regex);
void parse(FILE *f, char *pattern, int child);
void toshell(FILE *src, char *firstline, int spawnchild);
void usage(void);
void writecmds(FILE *src, int dest, char *firstline);

#ifndef __OpenBSD__
 #define pledge(x, y) (0)
 #define unveil(x, y) (0)
#endif /* __OpenBSD__ */

void
usage(void)
{
	fprintf(stderr, "usage: %s [-c] [-f configfile] item\n", argv0);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char config[PATH_MAX];
	FILE *f;
	int ch;
	int spawnchild = 0;

	if(pledge("exec stdio proc rpath unveil", NULL) == -1)
		err(1, "pledge");
	argv0 = basename(argv[0]);
	config[0] = '\0';
	while((ch = getopt(argc, argv, "cf:")) != -1) {
		switch(ch) {
		case 'c':
			spawnchild = 1;
			break;
		case 'f':
			if(strlen(optarg) >= PATH_MAX)
				errx(1, "config path is too long");
			strcpy(config, optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if(argc != 1)
		usage();
	if(config[0] == '\0')
		defconfig(config);
	if(unveil(config, "r") == -1)
		err(1, "unveil");
	if(unveil(shell, "x") == -1)
		err(1, "unveil");
	if(unveil(NULL, NULL) == -1)
		err(1, "unveil");
	if((f = fopen(config, "r")) == NULL)
		err(1, "could not open config file");
	if(pledge("exec stdio proc", NULL) == -1)
		err(1, "pledge");
	parse(f, argv[0], spawnchild);
}

void
defconfig(char buf[PATH_MAX])
{
	int ret;

	ret = snprintf(buf, PATH_MAX, "%s/%s", homedir(), ".regopen");
	if (ret < 0 || (size_t)ret >= PATH_MAX)
		errx(1, "config path is too long");
}

char *
homedir(void)
{
	char *hd;

	if((hd = getenv("HOME")) != NULL)
		return hd;
	errx(1, "could not get home directory");
}

void
parse(FILE *f, char *item, int child)
{
	enum State {
		WAITMATCH,
		WAITPROG
	};
	enum State state;
	char *line = NULL;
	size_t linesize = 0;
	size_t linelen;

	state = WAITMATCH;
	/* get first matching string */
	for(;;) {
		linelen = getline(&line, &linesize, f);
		if(linelen == -1) {
			if(errno)
				err(1, "error reading config");
			else
				exit(2);	/* no matching rule/action */
		}
		line[linelen - 1] = '\0';
		if(line[0] == '#')
			continue;
		if(state == WAITMATCH) {
			if(line[0] == '\0' || line[0] == '\t')
				continue;
			if(matches(item, line)) {
				state = WAITPROG;
				continue;
			}
		} else {
			if(line[0] == '\t') {
				setenv("item", item, 1);
				toshell(f, line, child);
			}
		}
	}
	/* NOT REACHED */
}

int
matches(char *item, char *pattern)
{
	regex_t re;
	regmatch_t match;
	int fail;

	fail = regcomp(&re, pattern, REG_EXTENDED);
	if(fail)
		errx(1, "regex compiling error");

	fail = regexec(&re, item, 1, &match, 0);
	if(fail && fail != REG_NOMATCH)
		errx(1, "regex matching error");

	regfree(&re);
	if(fail || match.rm_so != 0 || match.rm_eo != strlen(item))
		return 0;
	else
		return 1;
}

void
toshell(FILE *src, char *firstline, int spawnchild)
{
	int pfd[2];
	pid_t pid;

	if(pipe(pfd) != 0)
		errx(1, "pipe");
	pid = fork();
	if(pid == -1)
		err(1, "fork");
	if(spawnchild) {
		if(pid == 0)
			goto exec;
		else
			goto print;
	}
	else {
		if(pid == 0)
			goto print;
		else
			goto exec;
	}
exec:
	close(pfd[1]);
	dup2(pfd[0], 0);
	close(pfd[0]);
	execl(shell, shell, NULL);
	err(1, "exec");
print:
	if(pledge("stdio", NULL) == -1)
		err(1, "pledge");
	close(pfd[0]);
	writecmds(src, pfd[1], firstline);
	close(pfd[1]);
	exit(0);
}

void
writecmds(FILE *src, int dest, char *firstline)
{
	char *line = NULL;
	size_t linesize = 0;
	size_t linelen;

	write(dest, firstline, strlen(firstline));
	write(dest, "\n", 1);
	for(;;) {
		linelen = getline(&line, &linesize, src);
		if(line[0] == '#')
			continue;
		if(linelen == -1 || line[0] != '\t')
			break;
		write(dest, line, linelen);
	}
	free(firstline);
	free(line);
}
