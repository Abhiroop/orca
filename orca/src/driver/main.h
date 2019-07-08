/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: main.h,v 1.8 1995/07/31 08:57:07 ceriel Exp $ */

/*   O R C A   C O M P I L E R   D R I V E R   */

_PROTOTYPE(int main, (int argc, char *argv[]));

_PROTOTYPE(void panic, (char *str, ...));
_PROTOTYPE(void error, (char *str, ...));

_PROTOTYPE(char *Orca_specfile, (char *nm, char *wd));
/* Looks for the specification file belonging to the module/object 
   indicated by nm. If found, its path is returned, otherwise 0 is
   returned. The module itself resides in the directory wd.
*/

_PROTOTYPE(char *include_file, (char *nm));
/* Looks for the include file indicated by nm.
   If found, its path is returned, otherwise 0 is returned.
*/

extern char	*base;		/* Basename of current .imp/.spf file. */

#define		DRIVER_DIR	".oc_driver"

extern int	changed_Cline;
extern int	v_flag;
extern char	*wdir;
extern char	*oc_comp;
