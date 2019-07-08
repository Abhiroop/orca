/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: strlist.h,v 1.4 1995/07/31 08:57:17 ceriel Exp $ */

/*   C O M M A - S E P A R A T E D   L I S T   H A N D L I N G   */

/*	This module provides a mechanism for walking trough a character string
	containing comma-separated items having the form aaa(bbb),
	set_strlist initializes the module for walking through the string
	"str", and get_strlist can then be called repeatedly and returns
	1 if it found an item, 0 if not. If it returns 1, "aaa" contains
	the aaa part, and "bbb" contains the bbb part. Space for these
	strings is allocated dynamically, and the application is responsible
	for freeing it.
*/

_PROTOTYPE(void *set_strlist, (char *str));
_PROTOTYPE(int get_strlist, (char **aaa, char **bbb, void *list));
#define end_strlist(l)	free(l)
