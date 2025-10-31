.section .rodata, "a", %progbits

.macro embed file, symbol
.global _inc_\symbol
.align 16
_inc_\symbol:
    .incbin "\file"
.endm

embed "main.vert.gxp", mainVertexProgramGxp
embed "main.color.frag.gxp", mainColorFragmentProgramGxp
embed "main.texture.frag.gxp", mainTextureFragmentProgramGxp
embed "plane.vert.gxp", planeVertexProgramGxp
embed "image.frag.gxp", imageFragmentProgramGxp
embed "color.frag.gxp", colorFragmentProgramGxp
