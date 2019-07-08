/*
 * File:        orca_link.h 
 * Author:      Peter Boncz - Vrije Universiteit Amsterdam - 1994
 * Contains:	drawing area private include file.
 *
 * Copyright (C) 1994  Peter Boncz
 */
#ifndef _DrawAreaP_h
#define _DrawAreaP_h

#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include "DrawArea.h"



typedef struct
{
    /* resources */
    Cardinal width, height;
    int num_colors;		/* number of color cells for display */
    int num_reserved;		/* number of reserved color cells */
    void (*rect_callback)();   /* 2b called back when a rectangle is selected */
    void (*InitColor_callback)(); /* 2b called back when the window is realized */

    /* private state */
    GC draw_gc;			/* GC for drawing the pixmaps */
    GC clear_gc;		/* GC for clearing the pixmaps */
    GC rect_gc;			/* GC for drawing the rectangles */
	/* two alternating pixmaps: one is visible, the other not. */
    Pixmap *foreground, *background; 
    Pixmap pixmap1;		/* pixmap for image */
    Pixmap pixmap2;		/* pixmap for image */
    XImage *speedup;		/* image for fast copy */ 
	/* variables for the zoom rectangle */
    int rect_x1, rect_x2, rect_y1, rect_y2;
    int last_x1, last_x2, last_y1, last_y2;
    int rectangle_exists;

} DrawAreaPart;


/* Full instance record declaration */

typedef struct _DrawAreaRec
{
    CorePart core;
    DrawAreaPart draw_area;
} DrawAreaRec;


/* New fields for the DrawArea widget class record */

typedef struct
{
    int keep_compiler_happy;
} DrawAreaClassPart;


/* Full class record declaration */

typedef struct _DrawAreaClassRec
{
    CoreClassPart core_class;
    DrawAreaClassPart draw_area_class;
} DrawAreaClassRec;


/* Class pointer */

extern DrawAreaClassRec draw_areaClassRec;

#endif _DrawAreaP_h
