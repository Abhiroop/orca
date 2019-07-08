/*
 * File:        xdisplay.h 
 * Author:      Peter Boncz - Vrije Universiteit Amsterdam - 1994
 * Contains:	xdisplay module: a X display window for pixmap graphics.
 *
 * Copyright (C) 1994  Peter Boncz
 */
#include "orca_link.h"

#define NAME_LEN        16
#define LINE_LEN        80
#define offset(field) XtOffsetOf(struct resources, field)
#define FONT_HEIGHT(fs) (((int) (fs->max_bounds.ascent))+ \
                        ((int) (fs->max_bounds.descent)))

/* routine interface */
void xdisplay_create(/* int n_colors, n_reserved, width, height; void *param1,param2; */);
	/* Create the display with a canvas of 'width'x'height'.
	 * it calls back (after one second) the routine
	 * XDISPLAY_DRAWCALLBACK with param1.
	 * Also on Rectangle, Reset, Quit and Resize events, it calls
	 * XDISPLAY_REQUESTCALLBACK with param2.
	 */

void xdisplay_clear(/* int direct; */);
	/* Clears the canvas. 'direct'==1 means foreground, ==0 background. */ 

void xdisplay_swap();
	/* Swaps the background canvas with the foreground canvas. */ 

void xdisplay_draw(/* int start, size; orca_array_t *buffer; int direct; */);
	/* Puts the Orca array 'buffer' on the canvas. */ 

void xdisplay_square(/* int x,y,dx,dy; int direct; */);
	/* Puts an square on the background canvas . */ 

void xdisplay_ready(/* int time; */);
	/* Puts the window in interactive mode */

