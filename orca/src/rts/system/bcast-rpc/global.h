/* $Id: global.h,v 1.2 1994/03/11 10:51:32 ceriel Exp $ */

#ifdef DEBUG
extern FILE *output;
#define MON_EVENT(str)	{ fprintf(output, "%s\n", str); fflush(output); }
#else
#define MON_EVENT(str)
#endif
