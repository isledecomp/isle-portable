#ifndef MXIO_H
#define MXIO_H

#include "mxtypes.h"

#include <SDL3/SDL_iostream.h>

// [library:filesystem]
// We define the bare minimum constants and structures to be compatible with the code in mxio.cpp
// This is mostly copy-pasted from the MMSYSTEM.H Windows header.

/* MMIO error return values */
#define MMIOERR_BASE 256
#define MMIOERR_OUTOFMEMORY (MMIOERR_BASE + 2)   /* out of memory */
#define MMIOERR_CANNOTOPEN (MMIOERR_BASE + 3)    /* cannot open */
#define MMIOERR_CANNOTREAD (MMIOERR_BASE + 5)    /* cannot read */
#define MMIOERR_CANNOTWRITE (MMIOERR_BASE + 6)   /* cannot write */
#define MMIOERR_CANNOTSEEK (MMIOERR_BASE + 7)    /* cannot seek */
#define MMIOERR_CHUNKNOTFOUND (MMIOERR_BASE + 9) /* chunk not found */
#define MMIOERR_UNBUFFERED (MMIOERR_BASE + 10)   /*  */

/* bit field masks */
#define MMIO_RWMODE 0x00000003 /* open file for reading/writing/both */

/* constants for dwFlags field of MMIOINFO */
#define MMIO_ALLOCBUF 0x00010000 /* mmioOpen() should allocate a buffer */
#define MMIO_DIRTY 0x10000000    /* I/O buffer is dirty */

/* read/write mode numbers (bit field MMIO_RWMODE) */
#define MMIO_READ 0x00000000      /* open file for reading only */
#define MMIO_WRITE 0x00000001     /* open file for writing only */
#define MMIO_READWRITE 0x00000002 /* open file for reading and writing */

/* MMIO macros */
#define mmioFOURCC(ch0, ch1, ch2, ch3) FOURCC(ch0, ch1, ch2, ch3)

/* standard four character codes */
#define FOURCC_RIFF mmioFOURCC('R', 'I', 'F', 'F')
#define FOURCC_LIST mmioFOURCC('L', 'I', 'S', 'T')

/* various MMIO flags */
#define MMIO_FINDRIFF 0x0020 /* mmioDescend: find a LIST chunk */
#define MMIO_FINDLIST 0x0040 /* mmioDescend: find a RIFF chunk */

/* general MMIO information data structure */
typedef struct _ISLE_MMIOINFO {
	/* general fields */
	MxU32 dwFlags; /* general status flags */

	/* fields maintained by MMIO functions during buffered I/O */
	Sint64 cchBuffer;  /* size of I/O buffer (or 0L) */
	char* pchBuffer;   /* start of I/O buffer (or NULL) */
	char* pchNext;     /* pointer to next byte to read/write */
	char* pchEndRead;  /* pointer to last valid byte to read */
	char* pchEndWrite; /* pointer to last byte to write */
	Sint64 lBufOffset; /* disk offset of start of buffer */

	/* fields maintained by I/O procedure */
	Sint64 lDiskOffset; /* disk offset of next read or write */
} ISLE_MMIOINFO;

/* RIFF chunk information data structure */
typedef struct _ISLE_MMCKINFO {
	MxU32 ckid;         /* chunk ID */
	MxU32 cksize;       /* chunk size */
	MxU32 fccType;      /* form type or list type */
	MxU32 dwDataOffset; /* offset of data portion of chunk */
	MxU32 dwFlags;      /* flags used by MMIO functions */
} ISLE_MMCKINFO;

// SIZE 0x48
class MXIOINFO {
public:
	MXIOINFO();
	~MXIOINFO();

	MxU16 Open(const char*, MxULong);
	MxU16 Close(MxLong);
	MxLong Read(void*, MxLong);
	MxLong Write(void*, MxLong);
	MxLong Seek(MxLong, SDL_IOWhence);
	MxU16 SetBuffer(char*, MxLong, MxLong);
	MxU16 Flush(MxU16);
	MxU16 Advance(MxU16);
	MxU16 Descend(ISLE_MMCKINFO*, const ISLE_MMCKINFO*, MxU16);
	MxU16 Ascend(ISLE_MMCKINFO*, MxU16);

	// NOTE: In MXIOINFO, the `hmmio` member of MMIOINFO is used like
	// an HFILE (int) instead of an HMMIO (WORD).
	ISLE_MMIOINFO m_info;
	// [library:filesystem] This handle is always used instead of the `hmmio` member in m_info.
	SDL_IOStream* m_file;
};

#endif // MXIO_H
