/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/*=====================================================================*/
/*==== x.c : C imlementation of the Orca functions declared in     ====*/
/*==           Xd.spf                                                ==*/
/*==                                                                 ==*/

#include <stdio.h>
#include <interface.h>
#include "Xd.h"
/* #include "vogle.h" */
/* vogle.h not included, to make sure this compiles even if Vogle is not
   present on the system.
*/


/*---------------------------------------------------------------------*/
/*---- Note : All functions are directly mapped to the vogle functions */
/*            See the vogle documentation.                             */
/*---------------------------------------------------------------------*/

void f_Xd__X_Init(void)
{

    vinit(NULL);
    color(0);	/* BLACK */
    clear();
}


int  f_Xd__BackBuffer()
{
    return backbuffer();
}


void f_Xd__Circle(
	double  x, 
	double  y,
	double  radius
)
{
    circle(x, y, radius);
}

void f_Xd__CirclePrecision(
	int nseg
)
{
    circleprecision(nseg);
}

void f_Xd__Clear()
{
    clear();
}


void f_Xd__Color(
        int col
)
{
    (void)color(col);
}



void f_Xd__Draw2(
        double x, 
	double y
)
{
    draw2(x, y);
}



void f_Xd__DrawStr(
        t_string *os
)
{
    char *str = (char*)os->a_data + 1;    /* don't ask me why, but strings in Orca
					     are always preceded by a "X"
					   */
    drawstr(str);
}


void f_Xd__X_Exit(void)
{
    int c;

    vflush();

    (void)fprintf(stdout, "press 'q' in window to exit\n");

    if ((c = getkey()) == -1){
	(void)fprintf(stdout, " device has no keyboard\n");
	return;
    }
    
    while ((char)c != 'q'){
	c = getkey();
    }

    vexit();
}


void f_Xd__MapColor(
        int index,
        int red,
        int green,
        int blue
)
{
    mapcolor(index, red, green, blue);
}    

void f_Xd__Move2(
        double x,
        double y
)
{
    move2(x, y);
}


void f_Xd__Ortho2(
	double       left,
        double       right,
        double       bottom,
        double       top
)
{
    ortho2(left, right, bottom, top);
}


void f_Xd__PolyFill(
        t_boolean onoff
)
{
    polyfill(onoff);
}


void f_Xd__Point2(
	double x, 
	double y
)
{
    point2(x, y);
}

void f_Xd__Rect(
        double x1,
        double y1, 
        double x2,
        double y2
)
{
    rect(x1, y1, x2, y2);
}


void f_Xd__PrefPosition(
	int x,
        int y
)
{
    prefposition(x, y);
}


void f_Xd__PrefSize(
	int width,
        int height
)
{
    prefsize(width, height);
}


void f_Xd__SwapBuffers()
{
    swapbuffers();
}


void f_Xd__VsetFlush(
	t_boolean yesno
)
{
    vsetflush(yesno);
}


void f_Xd__Vflush()
{
    vflush();
}
