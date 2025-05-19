#ifndef MXTYPES_H
#define MXTYPES_H

#include <stdint.h>

typedef unsigned char MxU8;
typedef signed char MxS8;
typedef unsigned short MxU16;
typedef signed short MxS16;
typedef unsigned int MxU32;
typedef signed int MxS32;
typedef uint64_t MxU64;
typedef int64_t MxS64;
typedef float MxFloat;
typedef double MxDouble;

typedef int MxLong;
typedef unsigned int MxULong;

typedef MxS32 MxTime;

typedef MxLong MxResult;

#ifndef SUCCESS
#define SUCCESS 0
#endif

#ifndef FAILURE
#define FAILURE -1
#endif

typedef MxU8 MxBool;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#define TWOCC(a, b) (((a) << 0) | ((b) << 8))
#define FOURCC(a, b, c, d) (((a) << 0) | ((b) << 8) | ((c) << 16) | ((d) << 24))

// Must be union with struct for match.
typedef union {
	struct {
		MxU8 m_bit0 : 1;
		MxU8 m_bit1 : 1;
		MxU8 m_bit2 : 1;
		MxU8 m_bit3 : 1;
		MxU8 m_bit4 : 1;
		MxU8 m_bit5 : 1;
		MxU8 m_bit6 : 1;
		MxU8 m_bit7 : 1;
	};
	// BYTE all; // ?
} FlagBitfield;

static_assert(sizeof(MxBool) == 1, "sizeof(MxBool) == 1");
static_assert(sizeof(MxU8) == 1, "sizeof(MxU8) == 1");
static_assert(sizeof(MxS8) == 1, "sizeof(MxS8) == 1");
static_assert(sizeof(MxU16) == 2, "sizeof(MxU16) == 2");
static_assert(sizeof(MxS16) == 2, "sizeof(MxS16) == 2");
static_assert(sizeof(MxU32) == 4, "sizeof(MxU32) == 4");
static_assert(sizeof(MxS32) == 4, "sizeof(MxS32) == 4");
static_assert(sizeof(MxU64) == 8, "sizeof(MxU64) == 8");
static_assert(sizeof(MxS64) == 8, "sizeof(MxS64) == 8");
static_assert(sizeof(MxFloat) == 4, "sizeof(MxFloat) == 4");
static_assert(sizeof(MxDouble) == 8, "sizeof(MxDouble) == 8");
static_assert(sizeof(MxLong) == 4, "sizeof(MxLong) == 4");
static_assert(sizeof(MxULong) == 4, "sizeof(MxULong) == 4");
static_assert(sizeof(MxTime) == 4, "sizeof(MxTime) == 4");
static_assert(sizeof(MxResult) == 4, "sizeof(MxResult) == 4");

#endif // MXTYPES_H
