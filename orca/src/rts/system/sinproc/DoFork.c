/* $Id: DoFork.c,v 1.4 1996/07/04 08:53:46 ceriel Exp $ */

#include <interface.h>

void DoFork(int cpu, prc_dscr *procdscr, void **argtab)
{
  fprintf(stderr, "No FORKS allowed.\n");
  exit(1);
}
