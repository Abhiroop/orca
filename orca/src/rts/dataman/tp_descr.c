/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: tp_descr.c,v 1.8 1998/01/21 10:58:32 ceriel Exp $ */

/* This file defines the standard Orca type descriptors.
*/

#include <interface.h>

tp_dscr td_integer =	{ INTEGER,	sizeof(t_integer),  0, 0, 0, 0 };
tp_dscr	td_longint =	{ INTEGER,	sizeof(t_longint),  0, 0, 0, 0 };
tp_dscr	td_shortint =	{ INTEGER,	sizeof(t_shortint),  0, 0, 0, 0 };
tp_dscr	td_real =	{ REAL,		sizeof(t_real),     0, 0, 0, 0 };
tp_dscr	td_longreal =	{ REAL,		sizeof(t_longreal), 0, 0, 0, 0 };
tp_dscr	td_shortreal =	{ REAL,		sizeof(t_shortreal), 0, 0, 0, 0 };
tp_dscr	td_char =	{ CHAR,		sizeof(t_char),     0, 0, 0, 0 };
tp_dscr	td_boolean =	{ ENUM,		sizeof(t_boolean),  0, 0, 2, 0 };
tp_dscr	td_string =	{ ARRAY,	sizeof(t_string),   DYNAMIC, &td_char, 1, &td_integer };
tp_dscr td_nodename =	{ NODENAME,	sizeof(t_nodename), 0, 0, 0, 0 };
