/*
 * File:        std_colors.h 
 * Author:      Peter Boncz - Vrije Universiteit Amsterdam - 1994
 * Contains:	standard colors for ths xdisplay module.
 *
 * Copyright (C) 1994  Peter Boncz
 */
#define MAX_COLORS	256

#define MENU_COLORS	6
int SKYBLUE, SANDYBROWN, YELLOWGREEN, LIGHTCYAN, HOTPINK, GREY60;
 
#ifndef TEST_COLORS
static XColor colorcells[MAX_COLORS];
#endif


static unsigned char red[] = {
	'\232', '\231', '\364', '\207', '\340', '\377', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\371', '\363', 
	'\355', '\347', '\341', '\333', '\325', '\317', '\311', '\303', 
	'\275', '\267', '\261', '\253', '\245', '\237', '\231', '\223', 
	'\215', '\207', '\201', '\173', '\165', '\157', '\151', '\143', 
	'\135', '\127', '\121', '\113', '\105', '\077', '\071', '\063', 
	'\055', '\047', '\041', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\031', '\031', 
	'\031', '\031', '\031', '\031', '\031', '\031', '\037', '\045', 
	'\053', '\061', '\067', '\075', '\103', '\111', '\117', '\125', 
	'\133', '\141', '\147', '\155', '\163', '\171', '\177', '\205', 
	'\213', '\221', '\227', '\235', '\243', '\251', '\257', '\265', 
	'\273', '\301', '\307', '\315', '\323', '\331', '\337', '\345', 
	'\353', '\361', '\367', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', };
static unsigned char green[] = {
	'\315', '\231', '\244', '\316', '\377', '\151', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\041', '\047', '\055', '\063', '\071', '\077', '\105', '\113', 
	'\121', '\127', '\135', '\143', '\151', '\157', '\165', '\173', 
	'\201', '\207', '\215', '\223', '\231', '\237', '\245', '\253', 
	'\261', '\267', '\275', '\303', '\311', '\317', '\325', '\333', 
	'\341', '\347', '\355', '\363', '\371', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\371', '\363', '\355', '\347', '\341', '\333', '\325', 
	'\317', '\311', '\303', '\275', '\267', '\261', '\253', '\245', 
	'\237', '\231', '\223', '\215', '\207', '\201', '\173', '\165', 
	'\157', '\151', '\143', '\135', '\127', '\121', '\113', '\105', 
	'\077', '\071', '\063', '\055', '\047', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', };
static unsigned char blue[] = {
	'\062', '\231', '\140', '\353', '\377', '\264', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\041', '\041', '\041', '\041', 
	'\041', '\041', '\041', '\041', '\047', '\055', '\063', '\071', 
	'\077', '\105', '\113', '\121', '\127', '\135', '\143', '\151', 
	'\157', '\165', '\173', '\201', '\207', '\215', '\223', '\231', 
	'\237', '\245', '\253', '\261', '\267', '\275', '\303', '\311', 
	'\317', '\325', '\333', '\341', '\347', '\355', '\363', '\371', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\377', '\377', '\377', '\377', 
	'\377', '\377', '\377', '\377', '\371', '\363', '\355', '\347', 
	'\341', '\333', '\325', '\317', '\311', '\303', '\275', '\267', 
	'\261', '\253', '\245', '\237', '\231', '\223', '\215', '\207', 
	'\201', '\173', '\165', '\157', '\151', '\143', '\135', '\127', 
	'\121', '\113', '\105', '\077', '\071', '\063', '\055', '\047', };
