#include "mxomnicreateparam.h"

#include "decomp.h"

DECOMP_SIZE_ASSERT(MxOmniCreateParam, 0x40)

// FUNCTION: LEGO1 0x100b0b00
// FUNCTION: BETA10 0x10130b6b
MxOmniCreateParam::MxOmniCreateParam(
	HWND p_windowHandle,
	MxVideoParam& p_vparam,
	MxOmniCreateFlags p_flags
)
{
	this->m_windowHandle = p_windowHandle;
	this->m_videoParam = p_vparam;
	this->m_createFlags = p_flags;
}
