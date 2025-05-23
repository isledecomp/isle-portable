#include "mxomnicreateparam.h"

#include "decomp.h"

DECOMP_SIZE_ASSERT(MxOmniCreateParam, 0x40)

// FUNCTION: LEGO1 0x100b0b00
// FUNCTION: BETA10 0x10130b6b
MxOmniCreateParam::MxOmniCreateParam(
	const char* p_mediaPath,
	HWND p_windowHandle,
	MxVideoParam& p_vparam,
	MxOmniCreateFlags p_flags
)
{
	this->m_mediaPath = p_mediaPath;
	this->m_windowHandle = p_windowHandle;
	this->m_videoParam = p_vparam;
	this->m_createFlags = p_flags;
}
