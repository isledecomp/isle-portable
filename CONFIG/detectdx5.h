#if !defined(AFX_DETECTDX5_H)
#define AFX_DETECTDX5_H

#ifdef MINIWIN
#include "miniwin/windows.h"
#else
#include <windows.h>
#endif

extern bool DetectDirectX5();

extern void DetectDirectX(unsigned int* p_version, bool* p_found);

#endif // !defined(AFX_DETECTDX5_H)
