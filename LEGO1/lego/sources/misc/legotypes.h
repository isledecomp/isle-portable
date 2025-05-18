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
static_assert(sizeof(LegoS8) == 1, "Incorrect size");
typedef unsigned char LegoU8;
static_assert(sizeof(LegoU8) == 1, "Incorrect size");
typedef short LegoS16;
static_assert(sizeof(LegoS16) == 2, "Incorrect size");
typedef unsigned short LegoU16;
static_assert(sizeof(LegoU16) == 2, "Incorrect size");
typedef int LegoS32;
static_assert(sizeof(LegoS32) == 4, "Incorrect size");
typedef unsigned int LegoU32;
static_assert(sizeof(LegoU32) == 4, "Incorrect size");
typedef float LegoFloat;
static_assert(sizeof(LegoFloat) == 4, "Incorrect size");
typedef char LegoChar;
static_assert(sizeof(LegoChar) == 1, "Incorrect size");

typedef LegoU8 LegoBool;
static_assert(sizeof(LegoChar) == 1, "Incorrect size");
typedef LegoS32 LegoTime;
static_assert(sizeof(LegoTime) == 4, "Incorrect size");
typedef LegoS32 LegoResult;
static_assert(sizeof(LegoResult) == 4, "Incorrect size");

#endif // __LEGOTYPES_H
