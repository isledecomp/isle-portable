// This file cannot include any minwin headers.

#include "actual.h"

#include "structs.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cstring>
#include <vector>

// GL extension API functions.
bool g_useVBOs;
PFNGLGENBUFFERSPROC mwglGenBuffers;
PFNGLBINDBUFFERPROC mwglBindBuffer;
PFNGLBUFFERDATAPROC mwglBufferData;
PFNGLDELETEBUFFERSPROC mwglDeleteBuffers;

void GL11_InitState()
{
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
}

void GL11_LoadExtensions()
{
	g_useVBOs = SDL_GL_ExtensionSupported("GL_ARB_vertex_buffer_object");

	if (g_useVBOs) {
		// Load the required GL function pointers.
		mwglGenBuffers = (PFNGLGENBUFFERSPROC) SDL_GL_GetProcAddress("glGenBuffersARB");
		mwglBindBuffer = (PFNGLBINDBUFFERPROC) SDL_GL_GetProcAddress("glBindBufferARB");
		mwglBufferData = (PFNGLBUFFERDATAPROC) SDL_GL_GetProcAddress("glBufferDataARB");
		mwglDeleteBuffers = (PFNGLDELETEBUFFERSPROC) SDL_GL_GetProcAddress("glDeleteBuffersARB");
	}
}

void GL11_DestroyTexture(GLuint texId)
{
	glDeleteTextures(1, &texId);
}

GLuint GL11_UploadTextureData(void* pixels, int width, int height)
{
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	return texId;
}

void GL11_UploadMesh(GLMeshCacheEntry& cache, bool hasTexture)
{
	if (g_useVBOs) {
		mwglGenBuffers(1, &cache.vboPositions);
		mwglBindBuffer(GL_ARRAY_BUFFER_ARB, cache.vboPositions);
		mwglBufferData(
			GL_ARRAY_BUFFER_ARB,
			cache.positions.size() * sizeof(GL11_BridgeVector),
			cache.positions.data(),
			GL_STATIC_DRAW_ARB
		);

		mwglGenBuffers(1, &cache.vboNormals);
		mwglBindBuffer(GL_ARRAY_BUFFER_ARB, cache.vboNormals);
		mwglBufferData(
			GL_ARRAY_BUFFER_ARB,
			cache.normals.size() * sizeof(GL11_BridgeVector),
			cache.normals.data(),
			GL_STATIC_DRAW_ARB
		);

		if (hasTexture) {
			mwglGenBuffers(1, &cache.vboTexcoords);
			mwglBindBuffer(GL_ARRAY_BUFFER_ARB, cache.vboTexcoords);
			mwglBufferData(
				GL_ARRAY_BUFFER_ARB,
				cache.texcoords.size() * sizeof(GL11_BridgeTexCoord),
				cache.texcoords.data(),
				GL_STATIC_DRAW_ARB
			);
		}

		mwglGenBuffers(1, &cache.ibo);
		mwglBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, cache.ibo);
		mwglBufferData(
			GL_ELEMENT_ARRAY_BUFFER_ARB,
			cache.indices.size() * sizeof(cache.indices[0]),
			cache.indices.data(),
			GL_STATIC_DRAW_ARB
		);
	}
}

void GL11_DestroyMesh(GLMeshCacheEntry& cache)
{
	if (g_useVBOs) {
		mwglDeleteBuffers(1, &cache.vboPositions);
		mwglDeleteBuffers(1, &cache.vboNormals);
		mwglDeleteBuffers(1, &cache.vboTexcoords);
		mwglDeleteBuffers(1, &cache.ibo);
	}
}

void GL11_BeginFrame(const Matrix4x4* projection)
{
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	// Disable all lights and reset global ambient
	for (int i = 0; i < 8; ++i) {
		glDisable(GL_LIGHT0 + i);
	}
	const GLfloat zeroAmbient[4] = {0.f, 0.f, 0.f, 1.f};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zeroAmbient);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

	// Projection and view
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((const GLfloat*) projection);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void GL11_UploadLight(int lightIdx, GL11_BridgeSceneLight* l)
{
	// Setup light
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	GLenum lightId = GL_LIGHT0 + lightIdx++;
	const FColor& c = l->color;
	GLfloat col[4] = {c.r, c.g, c.b, c.a};
	const GLfloat zeroAmbient[4] = {0.f, 0.f, 0.f, 1.f};

	if (l->positional == 0.f && l->directional == 0.f) {
		// Ambient light only
		glLightfv(lightId, GL_AMBIENT, col);
		const GLfloat black[4] = {0.f, 0.f, 0.f, 1.f};
		glLightfv(lightId, GL_DIFFUSE, black);
		glLightfv(lightId, GL_SPECULAR, black);
		const GLfloat dummyPos[4] = {0.f, 0.f, 1.f, 0.f};
		glLightfv(lightId, GL_POSITION, dummyPos);
	}
	else {
		glLightfv(lightId, GL_AMBIENT, zeroAmbient);
		glLightfv(lightId, GL_DIFFUSE, col);
		if (l->directional == 1.0f) {
			glLightfv(lightId, GL_SPECULAR, col);
		}
		else {
			const GLfloat black[4] = {0.f, 0.f, 0.f, 1.f};
			glLightfv(lightId, GL_SPECULAR, black);
		}

		GLfloat pos[4];
		if (l->directional == 1.f) {
			pos[0] = -l->direction.x;
			pos[1] = -l->direction.y;
			pos[2] = -l->direction.z;
			pos[3] = 0.f;
		}
		else {
			pos[0] = l->position.x;
			pos[1] = l->position.y;
			pos[2] = l->position.z;
			pos[3] = 1.f;
		}
		glLightfv(lightId, GL_POSITION, pos);
	}
	glEnable(lightId);

	glPopMatrix();
}

void GL11_EnableTransparency()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
}

#define NO_TEXTURE_ID 0xffffffff

void GL11_SubmitDraw(
	GLMeshCacheEntry& mesh,
	const Matrix4x4& modelViewMatrix,
	const Appearance& appearance,
	GLuint texId
)
{
	glLoadMatrixf(&modelViewMatrix[0][0]);
	glEnable(GL_NORMALIZE);

	glColor4ub(appearance.color.r, appearance.color.g, appearance.color.b, appearance.color.a);

	if (appearance.shininess != 0.0f) {
		GLfloat whiteSpec[] = {1.f, 1.f, 1.f, 1.f};
		glMaterialfv(GL_FRONT, GL_SPECULAR, whiteSpec);
		glMaterialf(GL_FRONT, GL_SHININESS, appearance.shininess);
	}
	else {
		GLfloat noSpec[] = {0.0f, 0.0f, 0.0f, 0.0f};
		glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
		glMaterialf(GL_FRONT, GL_SHININESS, 0.0f);
	}

	if (mesh.flat) {
		glShadeModel(GL_FLAT);
	}
	else {
		glShadeModel(GL_SMOOTH);
	}

	// Bind texture if present
	if (appearance.textureId != NO_TEXTURE_ID) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texId);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else {
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	if (g_useVBOs) {
		mwglBindBuffer(GL_ARRAY_BUFFER_ARB, mesh.vboPositions);
		glVertexPointer(3, GL_FLOAT, 0, nullptr);

		mwglBindBuffer(GL_ARRAY_BUFFER_ARB, mesh.vboNormals);
		glNormalPointer(GL_FLOAT, 0, nullptr);

		if (appearance.textureId != NO_TEXTURE_ID) {
			mwglBindBuffer(GL_ARRAY_BUFFER_ARB, mesh.vboTexcoords);
			glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
		}

		mwglBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, mesh.ibo);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, nullptr);

		mwglBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
		mwglBindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}
	else {
		glVertexPointer(3, GL_FLOAT, 0, mesh.positions.data());
		glNormalPointer(GL_FLOAT, 0, mesh.normals.data());
		if (appearance.textureId != NO_TEXTURE_ID) {
			glTexCoordPointer(2, GL_FLOAT, 0, mesh.texcoords.data());
		}

		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, mesh.indices.data());
	}

	glPopMatrix();
}

void GL11_Resize(int width, int height)
{
	glViewport(0, 0, width, height);
}

void GL11_Clear(float r, float g, float b)
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearColor(r, g, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GL11_Draw2DImage(
	GLuint texId,
	const SDL_Rect& srcRect,
	const SDL_Rect& dstRect,
	float left,
	float right,
	float bottom,
	float top
)
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glOrtho(left, right, bottom, top, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_LIGHTING);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texId);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	GLint boundTexture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);

	GLfloat texW, texH;
	glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
	glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);

	float u1 = srcRect.x / texW;
	float v1 = srcRect.y / texH;
	float u2 = (srcRect.x + srcRect.w) / texW;
	float v2 = (srcRect.y + srcRect.h) / texH;

	float x1 = (float) dstRect.x;
	float y1 = (float) dstRect.y;
	float x2 = x1 + dstRect.w;
	float y2 = y1 + dstRect.h;

	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1);
	glVertex2f(x1, y1);
	glTexCoord2f(u2, v1);
	glVertex2f(x2, y1);
	glTexCoord2f(u2, v2);
	glVertex2f(x2, y2);
	glTexCoord2f(u1, v2);
	glVertex2f(x1, y2);
	glEnd();

	// Restore state
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

void GL11_Download(SDL_Surface* target)
{
	glFinish();
	glReadPixels(0, 0, target->w, target->h, GL_RGBA, GL_UNSIGNED_BYTE, target->pixels);
}
