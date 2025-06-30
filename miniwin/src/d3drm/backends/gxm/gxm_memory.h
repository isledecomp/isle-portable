#pragma once

#include <SDL3/SDL_stdinc.h>
#include <psp2/types.h>

void* patcher_host_alloc(void* user_data, unsigned int size);
void patcher_host_free(void* user_data, void* mem);

void* vita_mem_alloc(unsigned int type, size_t size, size_t alignment, int attribs, SceUID* uid);
void vita_mem_free(SceUID uid);

void* vita_mem_vertex_usse_alloc(unsigned int size, SceUID* uid, unsigned int* usse_offset);
void vita_mem_vertex_usse_free(SceUID uid);
void* vita_mem_fragment_usse_alloc(unsigned int size, SceUID* uid, unsigned int* usse_offset);
void vita_mem_fragment_usse_free(SceUID uid);

bool cdram_allocator_create();
void* cdram_alloc(size_t size, size_t align);
void cdram_free(void* ptr);
