#if !defined(AFX_DETECTDX5_H)
#define AFX_DETECTDX5_H

#ifdef MINIWIN
#include "miniwin.h"
#else
#include <windows.h>
#endif

extern BOOL DetectDirectX5();

extern void DetectDirectX(unsigned int* p_version, BOOL* p_found);

#endif // !defined(AFX_DETECTDX5_H)
