shaders += main.frag.gxp main.vert.gxp image.frag.gxp
shaders += clear.frag.gxp clear.vert.gxp

all: $(shaders)

%.vert.gxp: %.vert.cg
	./psp2cgc.exe -Wperf -profile sce_vp_psp2 $< -o $@

%.frag.gxp: %.frag.cg
	./psp2cgc.exe -Wperf -profile sce_fp_psp2 $< -o $@
