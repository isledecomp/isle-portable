#ifndef MXVIDEOPARAM_H
#define MXVIDEOPARAM_H

#include "compat.h"
#include "lego1_export.h"
#include "mxgeometry.h"
#include "mxtypes.h"
#include "mxvideoparamflags.h"

#ifdef MINIWIN
#include "miniwin/ddraw.h"
#else
#include <ddraw.h>
#endif

class MxPalette;

#define ISLE_PROP_WINDOW_CREATE_VIDEO_PARAM "ISLE.window.create.videoParam"

// SIZE 0x24
class MxVideoParam {
public:
	LEGO1_EXPORT MxVideoParam();
	LEGO1_EXPORT MxVideoParam(
		MxRect32& p_rect,
		MxPalette* p_palette,
		MxULong p_backBuffers,
		MxVideoParamFlags& p_flags
	);
	MxVideoParam(MxVideoParam& p_videoParam);
	LEGO1_EXPORT ~MxVideoParam();
	LEGO1_EXPORT void SetDeviceName(char* p_deviceId);
	LEGO1_EXPORT MxVideoParam& operator=(const MxVideoParam& p_videoParam);

	// FUNCTION: BETA10 0x100886e0
	MxVideoParamFlags& Flags() { return m_flags; }

	// FUNCTION: BETA10 0x100d81f0
	MxRect32& GetRect() { return m_rect; }

	// FUNCTION: BETA10 0x100d8210
	MxPalette* GetPalette() { return m_palette; }

	// FUNCTION: BETA10 0x100d8240
	void SetPalette(MxPalette* p_palette) { m_palette = p_palette; }

	// FUNCTION: BETA10 0x100d8270
	char* GetDeviceName() { return m_deviceId; }

	// FUNCTION: BETA10 0x10141f60
	MxU32 GetBackBuffers() { return m_backBuffers; }

	// FUNCTION: BETA10 0x10141fe0
	void SetBackBuffers(MxU32 p_backBuffers) { m_backBuffers = p_backBuffers; }

	void SetMSAASamples(MxU32 p_msaaSamples) { m_msaaSamples = p_msaaSamples; }
	MxU32 GetMSAASamples() { return m_msaaSamples; }

	void SetAnisotropic(MxFloat p_anisotropic) { m_anisotropic = p_anisotropic; }
	MxFloat GetAnisotropic() { return m_anisotropic; }

private:
	MxRect32 m_rect;           // 0x00
	MxPalette* m_palette;      // 0x10
	MxU32 m_backBuffers;       // 0x14
	MxVideoParamFlags m_flags; // 0x18
	int m_unk0x1c;             // 0x1c
	char* m_deviceId;          // 0x20
	MxU32 m_msaaSamples;
	MxFloat m_anisotropic;
};

#endif // MXVIDEOPARAM_H
