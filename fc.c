#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <termios.h>

#define p printf
#define die(s) { fprintf(stderr, s); abort(); }

char *readfile(char *path, long *n)
{
	int fd = open(path, O_RDONLY);
	long nr, sz = 10000;
	char *buf = malloc(sz);
	*n = 0;
	while ((nr = read(fd, buf + *n, sz - *n)) > 0) {
		if (*n + nr >= sz) {
			sz *= 2;
			*n += nr;
			buf = realloc(buf, sz);
		} else
			*n += nr;
	}
	if (nr)
		return NULL;
	return buf;
}

static long getmtime(char *path)
{
	struct stat st;
	if (!stat(path, &st))
		return st.st_mtime;
	return -1;
}

char *xgetenv(char **q)
{
	char *r = NULL;
	while (*q && !r) {
		if (**q == '$')
			r = getenv(*q+1);
		else
			return *q;
		q += 1;
	}
	return r;
}

void spawn(char **arg) {
	pid_t pid;
	if ((pid = fork()) == -1) {
		die("fork() failed\n")
	} else if (pid == 0) {
		execvp(arg[0], arg);
		die("execvp() failed\n")
	} else
		waitpid(pid, NULL, 0);
}

static char* ed[] = {"$EDITOR", "$FCEDIT", "ed", "vi", NULL};
static char* hist[] = {"$HISTFILE", "/root/.ash_history", NULL};

#define write_tmp(from, size) \
int fd = open("/tmp/fcbuf", O_WRONLY | O_CREAT | O_TRUNC, 0600); \
if (fd < 0) \
	die("failed to open/create /tmp/fcbuf, check permissions.\n") \
if (write(fd, &f[from], size) < 0) \
	die("failed to write to /tmp/fcbuf\n") \
free(f); \

int main(int argc, char *argv[])
{
	char *f;
	long filesz, filesz1;
	int i1, i2, lflag = 0, nflag = 1;
	long mtime;
	if (argc == 1) {
		if (open("/tmp/fcbuf", O_WRONLY | O_CREAT | O_TRUNC, 0600) < 0)
			die("failed to open/create /tmp/fcbuf, check permissions.\n")
		edit:
		mtime = getmtime("/tmp/fcbuf");
		spawn((char *[]){xgetenv(ed), "/tmp/fcbuf", NULL});
		f = readfile("/tmp/fcbuf", &filesz);
		if (f && getmtime("/tmp/fcbuf") != mtime) {
			struct termios termios;
			tcgetattr(0, &termios);
			termios.c_lflag &= ~ECHO;
			tcsetattr(0, TCSAFLUSH, &termios);
			int fd = open(ctermid(NULL), O_WRONLY);
			char *fend = f + filesz;
			while (f != fend)
				ioctl(fd, TIOCSTI, f++);
			termios.c_lflag |= ECHO;
			tcsetattr(0, 0, &termios);
		}
		return 0;
	}
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			i1 = atoi(&argv[i][0]);
			f = readfile(xgetenv(hist), &filesz);
			if (!f)
				die("readfile failed\n")
			if (!filesz)
				continue;
			if (i == argc-1) {
				while (filesz && f[--filesz] == '\n');
				/* skip last entered command, which would be this one */
				while (filesz && f[filesz--] != '\n'); /* ???   ??? */
				filesz1 = filesz;
				for (int i = 0; i < i1 && filesz; i++)
					while (filesz && f[filesz--] != '\n');
				if (lflag) {
					if (nflag) {
						int lc = 0;
						for (int i = 0; i < filesz; i++)
							if (f[i] == '\n')
								lc++;
						int c;
						for (int i = lc, z = filesz+2; i < lc + i1; i++) {
							c = strchr(&f[z], '\n') - &f[z] + 1;
							p("%d %.*s", i, c, &f[z]);
							z += c;
						}
					} else
						p("%.*s", (int)(filesz1 - filesz), 
								&f[filesz ? filesz+2 : filesz]);
					continue;
				}
				write_tmp(filesz ? filesz+2 : filesz, filesz1 - filesz)
				goto edit;
			}
			if (argv[++i][0] != '-') {
				i2 = atoi(&argv[i][0]);
				int c = 0, x = 0, z;
				for (z = 0; c < filesz; c++)
					if (f[c] == '\n' && z++ == i1)
						break;
				while (c && f[--c] != '\n');
				c += c ? 1 : c;
				x = c;
				for (z = i1; c < filesz; c++)
					if (f[c] == '\n' && z++ == i2)
						break;
				if (lflag) {
					if (nflag) {
						for (int i = x; i < c; i1++) {
							z = strchr(&f[i], '\n') - &f[i] + 1;
							p("%d %.*s", i1, z, &f[i]);
							i += z;
						}
					} else
						p("%.*s", c - x, &f[x]);
					continue;
				}
				write_tmp(x, c - x)
				goto edit;
			}
			continue;
		}
		if (argv[i][1] == 'h')
			p("Usage: %s %s", argv[0], "[options] range\n"
				"  20\tedit last x commands from history\n"
				"  0 20\tedit lines 0-20 from history\n"
				"Options:\n"
				"  -e vi\t\tspecify editor\n"
				"  -d out\tspecify history file\n"
				"  -l 0 20\tlist lines 0-20 from history\n"
				"  -l 20\t\tlist last 20 commands from history\n"
				"  -n \t\tsuppress command numbers when listing with -l\n");
		else if (argv[i][1] == 'e')
			ed[0] = &argv[++i][0];
		else if (argv[i][1] == 'd')
			hist[0] = &argv[++i][0];
		else if (argv[i][1] == 'l')
			lflag = 1;
		else if (argv[i][1] == 'n')
			nflag = 0;
		else
			die("unknown option\n")
	}
	return 0;
}
