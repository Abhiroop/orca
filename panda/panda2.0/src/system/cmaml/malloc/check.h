/* $Header: /usr/proj/panda/Repositories/gnucvs/panda2.0/src/system/cmaml/malloc/check.h,v 1.1 1996/03/08 10:59:14 tim Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#ifdef	CHECK

public check_mallinks(), calc_checksum(), check_work_empty();
public started_working_on(), stopped_working_on();

#else

#define	maldump(n)		abort()
#define	check_mallinks(s)
#define	calc_checksum(ml)
#define	started_working_on(ml)	0
#define	stopped_working_on(ml)	0
#define	check_work_empty(s)

#endif	/* CHECK */
