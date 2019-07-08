/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_BG_H__
#define __PAN_BG_H__

/*
   This group module provides a single group joined by all platforms.

   This module depends upon:
   \begin{itemize}
   \item the system module
   \end{itemize}
*/

#include "pan_sys.h"

/*
 \section{Initialization and termination}
 */
extern void pan_bg_init(void);
extern void pan_bg_end(void);

 
/* 
\section{Sending and Receiving}
*/
#define BG_MODE_SYNC  0
#define BG_MODE_SYNC2 1
#define BG_MODE_ASYNC 2
 
extern int  pan_bg_send_message(pan_msg_p message, int mode);
extern void pan_bg_finish_send(int entry);
extern void pan_bg_register(void (*func)(pan_msg_p message));
 

#endif /* __PAN_BG_H__ */
