#pragma once

#include "mortar_begin.h"

typedef struct MORTAR_Point {
	int x;
	int y;
} MORTAR_Point;

typedef struct MORTAR_FPoint {
	float x;
	float y;
} MORTAR_FPoint;

typedef struct MORTAR_Rect {
	int x, y;
	int w, h;
} MORTAR_Rect;

typedef struct MORTAR_FRect {
	float x;
	float y;
	float w;
	float h;
} MORTAR_FRect;

#include "mortar_end.h"
