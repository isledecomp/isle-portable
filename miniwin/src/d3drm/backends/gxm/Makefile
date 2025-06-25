shaders += main.frag.gxp main.vert.gxp
shaders += clear.frag.gxp clear.vert.gxp
shaders += blit.color.frag.gxp blit.tex.frag.gxp blit.vert.gxp

all: $(shaders)

%.vert.gxp: %.vert.cg
	./psp2cgc.exe -Wperf -profile sce_vp_psp2 $< -o $@

%.frag.gxp: %.frag.cg
	./psp2cgc.exe -Wperf -profile sce_fp_psp2 $< -o $@
