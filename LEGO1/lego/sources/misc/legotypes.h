/*
	This unpublished source code contains trade secrets and
	copyrighted materials which are the property of Mindscape, Inc.
	Unauthorized use, copying or distribution is a violation of U.S.
	and international laws and is strictly prohibited.
*/

#ifndef __LEGOTYPES_H
#define __LEGOTYPES_H

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef SUCCESS
#define SUCCESS 0
#endif

#ifndef FAILURE
#define FAILURE -1
#endif

typedef char LegoS8;
typedef unsigned char LegoU8;
typedef short LegoS16;
typedef unsigned short LegoU16;
typedef int LegoS32;
typedef unsigned int LegoU32;
typedef float LegoFloat;
typedef char LegoChar;

typedef LegoU8 LegoBool;
typedef LegoS32 LegoTime;
typedef LegoS32 LegoResult;

static_assert(sizeof(LegoS8) == 1, "sizeof(LegoS8) == 1");
static_assert(sizeof(LegoU8) == 1, "sizeof(LegoU8) == 1");
static_assert(sizeof(LegoS16) == 2, "sizeof(LegoS16) == 2");
static_assert(sizeof(LegoU16) == 2, "sizeof(LegoU16) == 2");
static_assert(sizeof(LegoS32) == 4, "sizeof(LegoS32) == 4");
static_assert(sizeof(LegoU32) == 4, "sizeof(LegoU32) == 4");
static_assert(sizeof(LegoFloat) == 4, "sizeof(LegoFloat) == 4");
static_assert(sizeof(LegoChar) == 1, "sizeof(LegoChar) == 1");
static_assert(sizeof(LegoChar) == 1, "sizeof(LegoChar) == 1");
static_assert(sizeof(LegoTime) == 4, "sizeof(LegoTime) == 4");
static_assert(sizeof(LegoResult) == 4, "sizeof(LegoResult) == 4");

#endif // __LEGOTYPES_H
