/*
 * Serial port over Telnet common routines
 * Lubomir Rintel <lkundrak@v3.sk>
 * License: GPL
 */

#define _POSIX_C_SOURCE 201112L
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <sys/socket.h>
#include <sys/types.h>

#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"

static void
com_port_option (const unsigned char *buf, int size, com_port_option_callback callback)
{
	union com_port_option_value value;

	if (size < 6) {
		fprintf (stderr, "too short: %d\n", size);
		return;
	}

	switch (buf[3] - 100) {
	case SET_BAUDRATE:
		if (size < 10)
			return;
		value.baudrate = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | (buf[7] << 0);
		callback (SET_BAUDRATE, &value);
		break;
	case SET_DATASIZE:
		value.datasize = buf[4];
		callback (SET_DATASIZE, &value);
		break;
	case SET_PARITY:
		value.parity = buf[4];
		callback (SET_PARITY, &value);
		break;
	case SET_STOPSIZE:
		value.stopsize = buf[4];
		callback (SET_STOPSIZE, &value);
		break;
	case SET_CONTROL:
		value.control = buf[4];
		callback (SET_CONTROL, &value);
		break;
	}
}

int
get_esc (const unsigned char *buf, int size, com_port_option_callback callback)
{
	int i;

	if (size < 2)
		return 0;

	if (buf[0] != IAC)
		return 0;

	switch (buf[1]) {
	case IAC:
		/* If we leave IAC IAC, then it's considered as IAC data. */
		return 0;
	case WILL:
		if (size < 3)
			return 0;
		return 3;
	case DO:
		if (size < 3)
			return 0;
		return 3;
	case SB:
		for (i = 2; i < size; i++) {
			if (i + 1 <= size && buf[i] == IAC && buf[i + 1] == SE)
				break;
		}
		if (i == size)
			return 0;
		i += 2;
		if (i < 5)
			return i;
		if (buf[2] == COM_PORT_OPTION && callback)
			com_port_option (buf, i, callback);
		return i;
	}

	/* Unknown code. Just consume the IAC and command. */
	return 2;
}

/* If the first character is an IAC after the previous get_esc() call
 * it is not a command. */
int
get_data (const unsigned char *buf, int size)
{
	int i;

	if (size == 0)
		return 0;
	if (size >= 2 && buf[0] == IAC && buf[1] == IAC)
		return 1;
	for (i = 0; i < size; i++) {
		if (buf[i] == IAC)
			break;
	}

	return i;
}

int
get_socket (const char *host, const char *service)
{
	int fd;
	int res;
	struct addrinfo hints = { .ai_socktype = SOCK_STREAM };
	struct addrinfo *ai;
	struct addrinfo *p;

	res = getaddrinfo (host, service, &hints, &ai);
	if (res != 0) {
		fprintf (stderr, "%s:%s: %s\n", host, service, gai_strerror (res));
		return -1;
	}

	for (p = ai; p; p = p->ai_next) {
		fd = socket (p->ai_family, p->ai_socktype, p->ai_protocol);
		if (fd == -1)
			continue;

		res = connect (fd, p->ai_addr, p->ai_addrlen);
		if (res != -1)
			break;

		close (fd);
		fd = -1;
	}

	if (p == NULL)
		perror ("connect");

	freeaddrinfo (ai);

	return fd;
}
