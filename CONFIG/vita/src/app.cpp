
#include <paf.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>

char sceUserMainThreadName[] = "paf_sample";
int sceUserMainThreadPriority = 0x10000100;
int sceUserMainThreadCpuAffinityMask = 0x70000;
SceSize sceUserMainThreadStackSize = 0x4000;

void operator delete(void* ptr, unsigned int n)
{
	return sce_paf_free(ptr);
}

int paf_main(void);

typedef struct _ScePafInit { // size is 0x18
	SceSize global_heap_size;
	int a2;
	int a3;
	int cdlg_mode;
	int heap_opt_param1;
	int heap_opt_param2;
} ScePafInit;

extern "C" int module_start(SceSize args, void* argp)
{
	int load_res;
	ScePafInit init_param;
	SceSysmoduleOpt sysmodule_opt;

	init_param.global_heap_size = 0x1000000;
	init_param.a2 = 0xEA60;
	init_param.a3 = 0x40000;
	init_param.cdlg_mode = 0;
	init_param.heap_opt_param1 = 0;
	init_param.heap_opt_param2 = 0;

	load_res = 0xDEADBEEF;
	sysmodule_opt.flags = 0;
	sysmodule_opt.result = &load_res;

	int res = sceSysmoduleLoadModuleInternalWithArg(
		SCE_SYSMODULE_INTERNAL_PAF,
		sizeof(init_param),
		&init_param,
		&sysmodule_opt
	);
	if ((res | load_res) != 0) {
		sceClibPrintf(
			"[PAF PRX Loader] Failed to load the PAF prx. (return value 0x%x, result code 0x%x )\n",
			res,
			load_res
		);
	}

	paf_main();

	return SCE_KERNEL_START_SUCCESS;
}

extern "C" void _start()
{
	module_start(0, nullptr);
}
