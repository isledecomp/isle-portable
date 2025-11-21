#pragma once

#include "mortar_begin.h"

typedef enum MORTAR_Scancode {
	MORTAR_SCANCODE_UNKNOWN = 0,

	/**
	 *  \name Usage page 0x07
	 *
	 *  These values are from usage page 0x07 (USB keyboard page).
	 */
	/* @{ */

	MORTAR_SCANCODE_A = 4,
	MORTAR_SCANCODE_B = 5,
	MORTAR_SCANCODE_C = 6,
	MORTAR_SCANCODE_D = 7,
	MORTAR_SCANCODE_E = 8,
	MORTAR_SCANCODE_F = 9,
	MORTAR_SCANCODE_G = 10,
	MORTAR_SCANCODE_H = 11,
	MORTAR_SCANCODE_I = 12,
	MORTAR_SCANCODE_J = 13,
	MORTAR_SCANCODE_K = 14,
	MORTAR_SCANCODE_L = 15,
	MORTAR_SCANCODE_M = 16,
	MORTAR_SCANCODE_N = 17,
	MORTAR_SCANCODE_O = 18,
	MORTAR_SCANCODE_P = 19,
	MORTAR_SCANCODE_Q = 20,
	MORTAR_SCANCODE_R = 21,
	MORTAR_SCANCODE_S = 22,
	MORTAR_SCANCODE_T = 23,
	MORTAR_SCANCODE_U = 24,
	MORTAR_SCANCODE_V = 25,
	MORTAR_SCANCODE_W = 26,
	MORTAR_SCANCODE_X = 27,
	MORTAR_SCANCODE_Y = 28,
	MORTAR_SCANCODE_Z = 29,

	MORTAR_SCANCODE_1 = 30,
	MORTAR_SCANCODE_2 = 31,
	MORTAR_SCANCODE_3 = 32,
	MORTAR_SCANCODE_4 = 33,
	MORTAR_SCANCODE_5 = 34,
	MORTAR_SCANCODE_6 = 35,
	MORTAR_SCANCODE_7 = 36,
	MORTAR_SCANCODE_8 = 37,
	MORTAR_SCANCODE_9 = 38,
	MORTAR_SCANCODE_0 = 39,

	MORTAR_SCANCODE_RETURN = 40,
	MORTAR_SCANCODE_ESCAPE = 41,
	MORTAR_SCANCODE_BACKSPACE = 42,
	MORTAR_SCANCODE_TAB = 43,
	MORTAR_SCANCODE_SPACE = 44,

	MORTAR_SCANCODE_MINUS = 45,
	MORTAR_SCANCODE_EQUALS = 46,
	MORTAR_SCANCODE_LEFTBRACKET = 47,
	MORTAR_SCANCODE_RIGHTBRACKET = 48,
	MORTAR_SCANCODE_BACKSLASH = 49, /**< Located at the lower left of the return
									 *   key on ISO keyboards and at the right end
									 *   of the QWERTY row on ANSI keyboards.
									 *   Produces REVERSE SOLIDUS (backslash) and
									 *   VERTICAL LINE in a US layout, REVERSE
									 *   SOLIDUS and VERTICAL LINE in a UK Mac
									 *   layout, NUMBER SIGN and TILDE in a UK
									 *   Windows layout, DOLLAR SIGN and POUND SIGN
									 *   in a Swiss German layout, NUMBER SIGN and
									 *   APOSTROPHE in a German layout, GRAVE
									 *   ACCENT and POUND SIGN in a French Mac
									 *   layout, and ASTERISK and MICRO SIGN in a
									 *   French Windows layout.
									 */
	MORTAR_SCANCODE_NONUSHASH = 50, /**< ISO USB keyboards actually use this code
									 *   instead of 49 for the same key, but all
									 *   OSes I've seen treat the two codes
									 *   identically. So, as an implementor, unless
									 *   your keyboard generates both of those
									 *   codes and your OS treats them differently,
									 *   you should generate MORTAR_SCANCODE_BACKSLASH
									 *   instead of this code. As a user, you
									 *   should not rely on this code because SDL
									 *   will never generate it with most (all?)
									 *   keyboards.
									 */
	MORTAR_SCANCODE_SEMICOLON = 51,
	MORTAR_SCANCODE_APOSTROPHE = 52,
	MORTAR_SCANCODE_GRAVE = 53, /**< Located in the top left corner (on both ANSI
								 *   and ISO keyboards). Produces GRAVE ACCENT and
								 *   TILDE in a US Windows layout and in US and UK
								 *   Mac layouts on ANSI keyboards, GRAVE ACCENT
								 *   and NOT SIGN in a UK Windows layout, SECTION
								 *   SIGN and PLUS-MINUS SIGN in US and UK Mac
								 *   layouts on ISO keyboards, SECTION SIGN and
								 *   DEGREE SIGN in a Swiss German layout (Mac:
								 *   only on ISO keyboards), CIRCUMFLEX ACCENT and
								 *   DEGREE SIGN in a German layout (Mac: only on
								 *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
								 *   French Windows layout, COMMERCIAL AT and
								 *   NUMBER SIGN in a French Mac layout on ISO
								 *   keyboards, and LESS-THAN SIGN and GREATER-THAN
								 *   SIGN in a Swiss German, German, or French Mac
								 *   layout on ANSI keyboards.
								 */
	MORTAR_SCANCODE_COMMA = 54,
	MORTAR_SCANCODE_PERIOD = 55,
	MORTAR_SCANCODE_SLASH = 56,

	MORTAR_SCANCODE_CAPSLOCK = 57,

	MORTAR_SCANCODE_F1 = 58,
	MORTAR_SCANCODE_F2 = 59,
	MORTAR_SCANCODE_F3 = 60,
	MORTAR_SCANCODE_F4 = 61,
	MORTAR_SCANCODE_F5 = 62,
	MORTAR_SCANCODE_F6 = 63,
	MORTAR_SCANCODE_F7 = 64,
	MORTAR_SCANCODE_F8 = 65,
	MORTAR_SCANCODE_F9 = 66,
	MORTAR_SCANCODE_F10 = 67,
	MORTAR_SCANCODE_F11 = 68,
	MORTAR_SCANCODE_F12 = 69,

	MORTAR_SCANCODE_PRINTSCREEN = 70,
	MORTAR_SCANCODE_SCROLLLOCK = 71,
	MORTAR_SCANCODE_PAUSE = 72,
	MORTAR_SCANCODE_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
								   does send code 73, not 117) */
	MORTAR_SCANCODE_HOME = 74,
	MORTAR_SCANCODE_PAGEUP = 75,
	MORTAR_SCANCODE_DELETE = 76,
	MORTAR_SCANCODE_END = 77,
	MORTAR_SCANCODE_PAGEDOWN = 78,
	MORTAR_SCANCODE_RIGHT = 79,
	MORTAR_SCANCODE_LEFT = 80,
	MORTAR_SCANCODE_DOWN = 81,
	MORTAR_SCANCODE_UP = 82,

	MORTAR_SCANCODE_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
										*/
	MORTAR_SCANCODE_KP_DIVIDE = 84,
	MORTAR_SCANCODE_KP_MULTIPLY = 85,
	MORTAR_SCANCODE_KP_MINUS = 86,
	MORTAR_SCANCODE_KP_PLUS = 87,
	MORTAR_SCANCODE_KP_ENTER = 88,
	MORTAR_SCANCODE_KP_1 = 89,
	MORTAR_SCANCODE_KP_2 = 90,
	MORTAR_SCANCODE_KP_3 = 91,
	MORTAR_SCANCODE_KP_4 = 92,
	MORTAR_SCANCODE_KP_5 = 93,
	MORTAR_SCANCODE_KP_6 = 94,
	MORTAR_SCANCODE_KP_7 = 95,
	MORTAR_SCANCODE_KP_8 = 96,
	MORTAR_SCANCODE_KP_9 = 97,
	MORTAR_SCANCODE_KP_0 = 98,
	MORTAR_SCANCODE_KP_PERIOD = 99,

	MORTAR_SCANCODE_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
										   *   keyboards have over ANSI ones,
										   *   located between left shift and Z.
										   *   Produces GRAVE ACCENT and TILDE in a
										   *   US or UK Mac layout, REVERSE SOLIDUS
										   *   (backslash) and VERTICAL LINE in a
										   *   US or UK Windows layout, and
										   *   LESS-THAN SIGN and GREATER-THAN SIGN
										   *   in a Swiss German, German, or French
										   *   layout. */
	MORTAR_SCANCODE_APPLICATION = 101,    /**< windows contextual menu, compose */
	MORTAR_SCANCODE_POWER = 102,          /**< The USB document says this is a status flag,
										   *   not a physical key - but some Mac keyboards
										   *   do have a power key. */
	MORTAR_SCANCODE_KP_EQUALS = 103,
	MORTAR_SCANCODE_F13 = 104,
	MORTAR_SCANCODE_F14 = 105,
	MORTAR_SCANCODE_F15 = 106,
	MORTAR_SCANCODE_F16 = 107,
	MORTAR_SCANCODE_F17 = 108,
	MORTAR_SCANCODE_F18 = 109,
	MORTAR_SCANCODE_F19 = 110,
	MORTAR_SCANCODE_F20 = 111,
	MORTAR_SCANCODE_F21 = 112,
	MORTAR_SCANCODE_F22 = 113,
	MORTAR_SCANCODE_F23 = 114,
	MORTAR_SCANCODE_F24 = 115,
	MORTAR_SCANCODE_EXECUTE = 116,
	MORTAR_SCANCODE_HELP = 117, /**< AL Integrated Help Center */
	MORTAR_SCANCODE_MENU = 118, /**< Menu (show menu) */
	MORTAR_SCANCODE_SELECT = 119,
	MORTAR_SCANCODE_STOP = 120,  /**< AC Stop */
	MORTAR_SCANCODE_AGAIN = 121, /**< AC Redo/Repeat */
	MORTAR_SCANCODE_UNDO = 122,  /**< AC Undo */
	MORTAR_SCANCODE_CUT = 123,   /**< AC Cut */
	MORTAR_SCANCODE_COPY = 124,  /**< AC Copy */
	MORTAR_SCANCODE_PASTE = 125, /**< AC Paste */
	MORTAR_SCANCODE_FIND = 126,  /**< AC Find */
	MORTAR_SCANCODE_MUTE = 127,
	MORTAR_SCANCODE_VOLUMEUP = 128,
	MORTAR_SCANCODE_VOLUMEDOWN = 129,
	/* not sure whether there's a reason to enable these */
	/*     MORTAR_SCANCODE_LOCKINGCAPSLOCK = 130,  */
	/*     MORTAR_SCANCODE_LOCKINGNUMLOCK = 131, */
	/*     MORTAR_SCANCODE_LOCKINGSCROLLLOCK = 132, */
	MORTAR_SCANCODE_KP_COMMA = 133,
	MORTAR_SCANCODE_KP_EQUALSAS400 = 134,

	MORTAR_SCANCODE_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
											footnotes in USB doc */
	MORTAR_SCANCODE_INTERNATIONAL2 = 136,
	MORTAR_SCANCODE_INTERNATIONAL3 = 137, /**< Yen */
	MORTAR_SCANCODE_INTERNATIONAL4 = 138,
	MORTAR_SCANCODE_INTERNATIONAL5 = 139,
	MORTAR_SCANCODE_INTERNATIONAL6 = 140,
	MORTAR_SCANCODE_INTERNATIONAL7 = 141,
	MORTAR_SCANCODE_INTERNATIONAL8 = 142,
	MORTAR_SCANCODE_INTERNATIONAL9 = 143,
	MORTAR_SCANCODE_LANG1 = 144, /**< Hangul/English toggle */
	MORTAR_SCANCODE_LANG2 = 145, /**< Hanja conversion */
	MORTAR_SCANCODE_LANG3 = 146, /**< Katakana */
	MORTAR_SCANCODE_LANG4 = 147, /**< Hiragana */
	MORTAR_SCANCODE_LANG5 = 148, /**< Zenkaku/Hankaku */
	MORTAR_SCANCODE_LANG6 = 149, /**< reserved */
	MORTAR_SCANCODE_LANG7 = 150, /**< reserved */
	MORTAR_SCANCODE_LANG8 = 151, /**< reserved */
	MORTAR_SCANCODE_LANG9 = 152, /**< reserved */

	MORTAR_SCANCODE_ALTERASE = 153, /**< Erase-Eaze */
	MORTAR_SCANCODE_SYSREQ = 154,
	MORTAR_SCANCODE_CANCEL = 155, /**< AC Cancel */
	MORTAR_SCANCODE_CLEAR = 156,
	MORTAR_SCANCODE_PRIOR = 157,
	MORTAR_SCANCODE_RETURN2 = 158,
	MORTAR_SCANCODE_SEPARATOR = 159,
	MORTAR_SCANCODE_OUT = 160,
	MORTAR_SCANCODE_OPER = 161,
	MORTAR_SCANCODE_CLEARAGAIN = 162,
	MORTAR_SCANCODE_CRSEL = 163,
	MORTAR_SCANCODE_EXSEL = 164,

	MORTAR_SCANCODE_KP_00 = 176,
	MORTAR_SCANCODE_KP_000 = 177,
	MORTAR_SCANCODE_THOUSANDSSEPARATOR = 178,
	MORTAR_SCANCODE_DECIMALSEPARATOR = 179,
	MORTAR_SCANCODE_CURRENCYUNIT = 180,
	MORTAR_SCANCODE_CURRENCYSUBUNIT = 181,
	MORTAR_SCANCODE_KP_LEFTPAREN = 182,
	MORTAR_SCANCODE_KP_RIGHTPAREN = 183,
	MORTAR_SCANCODE_KP_LEFTBRACE = 184,
	MORTAR_SCANCODE_KP_RIGHTBRACE = 185,
	MORTAR_SCANCODE_KP_TAB = 186,
	MORTAR_SCANCODE_KP_BACKSPACE = 187,
	MORTAR_SCANCODE_KP_A = 188,
	MORTAR_SCANCODE_KP_B = 189,
	MORTAR_SCANCODE_KP_C = 190,
	MORTAR_SCANCODE_KP_D = 191,
	MORTAR_SCANCODE_KP_E = 192,
	MORTAR_SCANCODE_KP_F = 193,
	MORTAR_SCANCODE_KP_XOR = 194,
	MORTAR_SCANCODE_KP_POWER = 195,
	MORTAR_SCANCODE_KP_PERCENT = 196,
	MORTAR_SCANCODE_KP_LESS = 197,
	MORTAR_SCANCODE_KP_GREATER = 198,
	MORTAR_SCANCODE_KP_AMPERSAND = 199,
	MORTAR_SCANCODE_KP_DBLAMPERSAND = 200,
	MORTAR_SCANCODE_KP_VERTICALBAR = 201,
	MORTAR_SCANCODE_KP_DBLVERTICALBAR = 202,
	MORTAR_SCANCODE_KP_COLON = 203,
	MORTAR_SCANCODE_KP_HASH = 204,
	MORTAR_SCANCODE_KP_SPACE = 205,
	MORTAR_SCANCODE_KP_AT = 206,
	MORTAR_SCANCODE_KP_EXCLAM = 207,
	MORTAR_SCANCODE_KP_MEMSTORE = 208,
	MORTAR_SCANCODE_KP_MEMRECALL = 209,
	MORTAR_SCANCODE_KP_MEMCLEAR = 210,
	MORTAR_SCANCODE_KP_MEMADD = 211,
	MORTAR_SCANCODE_KP_MEMSUBTRACT = 212,
	MORTAR_SCANCODE_KP_MEMMULTIPLY = 213,
	MORTAR_SCANCODE_KP_MEMDIVIDE = 214,
	MORTAR_SCANCODE_KP_PLUSMINUS = 215,
	MORTAR_SCANCODE_KP_CLEAR = 216,
	MORTAR_SCANCODE_KP_CLEARENTRY = 217,
	MORTAR_SCANCODE_KP_BINARY = 218,
	MORTAR_SCANCODE_KP_OCTAL = 219,
	MORTAR_SCANCODE_KP_DECIMAL = 220,
	MORTAR_SCANCODE_KP_HEXADECIMAL = 221,

	MORTAR_SCANCODE_LCTRL = 224,
	MORTAR_SCANCODE_LSHIFT = 225,
	MORTAR_SCANCODE_LALT = 226, /**< alt, option */
	MORTAR_SCANCODE_LGUI = 227, /**< windows, command (apple), meta */
	MORTAR_SCANCODE_RCTRL = 228,
	MORTAR_SCANCODE_RSHIFT = 229,
	MORTAR_SCANCODE_RALT = 230, /**< alt gr, option */
	MORTAR_SCANCODE_RGUI = 231, /**< windows, command (apple), meta */

	MORTAR_SCANCODE_MODE = 257, /**< I'm not sure if this is really not covered
								 *   by any of the above, but since there's a
								 *   special MORTAR_KMOD_MODE for it I'm adding it here
								 */

	/* @} */ /* Usage page 0x07 */

	/**
	 *  \name Usage page 0x0C
	 *
	 *  These values are mapped from usage page 0x0C (USB consumer page).
	 *
	 *  There are way more keys in the spec than we can represent in the
	 *  current scancode range, so pick the ones that commonly come up in
	 *  real world usage.
	 */
	/* @{ */

	MORTAR_SCANCODE_SLEEP = 258, /**< Sleep */
	MORTAR_SCANCODE_WAKE = 259,  /**< Wake */

	MORTAR_SCANCODE_CHANNEL_INCREMENT = 260, /**< Channel Increment */
	MORTAR_SCANCODE_CHANNEL_DECREMENT = 261, /**< Channel Decrement */

	MORTAR_SCANCODE_MEDIA_PLAY = 262,           /**< Play */
	MORTAR_SCANCODE_MEDIA_PAUSE = 263,          /**< Pause */
	MORTAR_SCANCODE_MEDIA_RECORD = 264,         /**< Record */
	MORTAR_SCANCODE_MEDIA_FAST_FORWARD = 265,   /**< Fast Forward */
	MORTAR_SCANCODE_MEDIA_REWIND = 266,         /**< Rewind */
	MORTAR_SCANCODE_MEDIA_NEXT_TRACK = 267,     /**< Next Track */
	MORTAR_SCANCODE_MEDIA_PREVIOUS_TRACK = 268, /**< Previous Track */
	MORTAR_SCANCODE_MEDIA_STOP = 269,           /**< Stop */
	MORTAR_SCANCODE_MEDIA_EJECT = 270,          /**< Eject */
	MORTAR_SCANCODE_MEDIA_PLAY_PAUSE = 271,     /**< Play / Pause */
	MORTAR_SCANCODE_MEDIA_SELECT = 272,         /* Media Select */

	MORTAR_SCANCODE_AC_NEW = 273,        /**< AC New */
	MORTAR_SCANCODE_AC_OPEN = 274,       /**< AC Open */
	MORTAR_SCANCODE_AC_CLOSE = 275,      /**< AC Close */
	MORTAR_SCANCODE_AC_EXIT = 276,       /**< AC Exit */
	MORTAR_SCANCODE_AC_SAVE = 277,       /**< AC Save */
	MORTAR_SCANCODE_AC_PRINT = 278,      /**< AC Print */
	MORTAR_SCANCODE_AC_PROPERTIES = 279, /**< AC Properties */

	MORTAR_SCANCODE_AC_SEARCH = 280,    /**< AC Search */
	MORTAR_SCANCODE_AC_HOME = 281,      /**< AC Home */
	MORTAR_SCANCODE_AC_BACK = 282,      /**< AC Back */
	MORTAR_SCANCODE_AC_FORWARD = 283,   /**< AC Forward */
	MORTAR_SCANCODE_AC_STOP = 284,      /**< AC Stop */
	MORTAR_SCANCODE_AC_REFRESH = 285,   /**< AC Refresh */
	MORTAR_SCANCODE_AC_BOOKMARKS = 286, /**< AC Bookmarks */

	/* @} */ /* Usage page 0x0C */

	/**
	 *  \name Mobile keys
	 *
	 *  These are values that are often used on mobile phones.
	 */
	/* @{ */

	MORTAR_SCANCODE_SOFTLEFT = 287,  /**< Usually situated below the display on phones and
										used as a multi-function feature key for selecting
										a software defined function shown on the bottom left
										of the display. */
	MORTAR_SCANCODE_SOFTRIGHT = 288, /**< Usually situated below the display on phones and
									   used as a multi-function feature key for selecting
									   a software defined function shown on the bottom right
									   of the display. */
	MORTAR_SCANCODE_CALL = 289,      /**< Used for accepting phone calls. */
	MORTAR_SCANCODE_ENDCALL = 290,   /**< Used for rejecting phone calls. */

	/* @} */ /* Mobile keys */

	/* Add any other keys here. */

	MORTAR_SCANCODE_RESERVED = 400, /**< 400-500 reserved for dynamic keycodes */

	MORTAR_SCANCODE_COUNT = 512 /**< not a key, just marks the number of scancodes for array bounds */

} MORTAR_Scancode;

#include "mortar_end.h"
