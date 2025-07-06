#pragma once

#include <psp2/gxm.h>
#include "tlsf.h"

#define GXM_DISPLAY_BUFFER_COUNT 3

typedef struct Vertex2D {
	float position[2];
    float texCoord[2];
} Vertex2D;

typedef struct GXMContext {
	// context
	SceUID vdmRingBufferUid;
	SceUID vertexRingBufferUid;
	SceUID fragmentRingBufferUid;
	SceUID fragmentUsseRingBufferUid;
	size_t fragmentUsseRingBufferOffset;

	void* vdmRingBuffer;
	void* vertexRingBuffer;
	void* fragmentRingBuffer;
	void* fragmentUsseRingBuffer;

	void* contextHostMem;
	SceGxmContext* context;

	// shader patcher
	SceUID patcherBufferUid;
	void* patcherBuffer;

	SceUID patcherVertexUsseUid;
	size_t patcherVertexUsseOffset;
	void* patcherVertexUsse;
	
	SceUID patcherFragmentUsseUid;
	size_t patcherFragmentUsseOffset;
    void* patcherFragmentUsse;

	SceGxmShaderPatcher* shaderPatcher;

	// clear
	SceGxmShaderPatcherId planeVertexProgramId;
	SceGxmShaderPatcherId colorFragmentProgramId;
	SceGxmShaderPatcherId imageFragmentProgramId;
	SceGxmVertexProgram* planeVertexProgram;
	SceGxmFragmentProgram* colorFragmentProgram;
	SceGxmFragmentProgram* imageFragmentProgram;
	const SceGxmProgramParameter* color_uColor;
	Vertex2D* clearVertices;
	uint16_t* clearIndices;

	// display
	SceGxmRenderTarget* renderTarget;
	void* displayBuffers[GXM_DISPLAY_BUFFER_COUNT];
	SceUID displayBuffersUid[GXM_DISPLAY_BUFFER_COUNT];
	SceGxmColorSurface displayBuffersSurface[GXM_DISPLAY_BUFFER_COUNT];
	SceGxmSyncObject* displayBuffersSync[GXM_DISPLAY_BUFFER_COUNT];
	int backBufferIndex = 0;
	int frontBufferIndex = 1;

	// depth buffer
	SceUID depthBufferUid;
	void* depthBufferData;
	SceUID stencilBufferUid;
	void* stencilBufferData;
	SceGxmDepthStencilSurface depthSurface;

	// allocator
	SceUID cdramUID;
	void* cdramMem;
	tlsf_t cdramPool;

	bool sceneStarted;

	void swap_display();
	void copy_frontbuffer();
	int init();
	void destroy();
	void clear(float r, float g, float b, bool new_scene);
	void* alloc(size_t size, size_t align);
	void free(void* ptr);
} GXMContext;

// global so that common dialog can be rendererd without GXMRenderer
extern GXMContext* gxm;
