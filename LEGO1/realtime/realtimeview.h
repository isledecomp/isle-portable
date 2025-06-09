#ifndef REALTIMEVIEW_H
#define REALTIMEVIEW_H

#include "lego1_export.h"

extern float g_userMaxLodPower;

class RealtimeView {
public:
	RealtimeView();
	~RealtimeView();

	static float GetPartsThreshold();
	LEGO1_EXPORT static float GetUserMaxLOD();
	static void SetPartsThreshold(float);
	static void UpdateMaxLOD();
	LEGO1_EXPORT static void SetUserMaxLOD(float);

	static float GetUserMaxLodPower() { return g_userMaxLodPower; }
};

#endif // REALTIMEVIEW_H
