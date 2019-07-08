/* $Id: remote.h,v 1.5 1996/04/25 14:04:04 ceriel Exp $ */

/*
** Operations.
*/

#define FORK		100		/* Do fork on remote machine */
#define OPERATION	102		/* Do operation on master copy */
#define HAVEACOPY	103		/* Here's a copy for you */
#define THANKYOU 	105		/* Reply tyto HAVEACOPY/BEMASTER*/
#define EXIT 		106		/* End of run */
#define BEMASTER 	107		/* You be master */
#define MAKEMASTER 	108		/* Please make someone else master */
#define PLEASECOPY	109		/* Please send a copy */
#define BIGBUF		110		/* Big req/rep */
#define JOIN		114
#define FINISH		113		/* Terminate al processes */
#define LEAVE		115
#define SAVESTATE	116
#define INTENTSAVE	117
#define OK		118
#define START		119
#define TIME		120
#define WANT_FORK	121
#define WARMUP		122

#ifdef INVALIDATE
#define DROPCOPY	112		/* Drop your copy */
#else
#define UPDATE		101		/* Update a copy */
#define DROPME		111		/* Drop my copy */
#define LOCK		104		/* Set/release a lock */
#endif
