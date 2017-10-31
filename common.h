/*
 * Serial port over Telnet common definitilns
 * Lubomir Rintel <lkundrak@v3.sk>
 * License: GPL
 */

#pragma once

enum {
	COM_PORT_OPTION = 44,

	SE	= 240,
	NOP	= 241,
	DATA	= 242,
	BRK	= 243,
	IP	= 244,
	AO	= 245,
	AYT	= 246,
	EC	= 247,
	EL	= 248,
	GA	= 249,
	SB	= 250,
	WILL	= 251,
	WONT	= 252,
	DO	= 253,
	DONT	= 254,
	IAC	= 255,
};

enum com_port_option {
	SET_BAUDRATE		= 1 ,
	SET_DATASIZE		= 2 ,
	SET_PARITY		= 3 ,
	SET_STOPSIZE		= 4 ,
	SET_CONTROL		= 5 ,
	NOTIFY_LINESTATE	= 6 ,
	NOTIFY_MODEMSTATE	= 7 ,
	FLOWCONTROL_SUSPEND	= 8 ,
	FLOWCONTROL_RESUME	= 9 ,
	SET_LINESTATE_MASK	= 10 ,
	SET_MODEMSTATE_MASK	= 11 ,
	PURGE_DATA		= 12 ,
};

enum {
	CONTROL_REQ_FLOW	= 0,
	CONTROL_REQ_BREAK	= 4,
	CONTROL_REQ_DTR		= 7,
	CONTROL_REQ_RTS		= 10,
	CONTROL_REQ_FLOW_IN	= 13,
	CONTROL_REQ_RESERVED	= 20,
};

union com_port_option_value {
	struct {
		int size;
		char *str;
	} signature;
	int baudrate;
	int datasize;
	int stopsize;
	int parity;
	int control;
};

static inline int
control_to_req (int control)
{
	if (control < CONTROL_REQ_BREAK)
		return CONTROL_REQ_FLOW;
	if (control < CONTROL_REQ_DTR)
		return CONTROL_REQ_BREAK;
	if (control < CONTROL_REQ_RTS)
		return CONTROL_REQ_DTR;
	if (control < CONTROL_REQ_FLOW_IN)
		return CONTROL_REQ_RTS;
	if (control < CONTROL_REQ_RESERVED)
		return CONTROL_REQ_FLOW_IN;
	return CONTROL_REQ_RESERVED;
}

typedef void (com_port_option_callback)(enum com_port_option option, union com_port_option_value *value);

int get_esc (const unsigned char *buf, int size, com_port_option_callback callback);

int get_data (const unsigned char *buf, int size);

int get_socket (const char *host, const char *service);
