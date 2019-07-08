/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: input.h,v 1.1 1999/10/11 14:23:38 ceriel Exp $ */
#define INP_PUSHBACK 3
#include "inputtype.h"
#include <inp_pkg.spec>

/*	Note: The following macro only garuantees one PushBack.
*/
#define UnGetChar()	((LexSave != EOI) ? ChPushBack(LexSave) : 0)

extern	int LexSave;	/* last character read by GetChar		*/
extern 	int GetChar();	/* character input, with trigraph parsing	*/
