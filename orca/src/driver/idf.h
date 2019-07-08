/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: idf.h,v 1.5 1997/07/02 14:12:36 ceriel Exp $ */

/*   I N S T A N T I A T I O N   O F   I D F   P A C K A G E   */

/* The idf package is used to associate database information with
   module/object names (through the id_db field). There is also a
   id_nxt field for walking through the list of modules/objects.
*/

struct id_u {
	DB	id_dbx;
	struct idf *id_nxtx;
	int	id_genric;
	int	id_ocd;
	char	*id_dir;
};

#define IDF_TYPE struct id_u

#define id_db	id_user.id_dbx
#define id_nxt	id_user.id_nxtx
#define id_generic	id_user.id_genric
#define id_ocdone	id_user.id_ocd
#define id_wdir		id_user.id_dir

#include <idf_pkg.spec>

typedef struct idf      t_idf;
