/*
 * File:        orca_link.h 
 * Author:      Peter Boncz - Vrije Universiteit Amsterdam - 1994
 * Contains:	#defines to bridge the xdisplay and Orca modules
 *
 * Copyright (C) 1994  Peter Boncz
 */


/* The Orca Stuff */
typedef struct {  /* VERY DIRTY - BUT NECESSARY!! */
        void *a_data; 
	/* more - unimportant - stuff */
} orca_array_t;

	/* xdisplay calls back this orca routine */
#define XDISPLAY_DRAWCALLBACK		f_Mandelbrot__DrawCallback
#define XDISPLAY_REQUESTCALLBACK	f_Mandelbrot__RequestCallback
extern void f_Mandelbrot__DrawCallback();
extern void f_Mandelbrot__RequestCallback();

/* the less obvious params to xdisplay_create */
void *draw_callback_param;
void *request_callback_param;

/* these should match with the consts in Orca!! */
#define REQUEST_QUIT	1
#define REQUEST_ZOOM	2
#define REQUEST_RESET	3
#define REQUEST_RESIZE	4
#define REQUEST_DWELL	5
#define REQUEST_BACK	6
#define REQUEST_DEMO	7

/* backward defines: */
#define xdisplay_create	f_Xdisplay__create
#define xdisplay_clear	f_Xdisplay__clear
#define xdisplay_swap	f_Xdisplay__swap
#define xdisplay_draw	f_Xdisplay__draw
#define xdisplay_square	f_Xdisplay__square
#define xdisplay_ready	f_Xdisplay__ready
