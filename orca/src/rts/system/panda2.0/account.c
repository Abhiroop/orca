/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifdef ACCOUNTING

#include "account.h"
#include "manager.h"
#include "pan_sys.h"
#include <stdio.h>
#include <string.h>
#include "rts_globals.h"

FILE *ac_out = NULL;
static pan_mutex_p print_lock;


void
ac_start()
{
	if (log_accounting) {
		char buf[100];

        	sprintf( buf, "account.%d", rts_my_pid);
        	ac_out = fopen( buf, "w");
        	if (ac_out == NULL)
                	perror( buf);
        	print_lock = pan_mutex_create();
	}
}


void
ac_end()
{
	if (log_accounting) {
        	if ( fclose( ac_out) != 0) {
			perror( "account: fclose() failed (fatal)");
			exit(55);
		}
        	pan_mutex_clear( print_lock);
	}
}


void
ac_init( fragment_p f)
{
        account_p ac = &f->fr_account;
        int i;

        for ( i = 0; i < AC_SIZE; i++) {
            ac->proc_loc[i] = 0;
            ac->proc_rpc[i] = 0;
            ac->proc_bc[i] = 0;
            ac->RTS_rpc[i] = 0;
            ac->RTS_bc[i] = 0;
        }
}


void
ac_clear( fragment_p f)
{
        account_p ac = &f->fr_account;
        char *id = f->fr_name;

        int tot_proc_loc = ac->proc_loc[AC_READ] + ac->proc_loc[AC_WRITE];
        int tot_proc_rpc = ac->proc_rpc[AC_READ] + ac->proc_rpc[AC_WRITE];
        int  tot_proc_bc = ac->proc_bc[AC_READ]  + ac->proc_bc[AC_WRITE];
        int  tot_RTS_rpc = ac->RTS_rpc[AC_READ]  + ac->RTS_rpc[AC_WRITE];
        int   tot_RTS_bc = ac->RTS_bc[AC_READ]   + ac->RTS_bc[AC_WRITE];
        int     tot_proc = tot_proc_loc + tot_proc_rpc + tot_proc_bc;
        int      tot_RTS = tot_proc_loc + tot_RTS_rpc  + tot_RTS_bc;
        int    proc_read = ac->proc_loc[AC_READ]  + ac->proc_rpc[AC_READ] +
                           ac->proc_bc[AC_READ];
        int   proc_write = ac->proc_loc[AC_WRITE] + ac->proc_rpc[AC_WRITE] +
                           ac->proc_bc[AC_WRITE];
        int     RTS_read = ac->proc_loc[AC_READ]  + ac->RTS_rpc[AC_READ] +
                           ac->RTS_bc[AC_READ];
        int    RTS_write = ac->proc_loc[AC_WRITE] + ac->RTS_rpc[AC_WRITE] +
                           ac->RTS_bc[AC_WRITE];

    if (log_accounting) {
        pan_mutex_lock( print_lock);

        if (tot_proc + tot_RTS > 0) {
            fprintf( ac_out, "%2d: %-29s: ", rts_my_pid, id);

            if ( tot_proc > 0)
                fprintf( ac_out, "process %7d (%7d,%7d); ", tot_proc, proc_read,
                         proc_write);
            else
                fprintf( ac_out, "%35s", "");

            if ( tot_RTS > 0)
                fprintf( ac_out, "RTS %7d (%7d,%7d)", tot_RTS,  RTS_read,
                         RTS_write);

            fprintf( ac_out, "\n");
        }

        if (tot_proc_loc > 0) {
            fprintf( ac_out, "%2d: %29s: local   %7d (%7d,%7d);     %7d (%7d,%7d)\n",
                     rts_my_pid, id,
                     tot_proc_loc, ac->proc_loc[AC_READ],
                     ac->proc_loc[AC_WRITE], tot_proc_loc,
                     ac->proc_loc[AC_READ], ac->proc_loc[AC_WRITE]);
        }

        if (tot_proc_rpc + tot_RTS_rpc > 0) {
            fprintf(ac_out, "%2d: %29s: ", rts_my_pid, id);

            if (tot_proc_rpc > 0)
                fprintf( ac_out, "RPC     %7d (%7d,%7d); ", tot_proc_rpc,
                         ac->proc_rpc[AC_READ], ac->proc_rpc[AC_WRITE]);
            else
                fprintf( ac_out, "RPC %31s", "");

            if (tot_RTS_rpc > 0)
                fprintf( ac_out, "    %7d (%7d,%7d)", tot_RTS_rpc,
                         ac->RTS_rpc[AC_READ], ac->RTS_rpc[AC_WRITE]);
            fprintf( ac_out, "\n");
        }

        if (tot_proc_bc + tot_RTS_bc > 0) {
	    fprintf(ac_out, "%2d: %29s: ", rts_my_pid, id);

	    if (tot_proc_bc > 0)
	    	fprintf( ac_out, "BC      %7d (%7d,%7d); ", tot_proc_bc,
			 ac->proc_bc[AC_READ], ac->proc_bc[AC_WRITE]);
	    else
		fprintf( ac_out, "BC  %31s", "");

	    if (tot_RTS_bc > 0)
	    	fprintf( ac_out, "    %7d (%7d,%7d)", tot_RTS_bc,
		     	 ac->RTS_bc[AC_READ], ac->RTS_bc[AC_WRITE]);
	    fprintf( ac_out, "\n");
	}

	pan_mutex_unlock(print_lock);
    }
}


void
ac_tick( fragment_p f, access_t kind, source_t source)
{
	account_p ac = &f->fr_account;
	int status = f_get_status(f);
	int bc = 0;

	/* multiplexing: source and usage-of-broadcasting */
	switch (source) {
	  case AC_LOCAL_BC:
		bc = 1;
		source = AC_LOCAL;
		break;
	  case AC_REMOTE_BC:
		bc = 1;
		source = AC_REMOTE;
		break;
	  default:
		break;
	}

	switch ( status) {
	  case f_owner:
		if ( source == AC_REMOTE)
			ac->RTS_rpc[kind]++;
		else
			ac->proc_loc[kind]++;
		break;

	  case f_remote:
		ac->proc_rpc[kind]++;
		break;

	  case f_unshared:
		ac->proc_loc[kind]++;
		break;

	  case f_replicated:
		if ( kind == AC_READ)
			f->fr_info.delta_reads++;
		/* fall through */
	  case f_manager:
		if ( source == AC_LOCAL_FAST)
			ac->proc_loc[kind]++;
		else {
			ac->RTS_bc[kind]++;
			if ( source == AC_LOCAL)
				ac->proc_bc[kind]++;
		}
		break;

	  default:
		printf( "ac_tick: illegal fragment status\n");
		abort();
	}
	/* tick guarded reads that did not succeed immediately, but required a
	 * broadcast as write operations to aid RTS in deciding which object
	 * implementation strategy is best.
	 */
	man_tick( f, (kind == AC_WRITE) || bc);
}

#endif
