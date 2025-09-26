#include <psp2/gxm.h>

#define GXP(sym)                                                                                                       \
	extern uint8_t _inc_##sym[];                                                                                       \
	static const SceGxmProgram* sym = (const SceGxmProgram*) _inc_##sym;

GXP(mainVertexProgramGxp);
GXP(mainColorFragmentProgramGxp);
GXP(mainTextureFragmentProgramGxp);
GXP(planeVertexProgramGxp);
GXP(imageFragmentProgramGxp);
GXP(colorFragmentProgramGxp);

static const SceGxmBlendInfo blendInfoOpaque = {
	.colorMask = SCE_GXM_COLOR_MASK_ALL,
	.colorFunc = SCE_GXM_BLEND_FUNC_NONE,
	.alphaFunc = SCE_GXM_BLEND_FUNC_NONE,
	.colorSrc = SCE_GXM_BLEND_FACTOR_ZERO,
	.colorDst = SCE_GXM_BLEND_FACTOR_ZERO,
	.alphaSrc = SCE_GXM_BLEND_FACTOR_ZERO,
	.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO,
};

static const SceGxmBlendInfo blendInfoTransparent = {
	.colorMask = SCE_GXM_COLOR_MASK_ALL,
	.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
	.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
	.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
	.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE,
	.alphaDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
};
