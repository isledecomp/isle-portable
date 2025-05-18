#ifndef MXTYPES_H
#define MXTYPES_H

#include <stdint.h>

typedef unsigned char MxU8;
static_assert(sizeof(MxU8) == 1, "Incorrect size");
typedef signed char MxS8;
static_assert(sizeof(MxS8) == 1, "Incorrect size");
typedef unsigned short MxU16;
static_assert(sizeof(MxU16) == 2, "Incorrect size");
typedef signed short MxS16;
static_assert(sizeof(MxS16) == 2, "Incorrect size");
typedef unsigned int MxU32;
static_assert(sizeof(MxU32) == 4, "Incorrect size");
typedef signed int MxS32;
static_assert(sizeof(MxS32) == 4, "Incorrect size");
typedef uint64_t MxU64;
static_assert(sizeof(MxU64) == 8, "Incorrect size");
typedef int64_t MxS64;
static_assert(sizeof(MxS64) == 8, "Incorrect size");
typedef float MxFloat;
static_assert(sizeof(MxFloat) == 4, "Incorrect size");
typedef double MxDouble;
static_assert(sizeof(MxDouble) == 8, "Incorrect size");

typedef int MxLong;
static_assert(sizeof(MxLong) == 4, "Incorrect size");
typedef unsigned int MxULong;
static_assert(sizeof(MxULong) == 4, "Incorrect size");

typedef MxS32 MxTime;
static_assert(sizeof(MxTime) == 4, "Incorrect size");

typedef MxLong MxResult;
static_assert(sizeof(MxResult) == 4, "Incorrect size");

#ifndef SUCCESS
#define SUCCESS 0
#endif

#ifndef FAILURE
#define FAILURE -1
#endif

typedef MxU8 MxBool;
static_assert(sizeof(MxBool) == 1, "Incorrect size");

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

#endif // MXTYPES_H
