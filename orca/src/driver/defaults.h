/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: defaults.h,v 1.12 1996/05/06 08:19:43 ceriel Exp $ */

/*   D E F A U L T   V A L U E S   F O R   C O M P I L A T I O N   */

/* Compiler names and values, and routines to get and/or set them.
*/

#ifndef OC_FLAGS
#define OC_FLAGS		""
#endif

#ifndef OC_INCLUDES
#define OC_INCLUDES		"-I$OC_HOME/$OC_LIBNAM/std"
#endif

#ifndef OC_RTSINCLUDES
#define OC_RTSINCLUDES		"-I$OC_HOME/$OC_LIBNAM/include -I$OC_HOME/$OC_LIBNAM/include/system/$OC_RTSNAM"
#endif

#ifndef OC_COMP
#define OC_COMP			"$OC_HOME/$OC_LIBNAM/oc_c.$OC_MACH"
#endif

#ifndef OC_LIBS
#define OC_LIBS			"$OC_HOME/$OC_LIBNAM/$OC_RTSNAM/$OC_MACH/$OC_SPECIAL/lib$OC_RTSNAM.a"
#endif

#ifndef OC_CCOMP
#define OC_CCOMP		"gcc"
#endif

#ifndef OC_CFLAGS
#define OC_CFLAGS		"-c"
#endif

#ifndef OC_LD
#define OC_LD			"gcc"
#endif

#ifndef OC_LDFLAGS
#define OC_LDFLAGS		""
#endif

#ifndef OC_STARTOFF
#define OC_STARTOFF		""
#endif

#ifndef OC_SPECIAL
#define OC_SPECIAL		""
#endif

#ifndef OC_RTSNAM
#define OC_RTSNAM		"unixproc"
#endif

#ifndef OC_LIBNAM
#define OC_LIBNAM		"lib"
#endif

#ifndef OC_LIBNAMOLD
#define OC_LIBNAMOLD		"lib"
#endif

#ifndef OC_HOME
#error OC_HOME should be defined
#endif

#ifndef OC_MACH
#error OC_MACH should be defined
#endif

_PROTOTYPE(char *get_value, (char *name));
/* Gets the current value of the name "name". "name" must be one of the
   macro names above, or 0 is returned.
*/

_PROTOTYPE(int set_value, (char *name, char *value));
/* Sets the value of the name "name" to "value". "name" must be one of the
   macro names above, or 0 is returned. Otherwise, 1 is returned.
*/

_PROTOTYPE(void init_defaults, (void));
/* Initialization routine. Also checks the environment for all of the
   above defines.
*/

_PROTOTYPE(void print_values, (void));
/* Print values of all names.
*/
