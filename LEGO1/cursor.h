#pragma once

typedef struct CursorBitmap {
	int width;
	int height;
	const unsigned char* data;
	const unsigned char* mask;
} CursorBitmap;
