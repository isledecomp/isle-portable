#if !defined(AFX_CONFIG_H)
#define AFX_CONFIG_H

#include "AboutDlg.h"
#include "compat.h"
#include "decomp.h"

#ifdef MINIWIN
#include "miniwin/d3d.h"
#else
#include <d3d.h>
#endif

#include <string>

class LegoDeviceEnumerate;
struct Direct3DDeviceInfo;
struct MxDriver;

#define currentConfigApp (&g_theApp)

// VTABLE: CONFIG 0x00406040
// SIZE 0x108
class CConfigApp {
public:
	CConfigApp();

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigApp)

public:
	bool InitInstance();
	int ExitInstance();
	//}}AFX_VIRTUAL

	// Implementation

	//	bool WriteReg(const char* p_key, const char* p_value) const;
	//	bool ReadReg(LPCSTR p_key, LPCSTR p_value, DWORD p_size) const;
	//	bool ReadRegBool(LPCSTR p_key, bool* p_bool) const;
	//	bool ReadRegInt(LPCSTR p_key, int* p_value) const;
	bool IsDeviceInBasicRGBMode() const;
	D3DCOLORMODEL GetHardwareDeviceColorModel() const;
	bool IsPrimaryDriver() const;
	bool ReadRegisterSettings();
	bool ValidateSettings();
	DWORD GetConditionalDeviceRenderBitDepth() const;
	DWORD GetDeviceRenderBitStatus() const;
	bool AdjustDisplayBitDepthBasedOnRenderStatus();
	void WriteRegisterSettings() const;
	void SetIniPath(const std::string& p_path);
	const std::string& GetIniPath() const;

	//{{AFX_MSG(CConfigApp)
	// NOTE - the ClassWizard will add and remove member functions here.
	//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	//	DECLARE_MESSAGE_MAP()

public:
	int m_aspect_ratio;
	int m_x_res;
	int m_y_res;
	int m_exf_x_res;
	int m_exf_y_res;
	float m_exf_fps;
	float m_frame_delta;
	LegoDeviceEnumerate* m_device_enumerator;
	MxDriver* m_driver;
	Direct3DDeviceInfo* m_device;
	int m_display_bit_depth;
	int m_msaa;
	int m_anisotropy;
	bool m_flip_surfaces;
	bool m_full_screen;
	bool m_exclusive_full_screen;
	int m_transition_type;
	bool m_3d_video_ram;
	bool m_wide_view_angle;
	bool m_3d_sound;
	bool m_draw_cursor;
	bool m_use_joystick;
	bool m_haptic;
	int m_joystick_index;
	int m_model_quality;
	int m_texture_quality;
	bool m_music;
	bool m_texture_load;
	std::string m_texture_path;
	std::string m_iniPath;
	std::string m_base_path;
	std::string m_cd_path;
	std::string m_save_path;
	float m_max_lod;
	int m_max_actors;
	int m_touch_scheme;
	int m_ram_quality_limit;
};

extern CConfigApp g_theApp;

#endif // !defined(AFX_CONFIG_H)
