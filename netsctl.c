/*
 * Serial port over Telnet control tool
 * Lubomir Rintel <lkundrak@v3.sk>
 * License: GPL
 */

#define _POSIX_C_SOURCE 201112L
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common.h"

struct {
	enum com_port_option option;
	int received;
	int requested;
	int control;
} options[] = {
	{ .option = SET_BAUDRATE },
	{ .option = SET_DATASIZE },
	{ .option = SET_PARITY },
	{ .option = SET_STOPSIZE },
	{ .option = SET_CONTROL, .control = CONTROL_REQ_FLOW	},
	{ .option = SET_CONTROL, .control = CONTROL_REQ_BREAK	},
	{ .option = SET_CONTROL, .control = CONTROL_REQ_DTR	},
	{ .option = SET_CONTROL, .control = CONTROL_REQ_RTS	},
	{ .option = SET_CONTROL, .control = CONTROL_REQ_FLOW_IN	},
};
int need_more;

static void
print_option (enum com_port_option option, union com_port_option_value *value)
{
	switch (option) {
	case SET_BAUDRATE:
		printf ("baudrate %d\n", value->baudrate);
		break;
	case SET_DATASIZE:
		printf ("datasize %d\n", value->datasize);
		break;
	case SET_PARITY:
		printf ("parity ");
		switch (value->parity) {
		case 1:
			printf ("NONE");
			break;
		case 2:
			printf ("ODD");
			break;
		case 3:
			printf ("EVEN");
			break;
		case 4:
			printf ("MARK");
			break;
		default:
			printf ("bad(%d)", value->parity);
			break;
		}
		printf ("\n");
		break;
	case SET_STOPSIZE:
		printf ("stopsize %d\n", value->stopsize);
		break;
	case SET_CONTROL:
		switch (value->control) {
		case 1:
			printf ("flow none\n");
			break;
		case 2:
			printf ("flow xonxoff\n");
			break;
		case 3:
			printf ("flow rtscts\n");
			break;
		case 5:
			printf ("break on\n");
			break;
		case 6:
			printf ("break off\n");
			break;
		case 8:
			printf ("dtr on\n");
			break;
		case 9:
			printf ("dtr off\n");
			break;
		case 11:
			printf ("rts on\n");
			break;
		case 12:
			printf ("rts off\n");
			break;
		case 14:
			printf ("flow_in none\n");
			break;
		case 15:
			printf ("flow_in xonxoff\n");
			break;
		case 16:
			printf ("flow_in rtscts\n");
			break;
		case 17:
			printf ("flow_in dcd\n");
			break;
		case 18:
			printf ("flow_in dtr\n");
			break;
		case 19:
			printf ("flow_in dsr\n");
			break;
		default:
			printf ("bad %d\n", value->control);
		}
		break;
	default:
		break;
	}
}

static void
got_option (enum com_port_option option, union com_port_option_value *value)
{
	int i;

	need_more = 0;
	for (i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
		if (options[i].option == option) {
			if (option != SET_CONTROL || (control_to_req (value->control) == options[i].control)) {
				if (options[i].requested && options[i].received == 0) {
					print_option (option, value);
					options[i].received = 1;
				}
			}
		}
		if (options[i].requested && !options[i].received)
			need_more++;
	}
}

int
main (int argc, char *argv[])
{
	unsigned char inbuf[128], outbuf[128];
	int inbytes = 0, outbytes = 0;
	struct pollfd pfd;
	/* -1 = do not care, 0 = request, non-zero = set */
	int baudrate = -1;
	int datasize = -1;
	int parity = -1;
	int stopsize = -1;
	int control_flow = -1;
	int control_break = -1;
	int control_dtr = -1;
	int control_rts = -1;
	int control_flow_in = -1;
	int res;
	int i;

	if (argc < 3 || argc % 2 == 0) {
		fprintf (stderr, "Usage: %s <host> <port> [<setting> <value> ...]\n", argv[0]);
		return 2;
	}

	for (i = 3; i < argc; i += 2) {
		if (argv[i + 1][0] == '\0')
			continue;
		if (strcmp (argv[i], "baudrate") == 0) {
			if (argv[i + 1][0] == '?') {
				baudrate = 0;
			} else if (argv[i + 1][0] >= '1' && argv[i + 1][0] <= '9') {
				baudrate = atoi (argv[i + 1]);
			} else {
				fprintf (stderr, "Bad baudrate: '%s'. Expected a non-zero number or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "datasize") == 0) {
			if (argv[i + 1][0] == '?') {
				datasize = 0;
			} else if (argv[i + 1][0] >= '1' && argv[i + 1][0] <= '9') {
				datasize = atoi (argv[i + 1]);
			} else {
				fprintf (stderr, "Bad datasize: '%s'. Expected a non-zero number or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "parity") == 0) {
			if (argv[i + 1][0] == '?') {
				parity = 0;
			} else if (argv[i + 1][0] >= '1' && argv[i + 1][0] <= '9') {
				parity = atoi (argv[i + 1]);
			} else if (strcasecmp (argv[i + 1], "NONE") == 0) {
				parity = 1;
			} else if (strcasecmp (argv[i + 1], "ODD") == 0) {
				parity = 2;
			} else if (strcasecmp (argv[i + 1], "EVEN") == 0) {
				parity = 3;
			} else if (strcasecmp (argv[i + 1], "MARK") == 0) {
				parity = 4;
			} else {
				fprintf (stderr, "Bad parity: '%s'. Expected 'NONE', 'ODD', 'EVEN', 'MARK' or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "stopsize") == 0) {
			if (argv[i + 1][0] == '?') {
				stopsize = 0;
			} else if (argv[i + 1][0] >= '1' && argv[i + 1][0] <= '9') {
				stopsize = atoi (argv[i + 1]);
			} else {
				fprintf (stderr, "Bad stopsize: '%s'. Expected a non-zero number or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "flow") == 0) {
			if (argv[i + 1][0] == '?') {
				control_flow = 0;
			} else if (strcmp (argv[i + 1], "none") == 0) {
				control_flow = 1;
			} else if (strcmp (argv[i + 1], "xonxoff") == 0) {
				control_flow = 2;
			} else if (strcmp (argv[i + 1], "rtscts") == 0) {
				control_flow = 3;
			} else {
				fprintf (stderr, "Bad flow: '%s'. Expected 'none', 'xonxoff', 'rtscts' or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "break") == 0) {
			if (argv[i + 1][0] == '?') {
				control_break = 0;
			} else if (strcmp (argv[i + 1], "on") == 0) {
				control_break = 5;
			} else if (strcmp (argv[i + 1], "off") == 0) {
				control_break = 6;
			} else {
				fprintf (stderr, "Bad break: '%s'. Expected 'on', 'off' or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "dtr") == 0) {
			if (argv[i + 1][0] == '?') {
				control_dtr = 0;
			} else if (strcmp (argv[i + 1], "on") == 0) {
				control_dtr = 8;
			} else if (strcmp (argv[i + 1], "off") == 0) {
				control_dtr = 9;
			} else {
				fprintf (stderr, "Bad dtr: '%s'. Expected 'on', 'off' or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "rts") == 0) {
			if (argv[i + 1][0] == '?') {
				control_rts = 0;
			} else if (strcmp (argv[i + 1], "on") == 0) {
				control_rts = 11;
			} else if (strcmp (argv[i + 1], "off") == 0) {
				control_rts = 12;
			} else {
				fprintf (stderr, "Bad rts: '%s'. Expected 'on', 'off' or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else if (strcmp (argv[i], "flow_in") == 0) {
			if (argv[i + 1][0] == '?') {
				control_flow_in = 0;
			} else if (strcmp (argv[i + 1], "none") == 0) {
				control_flow_in = 14;
			} else if (strcmp (argv[i + 1], "xonxoff") == 0) {
				control_flow_in = 15;
			} else if (strcmp (argv[i + 1], "rtscts") == 0) {
				control_flow_in = 16;
			} else if (strcmp (argv[i + 1], "dtr") == 0) {
				control_flow_in = 18;
			} else if (strcmp (argv[i + 1], "dcd") == 0) {
				control_flow_in = 17;
			} else if (strcmp (argv[i + 1], "dsr") == 0) {
				control_flow_in = 19;
			} else {
				fprintf (stderr, "Bad flow_in: '%s'. Expected 'none', 'xonxoff', 'rtscts', 'dtr', 'dcd', 'dsr' or '?'.\n", argv[i + 1]);
				return 2;
			}
		} else {
			fprintf (stderr, "YOLO: [%s] [%s]\n", argv[i], argv[i + 1]);
			return 2;
		}
	}

	if (argc == 3) {
		/* Request all if no arguments. */
		baudrate = 0;
		datasize = 0;
		parity = 0;
		stopsize = 0;
		control_flow = 0;
		control_break = 0;
		control_dtr = 0;
		control_rts = 0;
		control_flow_in = 0;
	}

	pfd.fd = get_socket (argv[1], argv[2]);
	if (pfd.fd == -1)
		return 1;

	outbuf[outbytes++] = IAC;
	outbuf[outbytes++] = WILL;
	outbuf[outbytes++] = COM_PORT_OPTION;

	for (i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
		int control_val;

		switch (options[i].option) {
		case SET_BAUDRATE:
			if (baudrate == -1)
				continue;
			else if (baudrate == 0)
				options[i].requested = 1;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SB;
			outbuf[outbytes++] = COM_PORT_OPTION;
			outbuf[outbytes++] = SET_BAUDRATE;
			outbuf[outbytes++] = (baudrate >> 24) & 0xff;
			outbuf[outbytes++] = (baudrate >> 16) & 0xff;
			outbuf[outbytes++] = (baudrate >>  8) & 0xff;
			outbuf[outbytes++] = (baudrate >>  0) & 0xff;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SE;
			break;
		case SET_DATASIZE:
			if (datasize == -1)
				continue;
			else if (datasize == 0)
				options[i].requested = 1;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SB;
			outbuf[outbytes++] = COM_PORT_OPTION;
			outbuf[outbytes++] = SET_DATASIZE;
			outbuf[outbytes++] = datasize;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SE;
			break;
		case SET_PARITY:
			if (parity == -1)
				continue;
			else if (parity == 0)
				options[i].requested = 1;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SB;
			outbuf[outbytes++] = COM_PORT_OPTION;
			outbuf[outbytes++] = SET_PARITY;
			outbuf[outbytes++] = parity;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SE;
			break;
		case SET_STOPSIZE:
			if (stopsize == -1)
				continue;
			else if (stopsize == 0)
				options[i].requested = 1;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SB;
			outbuf[outbytes++] = COM_PORT_OPTION;
			outbuf[outbytes++] = SET_STOPSIZE;
			outbuf[outbytes++] = stopsize;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SE;
			break;
		case SET_CONTROL:
			switch (options[i].control) {
			case CONTROL_REQ_FLOW:
				control_val = control_flow;
				break;
			case CONTROL_REQ_BREAK:
				control_val = control_break;
				break;
			case CONTROL_REQ_DTR:
				control_val = control_dtr;
				break;
			case CONTROL_REQ_RTS:
				control_val = control_rts;
				break;
			case CONTROL_REQ_FLOW_IN:
				control_val = control_flow_in;
				break;
			default:
				control_val = -1;
			}
			if (control_val == -1) {
				continue;
			} else if (control_val == 0) {
				control_val = options[i].control;
				options[i].requested = 1;
			}
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SB;
			outbuf[outbytes++] = COM_PORT_OPTION;
			outbuf[outbytes++] = SET_CONTROL;
			outbuf[outbytes++] = control_val;
			outbuf[outbytes++] = IAC;
			outbuf[outbytes++] = SE;
			break;
		default:
			break;
		}
		need_more += options[i].requested;
	};

	do {
		pfd.events = 0;
		if (inbytes < sizeof (inbuf) && outbytes == 0)
			pfd.events |= POLLIN;
		if (outbytes > 0)
			pfd.events |= POLLOUT;

		/* Get the events. */
		res = poll (&pfd, 1, -1);
		if (res == -1) {
			perror ("poll");
			return -1;
		}

		/* Data from telnet server. */
		if (pfd.revents & POLLIN) {
			res = read (pfd.fd, &inbuf[inbytes], sizeof (inbuf) - inbytes);
			if (res > 0) {
				inbytes += res;
			} else {
				if (res == -1) {
					perror ("read");
					inbytes = 0;
				}
				close (pfd.fd);
				pfd.fd = -1;
				return 1;
			}
		}

		/* Data for the telnet server. */
		if (pfd.revents & POLLOUT) {
			res = write (pfd.fd, outbuf, outbytes);
			if (res > 0) {
				outbytes -= res;
				memmove (outbuf, &outbuf[res], outbytes);
			} else {
				if (res == -1)
					perror ("write");
				close (pfd.fd);
				pfd.fd = -1;
			}
		}

		/* Telnet has gone off. */
		if (pfd.revents & POLLHUP) {
			close (pfd.fd);
			pfd.fd = -1;
			return 1;
		}

		while (inbytes) {
			/* Process the commands. */
			while ((res = get_esc (inbuf, inbytes, got_option)) > 0) {
				inbytes -= res;
				memmove (inbuf, &inbuf[res], inbytes);
			};

			/* Drop the data. */
			while ((res = get_data (inbuf, inbytes)) > 0) {
				if (res == 1 && inbuf[0] == IAC && inbytes >= 2) {
					/* Got one (IAC), but remove two (IAC IAC). */
					res = 2;
				}
				inbytes -= res;
				memmove (inbuf, &inbuf[res], inbytes);
			};
		}
	} while (need_more || outbytes);

	return 0;
}
