/*
 * File:        DrawArea.c 
 * Author:      Peter Boncz - Vrije Universiteit Amsterdam - 1994
 * Contains:	drawing area widget implementation.
 *
 * Copyright (C) 1994  Peter Boncz
 */
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include "DrawAreaP.h"

#define BLACK		0
#define WHITE		1
#define ASCENDING(x,y)	{if((x)>(y)){int tmp=(x);(x)=(y);(y)=(x);}}
#define INSIDE(val, x)	((int) ((x)<0?0:(x)>=(val)?(val)-1:(x)))
#define OFFSET(field)	XtOffset(DrawAreaWidget,draw_area.field)

void DrawAreaInitialize(), DrawAreaRealize(), DrawAreaResize();
void DrawAreaChangeRect(), DrawAreaUp(), DrawAreaDown(), DrawAreaMoveRect();
static XtActionsRec draw_area_actions[] = {
    {"press", DrawAreaDown},
    {"release", DrawAreaUp},
    {"move", DrawAreaMoveRect} };

static char draw_area_translations[] =
" <Btn1Down>:	press()\n\
 <Btn1Up>:	release()\n\
 <Btn1Motion>:	move()\n";


static XtResource resources[] = {
    {XtNrectCallback, "rect_callback", XtRInt, sizeof(void *),
	    OFFSET(rect_callback), XtRImmediate, (XtPointer) 0},
    {XtNinitColorCallback, "InitColor_callback", XtRInt, sizeof(void *),
	    OFFSET(InitColor_callback), XtRImmediate, (XtPointer) 0},
    {XtNnumColors, "num_colors", XtRInt, sizeof(int),
	    OFFSET(num_colors), XtRImmediate, (XtPointer) 2},
    {XtNnumReserved, "num_reserved", XtRInt, sizeof(int),
	    OFFSET(num_reserved), XtRImmediate, (XtPointer) 0},
};





DrawAreaClassRec draw_areaClassRec = {
    {
	/* core fields 		 */
	 /* superclass		 */ (WidgetClass) (&widgetClassRec),
	 /* class_name		 */ "DrawArea",
	 /* widget_size		 */ sizeof(DrawAreaRec),
	 /* class_initialize	 */ NULL,
	 /* class_part_initialize */ NULL,
	 /* class_inited         */ FALSE,
	 /* initialize		 */ DrawAreaInitialize,
	 /* initialize_hook	 */ NULL,
	 /* realize		 */ DrawAreaRealize,
	 /* actions		 */ draw_area_actions,
	 /* num_actions		 */ XtNumber(draw_area_actions),
	 /* resources		 */ resources,
	 /* resource_count	 */ XtNumber(resources),
	 /* xrm_class		 */ 0,
	 /* compress_motion	 */ TRUE,
	 /* compress_exposure	 */ TRUE,
	 /* compress_enterleave	 */ TRUE,
	 /* visible_interest	 */ FALSE,
	 /* destroy		 */ NULL,
	 /* resize		 */ DrawAreaResize,
	 /* expose		 */ DrawAreaRedisplay,
	 /* set_values		 */ NULL,
	 /* set_values_hook	 */ NULL,
	 /* set_values_almost	 */ XtInheritSetValuesAlmost,
	 /* get_values_hook	 */ NULL,
	 /* accept_focus         */ NULL,
	 /* version		 */ XtVersion,
	 /* callback_private	 */ NULL,
	 /* tm_table		 */ draw_area_translations,
	 /* query_geometry       */ NULL,
    }
};
WidgetClass draw_areaWidgetClass = (WidgetClass) &draw_areaClassRec;



/* -----------------------------------------------------------------
 * Private Routine implementations
 * -----------------------------------------------------------------
 */
void DrawAreaInitialize(request, new)
Widget request;			/* unused */
Widget new;
{
    DrawAreaWidget w = (DrawAreaWidget) new;
    Screen *screen = XtScreen(w);

    w->draw_area.rectangle_exists = 0;
}



void alloc_pixmaps(w, dpy)
DrawAreaWidget w;
Display *dpy;
{
    /* Create the two pixmaps.
     */
    w->draw_area.pixmap1 = XCreatePixmap(dpy, XtWindow(w), w->draw_area.width,
		w->core.height, w->core.depth);
    w->draw_area.pixmap2 = XCreatePixmap(dpy, XtWindow(w), w->draw_area.width,
		w->core.height, w->core.depth);
    w->draw_area.foreground = &(w->draw_area.pixmap1);
    w->draw_area.background = &(w->draw_area.pixmap2);

    if (!(w->draw_area.pixmap1&&w->draw_area.pixmap2)) {
	printf("Insufficient space for pixmap");
	exit(1);
    }
}



void DrawAreaRealize(widget, valueMask, attrs)
Widget widget;
XtValueMask *valueMask;
XSetWindowAttributes *attrs;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    Display *dpy = XtDisplay(w);
    Screen *screen = XtScreen(w);
    XtGCMask valuemask;
    XGCValues gcv;
    extern Widget toplevel;

    /* Create our window 
     */
    XtCreateWindow(widget, InputOutput, (Visual *) CopyFromParent,
      *valueMask, attrs);

    /* If necessary, make a new colormap 
     */
    if (w->draw_area.InitColor_callback)
    	(w->draw_area.InitColor_callback)(w->draw_area.num_colors,
					  w->draw_area.num_reserved);

    /* Make two GC's.
     */
    gcv.plane_mask = 0xff;
    gcv.function = GXcopy;
    gcv.fill_style = FillSolid;
    gcv.background = BLACK;
    gcv.foreground = WHITE;
    valuemask = GCFillStyle|GCBackground|GCForeground|GCFunction;
    w->draw_area.draw_gc = XtGetGC((Widget) w, valuemask, &gcv);

    gcv.background = WHITE;
    gcv.foreground = BLACK;
    w->draw_area.clear_gc = XtGetGC((Widget) w, valuemask, &gcv);

    gcv.function = GXinvert;
    gcv.plane_mask = 0xff;
    valuemask = GCBackground | GCForeground | GCFunction | GCPlaneMask;
    w->draw_area.rect_gc = XtGetGC((Widget) w, valuemask, &gcv);

    /* Create the speedup image.
     */
    w->draw_area.width = w->core.width - w->core.width%32;
    w->draw_area.height = w->core.height;
    w->draw_area.speedup = 
	XCreateImage(dpy, DefaultVisualOfScreen(screen), 
			w->core.depth, ZPixmap, 0, 0, 
			(unsigned) w->draw_area.width, 
			(unsigned) w->draw_area.height, 32,
			w->draw_area.width*w->core.depth/8);  

    /* Create the two pixmaps.
     */
    alloc_pixmaps(w, dpy);

    /* Clear the screen.
     */
    DrawAreaClear(widget, 0); DrawAreaClear(widget, 1);
    DrawAreaRedisplay(widget, 0);


}





void DrawAreaResize(widget)
Widget widget;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    Display *dpy = XtDisplay(w);
    extern Widget toplevel;

    /* for the speedup, it is necessary that the window width is a multiple
     * of 32 */
    if (!(XtIsRealized(widget)&&w->draw_area.rect_callback)) 
	return;

    if (w->core.width%32) {
        w->draw_area.width = w->core.width + 32 - w->core.width%32;
        XResizeWindow(dpy, XtWindow(toplevel), 
			w->draw_area.width, w->core.height);
    } else {
	w->draw_area.speedup->width = w->draw_area.width = w->core.width - w->core.width%32;
	w->draw_area.speedup->height = w->draw_area.height = w->core.height;
	w->draw_area.speedup->bytes_per_line = w->draw_area.width*w->core.depth/8;
	XFreePixmap(dpy, w->draw_area.pixmap1);
	XFreePixmap(dpy, w->draw_area.pixmap2);
	alloc_pixmaps(w, dpy);
    	DrawAreaClear(w, 0); DrawAreaClear(w, 1);
	(*w->draw_area.rect_callback)(1, 0, 0, 
			w->draw_area.width, w->draw_area.height);
    }
}




void DrawAreaChangeRect(w, x1, y1, x2, y2)
DrawAreaWidget w;
int x1, y1;
int x2, y2;
{
    Display *dpy = XtDisplay(w);
    Window window = XtWindow(w);

    if (w->draw_area.rectangle_exists)
	XDrawRectangle(dpy, window, w->draw_area.rect_gc, 
	  w->draw_area.last_x1, w->draw_area.last_y1,
	  (unsigned) (w->draw_area.last_x2 - w->draw_area.last_x1), 
	  (unsigned) (w->draw_area.last_y2 - w->draw_area.last_y1));

    ASCENDING(x1,x2); ASCENDING(y1,y2);
    w->draw_area.last_x1 = x1; w->draw_area.last_x2 = x2; 
    w->draw_area.last_y1 = y1; w->draw_area.last_y2 = y2;

    if (w->draw_area.rectangle_exists=((x1!=x2)&&(y1!=y2))) 
	XDrawRectangle(dpy, window, w->draw_area.rect_gc, x1, y1,
	  (unsigned) (x2 - x1), (unsigned) (y2 - y1));
}




void DrawAreaDown(widget, event)
Widget widget;
XEvent *event;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    XButtonEvent *ev = (XButtonEvent *) & event->xbutton;

    DrawAreaChangeRect(w, w->draw_area.rect_x1=ev->x,
	w->draw_area.rect_y1=ev->y, w->draw_area.rect_x1, w->draw_area.rect_y1);
}




void DrawAreaUp(widget, event)
Widget widget;
XEvent *event;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    XButtonEvent *ev = (XButtonEvent *) & event->xbutton;

    w->draw_area.rect_x2 = INSIDE(w->core.width, ev->x);
    w->draw_area.rect_y2 = INSIDE(w->core.height, ev->y);
    DrawAreaChangeRect(w, w->draw_area.rect_x1, w->draw_area.rect_y1, 
			w->draw_area.rect_x2, w->draw_area.rect_y2);
    ASCENDING(w->draw_area.rect_x1, w->draw_area.rect_x2);
    ASCENDING(w->draw_area.rect_y1, w->draw_area.rect_y2);

    if (w->draw_area.rect_callback) {
        w->draw_area.rectangle_exists = 0;
        (*w->draw_area.rect_callback)(0, 
		  w->draw_area.rect_x1, w->draw_area.rect_y1,
		  w->draw_area.rect_x2-w->draw_area.rect_x1,
		  w->draw_area.rect_y2-w->draw_area.rect_y1);
    }
}




void DrawAreaMoveRect(widget, event)
Widget widget;
XEvent *event;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    XMotionEvent *ev = (XMotionEvent *) & event->xmotion;

    DrawAreaChangeRect(w, w->draw_area.rect_x1, w->draw_area.rect_y1, 
	INSIDE(w->core.width, ev->x), INSIDE(w->core.height, ev->y));
}





/* -----------------------------------------------------------------
 * Public Routine implementations
 * -----------------------------------------------------------------
 */

void DrawAreaRedisplay(widget, event, region)
Widget widget;
XExposeEvent *event;
Region region;			/* unused */
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    Display *dpy = XtDisplay(w);
    Window window = XtWindow(w);
    Pixmap pixmap = *(w->draw_area.foreground);

    if (XtIsRealized(widget) == False)
	return;

    if (event == NULL)
	XCopyArea(dpy, pixmap, window, w->draw_area.draw_gc,
	  0, 0, w->core.width, w->core.height, 0, 0);
    else
	XCopyArea(dpy, pixmap, window, w->draw_area.draw_gc,
	  event->x, event->y, (unsigned) event->width,
	  (unsigned) event->height, event->x, event->y);
}




void DrawAreaSwap(widget) 
Widget widget;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    Pixmap *tmp = w->draw_area.foreground;

    w->draw_area.foreground = w->draw_area.background;
    w->draw_area.background = tmp;

    /* flush the screen */
    DrawAreaRedisplay(widget,0);
}




void DrawAreaSquare(widget, x, y, dx, dy, direct)
Widget widget;
int x, y, dx, dy;
int direct;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    Display *dpy = XtDisplay(w);
    Window window = XtWindow(w);
    Pixmap pixmap = (direct)?	*(w->draw_area.foreground):
				*(w->draw_area.background); 

    /* XDrawRectangle(dpy, pixmap, w->draw_area.rect_gc, x, y, dx, dy); */
    XFillRectangle(dpy, pixmap, w->draw_area.rect_gc, x, y, dx, dy);

    if (direct) {
        /* XDrawRectangle(dpy, window, w->draw_area.rect_gc, x, y, dx, dy); */
	XCopyArea(dpy, pixmap, window, w->draw_area.rect_gc, x, y, 
			(unsigned) dx, (unsigned) dy, x, y);
    }
}




void DrawAreaDraw(widget, start, size, src, direct)
Widget widget;
long start, size;
char *src;
int direct;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    Display *dpy = XtDisplay(w);
    Window window = XtWindow(w);
    Pixmap pixmap = (direct)?	*(w->draw_area.foreground):
				*(w->draw_area.background); 
    XImage *image = w->draw_area.speedup;
    int xmin, ymin, width, height;
    int i,byte,depth=w->core.depth,res=w->draw_area.num_reserved;
    char *dst=src;

    /* compute the 2b copied rectangle */
    width  = w->draw_area.width; 
    ymin   = start/width;
    xmin   = 0;
    image->height = height = 1 + (start+size-1)/width - ymin;
    image->width = width;
    image->data = src;

    if ((start%width+size) <= width) 
	xmin = start%width;

    /* speedup only works for colorscreens of depth 8 */
    if (depth!=8) {
	/* now we have to convert bytes to smaller things */
	size /= 8/depth;
	while (size--) {
	    for (byte=i=0; i<8; i+=depth, src++) {
		byte <<= depth;
		if (*src > res) byte += 1;
	    }
	    *dst = byte;
	    dst++;
	}
    }

    /* put it all back */
    XPutImage(dpy, pixmap, w->draw_area.draw_gc, image, 0, 0,
	 xmin, ymin, (unsigned) width, (unsigned) height);

    if (direct) 
    XCopyArea(dpy, pixmap, window, w->draw_area.draw_gc, xmin, 
	ymin, (unsigned) width, (unsigned) height, xmin, ymin);
}



void DrawAreaClear(widget, direct)
Widget widget;
int direct;
{
    DrawAreaWidget w = (DrawAreaWidget) widget;
    Display *dpy = XtDisplay(w);
    Pixmap pixmap = (direct)?	*(w->draw_area.foreground):
				*(w->draw_area.background); 

    XFillRectangle(dpy, pixmap, w->draw_area.clear_gc,
      0, 0, w->core.width, w->core.height);
}
