/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: chk_compile.h,v 1.3 1995/07/31 08:56:46 ceriel Exp $ */

/*   C H E C K I N G   I F   C O M P I L A T I O N   I S   N E E D E D   */

_PROTOTYPE(int chk_orca_compile, (char *fn));
/*	Check if the Orca file indicated by fn must be (re)compiled.
	This check requires several sub-checks:
	- check if there is a DB file. If not: recompile.
	- check if there is a time stamp. If not: recompile.
	- check that the filename in the time stamp corresponds with
	  fn. If not: recompile.
	- check if resulting .c and .h file (or .gc and .gh file)
	  are present. If not: recompile.
	- check time stamp with modification time of fn, corresponding
	  .spf file, and all imported .spf files. If any of those
	  is newer than the time stamp: recompile.
	- check read/write and blocking assumptions. If not correct:
	  recompile.
	Return 0 if recompilation is needed, 1 otherwise.
*/

_PROTOTYPE(int chk_C_compile, (char *fn));
/*	Check if the C file indicated by fn must be (re)compiled.
	This check requires several sub-checks:
	- check if C compilation command changed. If so: recompile.
	- check if resulting .o file is present. If not: recompile.
	- check time stamp of .o file with respect to fn and
	  included files. If any of those is newer than the time stamp:
	  recompile.
	Return 0 if recompilation is needed, 1 otherwise.
*/
