#ifndef __PAN_SYS_PARIX_T800_12_H__
#define __PAN_SYS_PARIX_T800_12_H__

/*
 * There are some (small) changes in interface between Parix 1.2 for the
 * T800 and Parix 1.2 for the PowerPC. In this file, we define macros so
 * both speak the same language: Parix 1.2 for the PowerPC.
 *
 * 21 March 1995 Rutger
 */

#ifdef PARIX_T800

#include <sys/link.h>
#include <sys/thread.h>
#include <sys/memory.h>

extern int LocalLink(LinkCB_t *Link[2]);	/* Missing prototype */

#define CreateThread(StackBase, StackSize, Function, Error, args) \
		StartThread(Function, StackSize, Error, sizeof(void *), args)

#define mallinfo() \
		MallInfo(MC_EXTERNAL)

#define mallsize(p) \
		MallSize(p)

#define GetLocal() \
		((void *)(GetThreadPtr()->status))

#define SetLocal(p) \
		((GetThreadPtr()->status) = (int)(p))

#define GetPriority() \
		_GetPriority()

#else		/* PARIX_T800 */

/* The manual page for the PowerPC claims the following typedef in
 * <sys/memory.h>:

     struct {
        int arena;
        int freearena;
        int usedblocks;
        int freeblocks;
     } mallinfo_t;

   However, in <sys/memory.h> we find:
     
    typedef struct mallinfo {
	size_t      Arena;                   * arena size                   *
	size_t      FreeBytes;               * free bytes in the arena      *
	size_t      UsedBlocks;              * total blocks used            *
	size_t      FreeBlocks;              * total blocks free            *
    } MallInfo_t;

 * We decide to redefine these fields so we follow the manual, not the include
 * file. Moreover, this yields compatibility between the T800 and PowerPC
 * interface.
 */


#include <malloc.h>

#define mallsize(p)     _PX_MallSize(p)
 
#define mallinfo()      _PX_MallInfo(MEM_DEFAULT)
#define arena           Arena
#define freearena       FreeBytes
#define usedblocks      UsedBytes
#define freeblocks      FreeBlocks

#endif		/* PARIX_T800 */

#endif
