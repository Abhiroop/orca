/*
 * File:        xdisplay.c 
 * Author:      Peter Boncz - Vrije Universiteit Amsterdam - 1994
 * Contains:	xdisplay module: a X display window for pixmap graphics.
 *
 * Copyright (C) 1994  Peter Boncz
 */
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Text.h>
#include <X11/Xaw/Cardinals.h>
#include "xdisplay.h"
#include "std_colors.h"
#include "DrawArea.h"

/* -----------------------------------------------------------------
 * forward_declarations 
 * -----------------------------------------------------------------
 */


void	quit_event_cb(); 
void	reset_event_cb(); 
void	back_event_cb(); 
void	demo_event_cb(); 
void	xdisplay_request(); 
void	clear_message();
void    xdisplay_rect_cb();
void	MakeNewColormap();
void    dialog_cancel_cb();
void    dialog_ok_action(/* Widget, XEvent, String *, Cardinal * */);
void    dialog_ok_cb();
void    setup_dialog_cb();
void	MyMainLoop();

/* -----------------------------------------------------------------
 * X globals 
 * -----------------------------------------------------------------
 */


Widget toplevel;	/* the application shell,           "xscout". */
Widget total;		/* static paned containing all:      "total". */
Widget header;		/* the menu bar, resource name:     "header". */
Widget quitb;		/* the quit button, name:             "quit". */
Widget resetb;		/* the reset button, name:           "reset". */
Widget backb;		/* the back button, name:             "back". */
Widget demob;		/* the demo button, name:             "demo". */
Widget dwellb;		/* the dwell button, name:           "dwell". */
Widget message;		/* the message label, name:        "message". */
Widget draw_area;	/* draw_area for drawing graphics:     "draw_area". */

Widget dialogsh;        /* popup shell for dialog box, "dialogsh". */
Widget dialog;          /* dialog box prompting invitee  "dialog". */
Widget cancelb;         /* Cancel button in dialog box,  "cancel". */
Widget okb;             /* OK button in dialog box, name:    "ok". */

Colormap colormap=0;
XtAppContext app;	/* the application context */
char dwell_string[LINE_LEN];
int old_dwell = 1000;
int stop = 0;


char buf[256];

String fallback_resources[] = {
    buf,
    "*geometry:					700x480",
    /* "*font:       -adobe-helvetica-bold-r-normal--18-180-75-75-p-103-iso8859-1", */
    "*font:       -adobe-helvetica-bold-r-normal--14-140-75-75-p-82-iso8859-1",
    "*input:					true",
    "*resize:					true",
    "*Foreground:				black",
    "*header*Background:			grey60",
    "*draw_area*Background:			grey60",
    "*header*quit*Background:			SandyBrown",
    "*header*dwell*Background:			SkyBlue",
    "*header*reset*Background:			YellowGreen",
    "*header*demo*Background:			HotPink",
    "*header*back*Background:			LightCyan",
    "*dialogsh*dialog*Background:               grey60",
    "*dialogsh*dialog*ok*Background:            YellowGreen",
    "*dialogsh*dialog*cancel*Background:        SandyBrown",
    "*dialogsh*dialog*translations:             #override \\n \
                                                <Key>Return: ok()",
    NULL, };

/* The actions table: return==ok in dialog box. */
XtActionsRec action_table[] = { { "ok", dialog_ok_action } };


void drawcallback();


/* -----------------------------------------------------------------
 * public routine implementations
 * -----------------------------------------------------------------
 */


void xdisplay_create(n_colors, n_reserved, width, height, ncpus, param1, param2)
int n_colors;
int n_reserved;
int width;
int height;
int ncpus;
void *param1;
void *param2;
{
    Arg args[16];
    XFontStruct *fs;
    char geometry[LINE_LEN];
    int header_height=0, make_colormap;
    int cnt = 0;

    /* store the user information */
    draw_callback_param    = param1;
    request_callback_param = param2;

    /* Make an X application shell and get your resources. */
    sprintf(buf, "*title:	Mandelbrot, %d CPU%s", ncpus, ncpus>1?"s":"");
    toplevel = XtAppInitialize(&app, buf, 0,0,
		&header_height, NULL, fallback_resources, NULL, 0);
    XtAppAddActions(app, action_table, XtNumber(action_table));

    /* Create the paned widget. */
    total = XtCreateManagedWidget("total", panedWidgetClass,
		toplevel, NULL, 0);

    /* Create the header bar widget. */
    XtSetArg(args[0], XtNshowGrip, FALSE);
    header = XtCreateManagedWidget("header", boxWidgetClass,
		total, args, 1);

    /* Create the quit button widget. */
    XtSetArg(args[0], XtNshapeStyle, XawShapeRectangle);
    XtSetArg(args[1], XtNhighlightThickness, 3);
    XtSetArg(args[2], XtNsensitive, FALSE);
    XtSetArg(args[3], XtNlabel, "quit");
    quitb = XtCreateManagedWidget("quit", 
				commandWidgetClass, header, args, 4);
    XtAddCallback(quitb, XtNcallback, quit_event_cb, NULL); 
    XtVaGetValues(quitb, XtNfont, &fs, NULL);

    /* Create the dwell button widget. */
    XtSetArg(args[3], XtNlabel, "dwell");
    dwellb = XtCreateManagedWidget("dwell", 
				commandWidgetClass, header, args, 4);
    XtAddCallback(dwellb, XtNcallback, setup_dialog_cb, NULL);

    /* Create the reset button widget. */
    XtSetArg(args[3], XtNlabel, "reset");
    resetb = XtCreateManagedWidget("reset", 
				commandWidgetClass, header, args, 4);
    XtAddCallback(resetb, XtNcallback, reset_event_cb, NULL); 

    /* Create the back button widget. */
    XtSetArg(args[3], XtNlabel, "back");
    backb = XtCreateManagedWidget("back", 
				commandWidgetClass, header, args, 4);
    XtAddCallback(backb, XtNcallback, back_event_cb, NULL); 

    /* Create the demo button widget. */
    XtSetArg(args[3], XtNlabel, "demo");
    demob = XtCreateManagedWidget("demo", 
				commandWidgetClass, header, args, 4);
    XtAddCallback(demob, XtNcallback, demo_event_cb, NULL); 

    /* create the message field */
    XtSetArg(args[0], XtNjustify, XtJustifyLeft);
    XtSetArg(args[1], XtNborderWidth, 0);
    XtSetArg(args[2], XtNlabel, "computing...");
    message = XtCreateManagedWidget("message",
                                labelWidgetClass, header, args, 3);
    /* do some sizes */
    header_height = 1.7*FONT_HEIGHT(fs);
    sprintf(geometry,"%dx%d", width, height+1+header_height);
    XtVaSetValues(toplevel, XtNgeometry, geometry, NULL);
    XtVaSetValues(header, XtNmin, header_height, NULL);
    XtVaSetValues(header, XtNmax, header_height, NULL);

    cnt = 0;
    if ((XDefaultDepthOfScreen(XtScreen(toplevel))==8) &&
			((n_colors-n_reserved)>2)) {
	/* our colormap has the standard colors at these spots: */
	YELLOWGREEN = (n_reserved - MENU_COLORS) % n_colors;
	GREY60 = (n_reserved - MENU_COLORS + 1) % n_colors;
	SANDYBROWN = (n_reserved - MENU_COLORS + 2) % n_colors;
	SKYBLUE = (n_reserved - MENU_COLORS + 3) % n_colors;
	LIGHTCYAN = (n_reserved - MENU_COLORS + 4) % n_colors;
	HOTPINK = (n_reserved - MENU_COLORS + 5) % n_colors;

	/* insert the standard color in our widgets. */
 	XtVaSetValues(quitb, XtNbackground, SANDYBROWN, NULL);
	XtVaSetValues(dwellb, XtNbackground, SKYBLUE, NULL);
	XtVaSetValues(resetb, XtNbackground, YELLOWGREEN, NULL);
	XtVaSetValues(demob, XtNbackground, HOTPINK, NULL);
	XtVaSetValues(backb, XtNbackground, LIGHTCYAN, NULL);
	XtVaSetValues(header, XtNbackground, GREY60, NULL);
	XtVaSetValues(message, XtNbackground, GREY60, NULL);
	XtVaSetValues(toplevel, XtNbackground, GREY60, NULL);
        XtSetArg(args[cnt], XtNinitColorCallback, MakeNewColormap); cnt++;
	XtSetArg(args[cnt], XtNbackground, GREY60); cnt++;
    } else
        XtSetArg(args[cnt], XtNinitColorCallback, NULL); cnt++;

    /* Create the draw_area. */
    XtSetArg(args[cnt], XtNnumColors, n_colors); cnt++;
    XtSetArg(args[cnt], XtNnumReserved, n_reserved); cnt++;
    XtSetArg(args[cnt], XtNwidth, width); cnt++;
    XtSetArg(args[cnt], XtNheight, height); cnt++;
    draw_area = XtCreateManagedWidget("draw_area", draw_areaWidgetClass,
      total, args, cnt);

    /* set the first timer in one second */
    XtAppAddTimeOut(app, 10, drawcallback, draw_callback_param);
 
    /* Put it all on the screen and go for a walk. */
    XtRealizeWidget(toplevel);
    MyMainLoop(app); 
}

void
drawcallback(o)
	void	*o;
{
	int	op_flags = 0;

	XDISPLAY_DRAWCALLBACK(&op_flags, o);
}


void MyMainLoop(app)	     /* Done by hand for explicit termination control */
XtAppContext app;
{
	XEvent event;

	do {
		XtAppNextEvent( app, &event);
		XtDispatchEvent( &event);
	}
	while (!stop);
}



void xdisplay_clear(direct)
int direct;
{
    DrawAreaClear(draw_area, direct);
    if (direct) DrawAreaRedisplay(draw_area, 0);
}






void xdisplay_swap()
{
    DrawAreaSwap(draw_area);
}




void xdisplay_square(x, y, width, height, direct)
int x;
int y;
int width;
int height;
int direct;
{
    /* do write */
    DrawAreaSquare(draw_area, x, y, width, height, direct); 
    if (direct) XSync(XtDisplay(toplevel), 0);
}



void xdisplay_draw(start, size, buffer, direct)
int start;
int size;
orca_array_t *buffer;
int direct;
{
    DrawAreaDraw(draw_area, start, size, buffer->a_data, direct); 
}



void xdisplay_ready(time)
int time;
{ 
    char mess[LINE_LEN];

    sprintf(mess, "%5d.%d sec.  ", time/1000, time%1000);
    XtVaSetValues(message, XtNlabel, mess, NULL);

    /* make buttons and zoom box active. */
    XtVaSetValues(draw_area, XtNrectCallback, xdisplay_rect_cb, NULL);
    XtVaSetValues(quitb, XtNsensitive, TRUE, NULL);
    XtVaSetValues(resetb, XtNsensitive, TRUE, NULL);
    XtVaSetValues(backb, XtNsensitive, TRUE, NULL);
    XtVaSetValues(demob, XtNsensitive, TRUE, NULL);
    XtVaSetValues(dwellb, XtNsensitive, TRUE, NULL);
    XSync(XtDisplay(toplevel), 0);
}





/* -----------------------------------------------------------------
 * auxiliary routines implementations
 * -----------------------------------------------------------------
 */

void xdisplay_request(request, i1, i2, i3, i4)
int request;
int i1, i2, i3, i4;
{
    int op_flags = 0;
    XtVaSetValues(quitb, XtNsensitive, FALSE, NULL);
    XtVaSetValues(resetb, XtNsensitive, FALSE, NULL);
    XtVaSetValues(backb, XtNsensitive, FALSE, NULL);
    XtVaSetValues(demob, XtNsensitive, FALSE, NULL);
    XtVaSetValues(dwellb, XtNsensitive, FALSE, NULL);
    XtVaSetValues(message, XtNlabel, "computing...", NULL);
    XtVaSetValues(draw_area, XtNrectCallback, 0, NULL);
    XSync(XtDisplay(toplevel), 0);
    /*
    DrawAreaClear(draw_area, 1);
    DrawAreaRedisplay(draw_area, 0);
    */
    XDISPLAY_REQUESTCALLBACK(&op_flags, request_callback_param, request, i1, i2, i3, i4);
    XtAppAddTimeOut(app, 10, drawcallback, draw_callback_param);
}




/* Someone clicked the reset button. */ 
void reset_event_cb() 
{
    xdisplay_request(REQUEST_RESET);
}



/* Someone clicked the back button. */ 
void back_event_cb() 
{
    xdisplay_request(REQUEST_BACK);
}



/* Someone clicked the demo button. */ 
void demo_event_cb() 
{
    xdisplay_request(REQUEST_DEMO);
}



/* Someone clicked the quit button. */ 
void quit_event_cb() 
{
    if (colormap) XFreeColormap(XtDisplay(toplevel), colormap);
    xdisplay_request(REQUEST_QUIT);
    stop = 1;
}



void xdisplay_rect_cb(resize, x, y, width, height)
int resize, x, y, width, height;
{
    xdisplay_request(resize?REQUEST_RESIZE:REQUEST_ZOOM, x ,y ,width, height);
}




void MakeNewColormap(n_colors, n_reserved)
int n_colors;
int n_reserved;
{
    Display *dpy = XtDisplay(toplevel);
    Screen *screen = XtScreen(toplevel);
    Window window = XtWindow(toplevel); 
    int i;

    /* get the reserved colors */
    for (i=0; i<n_reserved; i++) colorcells[i].pixel = i;
    XQueryColors(dpy, DefaultColormapOfScreen(screen), 
				colorcells, n_reserved);

    /* Create the new colormap. */
    colormap = XCreateColormap(dpy, window, 
		DefaultVisualOfScreen(screen), AllocAll);
    XSetWindowColormap(dpy, window, colormap);

    for (i=n_reserved; i<n_colors; i++) {
	colorcells[i].pixel = i;
	colorcells[i].red = red[i] <<8;
	colorcells[i].green = green[i] <<8;
	colorcells[i].blue = blue[i] <<8;
	colorcells[i].flags = DoRed | DoGreen | DoBlue;
    }

    /* snatch the upper reserved colors to put our standards */
    for (i=0; (i<=n_reserved)&&(i<=MENU_COLORS); i++) {
	colorcells[n_reserved-i].pixel = n_reserved-i;
	colorcells[n_reserved-i].red = red[MENU_COLORS-i] <<8;
	colorcells[n_reserved-i].green = green[MENU_COLORS-i] <<8;
	colorcells[n_reserved-i].blue = blue[MENU_COLORS-i] <<8;
	colorcells[n_reserved-i].flags = DoRed | DoGreen | DoBlue;
    }

    /* Store all colors in the new colormap. */
    XStoreColors(dpy, colormap, colorcells, n_colors);
}



/* -----------------------------------------------------------------
 * dialog box routines
 * -----------------------------------------------------------------
*/

void dialog_cancel_cb()
{
    XtDestroyWidget(dialogsh);
    XtVaSetValues(dwellb, XtNsensitive, TRUE, NULL);
}



void dialog_ok_action() { dialog_ok_cb(); }

void dialog_ok_cb()
{
    int dwell = atoi(XawDialogGetValueString(dialog));

    XtDestroyWidget(dialogsh);
    XtVaSetValues(dwellb, XtNsensitive, TRUE, NULL);
#define MAX_DWELL 65535
    if ((dwell>0)&&(dwell<MAX_DWELL)) 
        xdisplay_request(REQUEST_DWELL, old_dwell=dwell);
}


void setup_dialog_cb()
{
    Arg args[16];
    XFontStruct *fs;
    char geometry[LINE_LEN];

    XtVaSetValues(dwellb, XtNsensitive, FALSE, NULL);

    /* create the dialog box */
    XtVaGetValues(dwellb, XtNfont, &fs, NULL);
    if (fs==(XFontStruct *) NULL) fs=(XFontStruct *) XtDefaultFont;
    sprintf(geometry,"%dx%d+400+300",
                FONT_HEIGHT(fs)*16,6*FONT_HEIGHT(fs));
    XtSetArg(args[0], XtNtitle, "Dwell Select Box");
    XtSetArg(args[1], XtNgeometry, geometry);
    XtSetArg(args[2], XtNwindowGroup, NULL);
    XtSetArg(args[3], XtNtransientFor, NULL);
    dialogsh = XtCreatePopupShell("dialogsh",
                transientShellWidgetClass, toplevel, args, 4);

    sprintf(dwell_string, "%d", old_dwell);
    XtSetArg(args[0], XtNlabel, "select a new dwell:");
    XtSetArg(args[1], XtNvalue, dwell_string);
    dialog = XtCreateManagedWidget("dialog",
                dialogWidgetClass, dialogsh, args, 2);
    XtSetArg(args[0], XtNshapeStyle, XawShapeRectangle);
    XtSetArg(args[1], XtNhighlightThickness, 3);
    XtSetArg(args[2], XtNlabel, "ok");
    XtSetArg(args[3], XtNheight, 5 + (int) 1.1*FONT_HEIGHT(fs));
    okb = XtCreateManagedWidget("ok",
                commandWidgetClass, dialog, args, 4);
    XtAddCallback(okb, XtNcallback, dialog_ok_cb, NULL);
    XtSetArg(args[2], XtNlabel, "cancel");
    cancelb = XtCreateManagedWidget("cancel",
                commandWidgetClass, dialog, args, 4);
    XtAddCallback(cancelb, XtNcallback, dialog_cancel_cb, NULL);

    XtRealizeWidget(dialogsh);

    if (colormap) {
        XtVaSetValues(cancelb, XtNbackground, SANDYBROWN, NULL);
        XtVaSetValues(okb, XtNbackground, YELLOWGREEN, NULL);
        XtVaSetValues(dialog, XtNbackground, GREY60, NULL);
        XtVaSetValues(dialogsh, XtNbackground, GREY60, NULL);
    	XSetWindowColormap(XtDisplay(toplevel), XtWindow(dialogsh), colormap);
    }

    XtPopup(dialogsh, XtGrabNone);
}
