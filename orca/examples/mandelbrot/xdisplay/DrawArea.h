/*
 * File:        DrawArea.h 
 * Author:      Peter Boncz - Vrije Universiteit Amsterdam - 1994
 * Contains:	draing area widget public include file.
 *
 * Copyright (C) 1994  Peter Boncz
 */
#ifndef _DrawArea_h
#define _DrawArea_h

/* Class record constants */

extern WidgetClass draw_areaWidgetClass;

typedef struct _DrawAreaRec *DrawAreaWidget;
typedef struct _DrawAreaClassRec *DrawAreaWidgetClass;

#define XtNrectCallback "rect_callback"
#define XtNinitColorCallback "InitColor_callback"
#define XtNnumColors "num_colors"
#define XtNnumReserved "num_reserved"

/* Public functions */

void DrawAreaClear();
void DrawAreaDraw();
void DrawAreaSwap();
void DrawAreaSquare();
void DrawAreaRedisplay();

#endif _DrawArea_h
