/*
 * Serial port over Telnet client tool
 * Lubomir Rintel <lkundrak@v3.sk>
 * License: GPL
 */

#define _POSIX_C_SOURCE 201112L
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

int
main (int argc, char *argv[])
{
	/* The outbut buffer is twice as big because the IAC can be doubled. */
	unsigned char inbuf[512], outbuf[1024];
	int inbytes = 0, outbytes = 0;
	struct pollfd pfd[2];
	pid_t pid = 0;
	int status;
	int res;
	int i;

	if (argc < 3) {
		fprintf (stderr, "Usage: %s <host> <port> [<link>|--] <command> ...]\n", argv[0]);
		return 2;
	}

	pfd[0].fd = -1; /* Will be (re)opened on demand. */

	pfd[1].fd = open ("/dev/ptmx", O_RDWR);
	if (pfd[1].fd == -1) {
		perror ("ptmx");
		return 1;
	}
	grantpt (pfd[1].fd);
	unlockpt (pfd[1].fd);

	if (argc > 3) {
		if (strcmp (argv[3], "--") != 0) {
			/* We're making a link. */
			unlink (argv[3]);
			res = symlink (ptsname (pfd[1].fd), argv[3]);
			if (res == -1) {
				perror (argv[3]);
				return 1;
			}
		}
		if (argc > 4) {
			/* We're running a command. */
			char *args[argc];

			pid = fork ();
			if (pid == -1) {
				perror ("fork");
				return 1;
			}

			if (pid == 0) {
				for (i = 4; i < argc; i++) {
					if (strcmp (argv[i], "{}"))
						args[i - 4] = argv[i];
					else
						args[i - 4] = ptsname (pfd[1].fd);
				}
				args[i - 4] = NULL;

				res = execvp (args[0], args);
				perror (args[0]);
				return -1;
			}
		}
	} else {
		/* Just print the PTY name. */
		printf ("%s\n", ptsname (pfd[1].fd));
	}

	while (1) {
		if (pid) {
			/* We're running a command. */
			res = waitpid (pid, &status, pfd[1].fd == -1 ? 0 : WNOHANG);
			if (res == -1) {
				perror ("waitpid");
				return 1;
			}

			if (res == pid)
				return WEXITSTATUS (status);
		}

		/* Process the data from the telnet server.
		 * If just one character was consumed, then the other IAC has
		 * to be left verbatim. */
		while ((res = get_esc (inbuf, inbytes, NULL)) > 1) {
			inbytes -= res;
			memmove (inbuf, &inbuf[res], inbytes);
		};


		/* The telnet server side. Connect or reconnect to it. */
		if (pfd[0].fd == -1)
			pfd[0].fd = get_socket (argv[1], argv[2]);
		pfd[0].events = 0;
		if (inbytes < sizeof (inbuf) && outbytes == 0)
			pfd[0].events |= POLLIN;
		if (outbytes)
			pfd[0].events |= POLLOUT;
		pfd[0].revents = 0;

		/* PTY side. */
		pfd[1].events = 0;
		if (outbytes < sizeof (outbuf) / 2 && inbytes == 0)
			pfd[1].events |= POLLIN;
		if (inbytes >= 1 && inbuf[0] != IAC)
			pfd[1].events |= POLLOUT;
		if (inbytes >= 2 && inbuf[0] == IAC && inbuf[1] == IAC)
			pfd[1].events |= POLLOUT;
		pfd[1].revents = 0;

		/* Get the events. */
		res = poll (pfd, sizeof (pfd) / sizeof (pfd[0]), -1);
		if (res == -1) {
			perror ("poll");
			return -1;
		}

		/* Data from telnet server. */
		if (pfd[0].revents & POLLIN) {
			res = read (pfd[0].fd, &inbuf[inbytes], sizeof (inbuf) - inbytes);
			if (res > 0) {
				inbytes += res;
			} else {
				if (res == -1) {
					perror ("read");
					inbytes = 0;
				}
				close (pfd[0].fd);
				pfd[0].fd = -1;
			}
		}

		/* Data for the telnet server. */
		if (pfd[0].revents & POLLOUT) {
			res = write (pfd[0].fd, outbuf, outbytes);
			if (res > 0) {
				outbytes -= res;
				memmove (outbuf, &outbuf[res], outbytes);
			} else {
				if (res == -1)
					perror ("write");
				close (pfd[0].fd);
				pfd[0].fd = -1;
			}
		}

		/* Telnet has gone off. */
		if (pfd[0].revents & POLLHUP) {
			close (pfd[0].fd);
			pfd[0].fd = -1;
		}

		/* Data from pty. */
		if (pfd[1].revents & POLLIN) {
			res = read (pfd[1].fd, &outbuf[outbytes], sizeof (outbuf) / 2 - outbytes);
			if (res > 0) {
				for (i = outbytes; i < outbytes + res; i++) {
					/* If there's a IAC, double it. */
					if (outbuf[i] == IAC)
						memmove (&outbuf[i+1], &outbuf[i], res - i);
				}
				outbytes += res;
			} else {
				close (pfd[1].fd);
				if (res == -1) {
					perror ("read");
					outbytes = 0;
					return 1;
				}
				/* EOF on the pty. Can this ever happen? */
				return -1;
			}
		}

		/* Data to pty. */
		if (pfd[1].revents & POLLOUT) {
			/* Only up to a an IAC. */
			res = get_data (inbuf, inbytes);
			res = write (pfd[1].fd, inbuf, res);
			if (res > 0) {
				if (res == 1 && inbuf[0] == IAC && inbytes >= 2) {
					/* Wrote one (IAC), but remove two (IAC IAC). */
					res = 2;
				}
				inbytes -= res;
				memmove (inbuf, &inbuf[res], inbytes);
			} else {
				close (pfd[1].fd);
				pfd[1].fd = -1;
				if (res == -1) {
					perror ("write");
					return 1;
				}
				/* EOF on the pty. Can this ever happen? */
				return -1;
			}
		}

		/* The PTY has been hung up. */
		if (pfd[1].revents & POLLHUP) {
			if (pid) {
				/* From now on the waitpid will be blocking. */
				close (pfd[1].fd);
				pfd[1].fd = -1;
			} else {
				/* Allow other clients. */
				unlockpt (pfd[1].fd);
			}
		}
	}

	/* Not reached really. */
	return -1;
}
