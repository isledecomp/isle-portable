all: main.frag.gxp main.vert.gxp clear.frag.gxp clear.vert.gxp

%.vert.gxp: %.vert.cg
	./psp2cgc.exe -Wperf -profile sce_vp_psp2 $< -o $@

%.frag.gxp: %.frag.cg
	./psp2cgc.exe -Wperf -profile sce_fp_psp2 $< -o $@
