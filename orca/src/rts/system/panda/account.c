#ifdef ACCOUNTING

#include "account.h"
#include "manager.h"
#include "panda/panda.h"
#include <stdio.h>
#include <string.h>

FILE *ac_out = NULL;
PRIVATE mutex_t print_lock;


PUBLIC void
ac_start()
{
	char buf[100];

	sprintf( buf, "account.%d", sys_my_pid);
	ac_out = fopen( buf, "w");
	if (ac_out == NULL)
		perror( buf);
	sys_mutex_init( &print_lock);
}

PUBLIC void
ac_end()
{
	fclose( ac_out);
	sys_mutex_clear( &print_lock);
}


PUBLIC void
ac_init( fragment_p f)
{
	account_p ac = &f->fr_account;
	int i, len;

    	for ( i = 0; i < AC_SIZE; i++) {
	    ac->proc_loc[i] = 0;
	    ac->proc_rpc[i] = 0;
	    ac->proc_bc[i] = 0;
	    ac->RTS_rpc[i] = 0;
	    ac->RTS_bc[i] = 0;
	}
}


PUBLIC void
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

	sys_mutex_lock( &print_lock);

	if (tot_proc + tot_RTS > 0) {
    	    fprintf( ac_out, "%2d: %-29s: ", sys_my_pid, id);

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
	    	     sys_my_pid, id,
		     tot_proc_loc, ac->proc_loc[AC_READ],
		     ac->proc_loc[AC_WRITE], tot_proc_loc,
		     ac->proc_loc[AC_READ], ac->proc_loc[AC_WRITE]);
	}

	if (tot_proc_rpc + tot_RTS_rpc > 0) {
	    fprintf(ac_out, "%2d: %29s: ", sys_my_pid, id);

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
	    fprintf(ac_out, "%2d: %29s: ", sys_my_pid, id);

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

	sys_mutex_unlock( &print_lock);
}


PUBLIC void
ac_tick( fragment_p f, access_t kind, source_t source)
{
	account_p ac = &f->fr_account;
	int status = f_get_status(f);

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
	man_tick( f, kind == AC_WRITE);
}

#endif
