#include <paf.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>

char sceUserMainThreadName[] = "isle_config";
int sceUserMainThreadPriority = 0x10000100;
int sceUserMainThreadCpuAffinityMask = 0x70000;
SceSize sceUserMainThreadStackSize = 0x4000;

void operator delete(void* ptr, unsigned int n)
{
	return sce_paf_free(ptr);
}

extern "C"
{
	void user_malloc_init()
	{
	}

	void user_malloc_finalize(void)
	{
	}

	void* user_malloc(size_t size)
	{
		return sce_paf_malloc(size);
	}

	void user_free(void* ptr)
	{
		sce_paf_free(ptr);
	}

	void* user_calloc(size_t nelem, size_t size)
	{
		return sce_paf_calloc(nelem, size);
	}

	void* user_realloc(void* ptr, size_t new_size)
	{
		return sce_paf_realloc(ptr, new_size);
	}

	void* user_memalign(size_t boundary, size_t size)
	{
		return sce_paf_memalign(boundary, size);
	}

	void* user_reallocalign(void* ptr, size_t size, size_t boundary)
	{
		sceClibPrintf("[PAF2LIBC] reallocalign is not supported\n");
		abort();
		return NULL;
	}

	int user_malloc_stats(struct malloc_managed_size* mmsize)
	{
		sceClibPrintf("malloc_stats\n");
		abort();
		return 0;
	}

	int user_malloc_stats_fast(struct malloc_managed_size* mmsize)
	{
		sceClibPrintf("user_malloc_stats_fast\n");
		abort();
		return 0;
	}

	size_t user_malloc_usable_size(void* ptr)
	{
		return sce_paf_musable_size(ptr);
	}
}

void* user_new(std::size_t size)
{
	return sce_paf_malloc(size);
}

void* user_new(std::size_t size, const std::nothrow_t& x)
{
	return sce_paf_malloc(size);
}

void* user_new_array(std::size_t size)
{
	return sce_paf_malloc(size);
}

void* user_new_array(std::size_t size, const std::nothrow_t& x)
{
	return sce_paf_malloc(size);
}

void user_delete(void* ptr)
{
	sce_paf_free(ptr);
}

void user_delete(void* ptr, const std::nothrow_t& x)
{
	sce_paf_free(ptr);
}

void user_delete_array(void* ptr)
{
	sce_paf_free(ptr);
}

void user_delete_array(void* ptr, const std::nothrow_t& x)
{
	sce_paf_free(ptr);
}

int paf_init()
{
	int load_res;
	ScePafInit init_param;
	SceSysmoduleOpt sysmodule_opt;

	init_param.global_heap_size = 0x1000000;
	init_param.cdlg_mode = 0;
	init_param.global_heap_alignment = 0;
	init_param.global_heap_disable_assert_on_alloc_failure = 0;

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
		return -1;
	}
	return 0;
}

int main();

extern "C" int module_start(SceSize args, void* argp)
{
	int res = paf_init();
	if (res < 0) {
		sceKernelExitProcess(res);
		return SCE_KERNEL_START_FAILED;
	}

	res = main();
	sceKernelExitProcess(res);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" void _start()
{
	module_start(0, nullptr);
}
