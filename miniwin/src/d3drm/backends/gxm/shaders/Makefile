shaders += main.frag.gxp main.vert.gxp
shaders += color.frag.gxp image.frag.gxp

all: $(shaders)

CGCFLAGS = -Wperf -cache -cachedir cache

cache:
	mkdir -p cache

%.vert.gxp: %.vert.cg | cache
	./psp2cgc.exe $(CGCFLAGS) -profile sce_vp_psp2 $< -o $@

%.frag.gxp: %.frag.cg | cache
	./psp2cgc.exe $(CGCFLAGS) -profile sce_fp_psp2 $< -o $@
