shaders += plane.vert.gxp color.frag.gxp image.frag.gxp
shaders += main.vert.gxp main.color.frag.gxp main.texture.frag.gxp

PERFS = $(shaders:.gxp=.perf.txt)

all: $(shaders)
perfs: $(PERFS)

CGCFLAGS = -Wperf -cache -cachedir cache -W4 -Wsuppress=5206,5203

cache:
	mkdir -p cache

%.vert.gxp: %.vert.cg | cache
	./psp2cgc.exe $(CGCFLAGS) -profile sce_vp_psp2 $< -o $@

%.frag.gxp: %.frag.cg | cache
	./psp2cgc.exe $(CGCFLAGS) -profile sce_fp_psp2 $< -o $@

main.color.frag.gxp: main.frag.cg | cache
	./psp2cgc.exe $(CGCFLAGS) -profile sce_fp_psp2 $< -o $@

main.texture.frag.gxp: main.frag.cg | cache
	./psp2cgc.exe $(CGCFLAGS) -DTEXTURED=1 -profile sce_fp_psp2 $< -o $@

%.perf.txt: %.gxp
	./psp2shaderperf.exe -stats -symbols -disasm $< > $@
