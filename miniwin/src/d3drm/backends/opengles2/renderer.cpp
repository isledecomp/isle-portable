#include "d3drmrenderer_opengles2.h"
#include "meshutils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <string>

static GLuint CompileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glDeleteShader(shader);
		SDL_Log("CompileShader (%s)", SDL_GetError());
		return 0;
	}
	return shader;
}

struct SceneLightGLES2 {
	float color[4];
	float position[4];
	float direction[4];
};

Direct3DRMRenderer* OpenGLES2Renderer::Create(DWORD width, DWORD height)
{
	// We have to reset the attributes here after having enumerated the
	// OpenGL ES 2.0 renderer, or else SDL gets very confused by SDL_GL_DEPTH_SIZE
	// call below when on an EGL-based backend, and crashes with EGL_BAD_MATCH.
	SDL_GL_ResetAttributes();
	// But ResetAttributes resets it to 16.
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_Window* window = DDWindow;
	bool testWindow = false;
	if (!window) {
		window = SDL_CreateWindow("OpenGL ES 2.0 test", width, height, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
		testWindow = true;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	if (!SDL_GL_MakeCurrent(window, context)) {
		if (testWindow) {
			SDL_DestroyWindow(window);
		}
		return nullptr;
	}

	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	const char* vertexShaderSource = R"(
		attribute vec3 a_position;
		attribute vec3 a_normal;
		attribute vec2 a_texCoord;

		uniform mat4 u_modelViewMatrix;
		uniform mat3 u_normalMatrix;
		uniform mat4 u_projectionMatrix;

		varying vec3 v_viewPos;
		varying vec3 v_normal;
		varying vec2 v_texCoord;

		void main() {
			vec4 viewPos = u_modelViewMatrix * vec4(a_position, 1.0);
			gl_Position = u_projectionMatrix * viewPos;
			v_viewPos = viewPos.xyz;
			v_normal = normalize(u_normalMatrix * a_normal);
			v_texCoord = a_texCoord;
		}
	)";

	const char* fragmentShaderSource = R"(
		precision mediump float;

		struct SceneLight {
			vec4 color;
			vec4 position;
			vec4 direction;
		};

		uniform SceneLight u_lights[3];
		uniform int u_lightCount;

		varying vec3 v_viewPos;
		varying vec3 v_normal;
		varying vec2 v_texCoord;

		uniform float u_shininess;
		uniform vec4 u_color;
		uniform int u_useTexture;
		uniform sampler2D u_texture;

		void main() {
			vec3 diffuse = vec3(0.0);
			vec3 specular = vec3(0.0);

			for (int i = 0; i < 3; ++i) {
				if (i >= u_lightCount) break;

				vec3 lightColor = u_lights[i].color.rgb;

				if (u_lights[i].position.w == 0.0 && u_lights[i].direction.w == 0.0) {
					diffuse += lightColor;
					continue;
				}

				vec3 lightVec;
				if (u_lights[i].direction.w == 1.0) {
					lightVec = -normalize(u_lights[i].direction.xyz);
				}
				else {
					lightVec = u_lights[i].position.xyz - v_viewPos;
				}
				lightVec = normalize(lightVec);

				float dotNL = max(dot(v_normal, lightVec), 0.0);
				if (dotNL > 0.0) {
					// Diffuse contribution
					diffuse += dotNL * lightColor;

					// Specular
					if (u_shininess > 0.0 && u_lights[i].direction.w == 1.0) {
						vec3 viewVec = normalize(-v_viewPos); // Assuming camera at origin
						vec3 H = normalize(lightVec + viewVec);
						float dotNH = max(dot(v_normal, H), 0.0);
						float spec = pow(dotNH, u_shininess);
						specular += spec * lightColor;
					}
				}
			}

			vec4 finalColor = u_color;
			finalColor.rgb = clamp(diffuse * u_color.rgb + specular, 0.0, 1.0);
			if (u_useTexture != 0) {
				vec4 texel = texture2D(u_texture, v_texCoord);
				finalColor.rgb = clamp(texel.rgb * finalColor.rgb, 0.0, 1.0);
				finalColor.a = texel.a;
			}

			gl_FragColor = finalColor;
		}
	)";

	GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vs);
	glAttachShader(shaderProgram, fs);
	glBindAttribLocation(shaderProgram, 0, "a_position");
	glBindAttribLocation(shaderProgram, 1, "a_normal");
	glBindAttribLocation(shaderProgram, 2, "a_texCoord");
	glLinkProgram(shaderProgram);
	glDeleteShader(vs);
	glDeleteShader(fs);

	if (testWindow) {
		SDL_DestroyWindow(window);
	}

	return new OpenGLES2Renderer(width, height, context, shaderProgram);
}

GLES2MeshCacheEntry GLES2UploadMesh(const MeshGroup& meshGroup, bool forceUV = false)
{
	GLES2MeshCacheEntry cache{&meshGroup, meshGroup.version};

	cache.flat = meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT;

	std::vector<D3DRMVERTEX> vertices;
	if (cache.flat) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr || forceUV,
			vertices,
			cache.indices
		);
	}
	else {
		vertices = meshGroup.vertices;
		cache.indices.resize(meshGroup.indices.size());
		std::transform(meshGroup.indices.begin(), meshGroup.indices.end(), cache.indices.begin(), [](DWORD index) {
			return static_cast<uint16_t>(index);
		});
	}

	std::vector<TexCoord> texcoords;
	if (meshGroup.texture || forceUV) {
		texcoords.resize(vertices.size());
		std::transform(vertices.begin(), vertices.end(), texcoords.begin(), [](const D3DRMVERTEX& v) {
			return v.texCoord;
		});
	}
	std::vector<D3DVECTOR> positions(vertices.size());
	std::transform(vertices.begin(), vertices.end(), positions.begin(), [](const D3DRMVERTEX& v) {
		return v.position;
	});
	std::vector<D3DVECTOR> normals(vertices.size());
	std::transform(vertices.begin(), vertices.end(), normals.begin(), [](const D3DRMVERTEX& v) { return v.normal; });

	glGenBuffers(1, &cache.vboPositions);
	glBindBuffer(GL_ARRAY_BUFFER, cache.vboPositions);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(D3DVECTOR), positions.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &cache.vboNormals);
	glBindBuffer(GL_ARRAY_BUFFER, cache.vboNormals);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(D3DVECTOR), normals.data(), GL_STATIC_DRAW);

	if (meshGroup.texture || forceUV) {
		glGenBuffers(1, &cache.vboTexcoords);
		glBindBuffer(GL_ARRAY_BUFFER, cache.vboTexcoords);
		glBufferData(GL_ARRAY_BUFFER, texcoords.size() * sizeof(TexCoord), texcoords.data(), GL_STATIC_DRAW);
	}

	glGenBuffers(1, &cache.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cache.ibo);
	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		cache.indices.size() * sizeof(cache.indices[0]),
		cache.indices.data(),
		GL_STATIC_DRAW
	);

	return cache;
}

OpenGLES2Renderer::OpenGLES2Renderer(DWORD width, DWORD height, SDL_GLContext context, GLuint shaderProgram)
	: m_context(context), m_shaderProgram(shaderProgram)
{
	m_virtualWidth = width;
	m_virtualHeight = height;
	ViewportTransform viewportTransform = {1.0f, 0.0f, 0.0f};
	Resize(width, height, viewportTransform);

	m_uiMesh.vertices = {
		{{0.0f, 0.0f, 0.0f}, {0, 0, -1}, {0.0f, 0.0f}},
		{{1.0f, 0.0f, 0.0f}, {0, 0, -1}, {1.0f, 0.0f}},
		{{1.0f, 1.0f, 0.0f}, {0, 0, -1}, {1.0f, 1.0f}},
		{{0.0f, 1.0f, 0.0f}, {0, 0, -1}, {0.0f, 1.0f}}
	};
	m_uiMesh.indices = {0, 1, 2, 0, 2, 3};
	m_uiMeshCache = GLES2UploadMesh(m_uiMesh, true);
	m_posLoc = glGetAttribLocation(m_shaderProgram, "a_position");
	m_normLoc = glGetAttribLocation(m_shaderProgram, "a_normal");
	m_texLoc = glGetAttribLocation(m_shaderProgram, "a_texCoord");
	m_colorLoc = glGetUniformLocation(m_shaderProgram, "u_color");
	m_shinLoc = glGetUniformLocation(m_shaderProgram, "u_shininess");
	m_lightCountLoc = glGetUniformLocation(m_shaderProgram, "u_lightCount");
	m_useTextureLoc = glGetUniformLocation(m_shaderProgram, "u_useTexture");
	m_textureLoc = glGetUniformLocation(m_shaderProgram, "u_texture");
	for (int i = 0; i < 3; ++i) {
		std::string base = "u_lights[" + std::to_string(i) + "]";
		u_lightLocs[i][0] = glGetUniformLocation(m_shaderProgram, (base + ".color").c_str());
		u_lightLocs[i][1] = glGetUniformLocation(m_shaderProgram, (base + ".position").c_str());
		u_lightLocs[i][2] = glGetUniformLocation(m_shaderProgram, (base + ".direction").c_str());
	}
	m_modelViewMatrixLoc = glGetUniformLocation(m_shaderProgram, "u_modelViewMatrix");
	m_normalMatrixLoc = glGetUniformLocation(m_shaderProgram, "u_normalMatrix");
	m_projectionMatrixLoc = glGetUniformLocation(m_shaderProgram, "u_projectionMatrix");
}

OpenGLES2Renderer::~OpenGLES2Renderer()
{
	SDL_DestroySurface(m_renderedImage);
	glDeleteProgram(m_shaderProgram);
}

void OpenGLES2Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	if (count > 3) {
		SDL_Log("Unsupported number of lights (%d)", static_cast<int>(count));
		count = 3;
	}

	m_lights.assign(lightsArray, lightsArray + count);
}

void OpenGLES2Renderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

void OpenGLES2Renderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

struct TextureDestroyContextGLS2 {
	OpenGLES2Renderer* renderer;
	Uint32 textureId;
};

void OpenGLES2Renderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	auto* ctx = new TextureDestroyContextGLS2{this, id};
	texture->AddDestroyCallback(
		[](IDirect3DRMObject* obj, void* arg) {
			auto* ctx = static_cast<TextureDestroyContextGLS2*>(arg);
			auto& cache = ctx->renderer->m_textures[ctx->textureId];
			if (cache.glTextureId != 0) {
				glDeleteTextures(1, &cache.glTextureId);
				cache.glTextureId = 0;
				cache.texture = nullptr;
			}
			delete ctx;
		},
		ctx
	);
}

bool UploadTexture(SDL_Surface* source, GLuint& outTexId, bool isUi)
{
	SDL_Surface* surf = source;
	if (source->format != SDL_PIXELFORMAT_RGBA32) {
		surf = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
		if (!surf) {
			return false;
		}
	}

	glGenTextures(1, &outTexId);
	glBindTexture(GL_TEXTURE_2D, outTexId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);

	if (isUi) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (strstr((const char*) glGetString(GL_EXTENSIONS), "GL_EXT_texture_filter_anisotropic")) {
			GLfloat maxAniso = 0.0f;
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
			GLfloat desiredAniso = fminf(8.0f, maxAniso);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, desiredAniso);
		}
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	if (surf != source) {
		SDL_DestroySurface(surf);
	}

	return true;
}

Uint32 OpenGLES2Renderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUi)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				glDeleteTextures(1, &tex.glTextureId);
				if (UploadTexture(surface->m_surface, tex.glTextureId, isUi)) {
					tex.version = texture->m_version;
				}
			}
			return i;
		}
	}

	GLuint texId;
	if (!UploadTexture(surface->m_surface, texId, isUi)) {
		return NO_TEXTURE_ID;
	}

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.glTextureId = texId;
			tex.width = surface->m_surface->w;
			tex.height = surface->m_surface->h;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back(
		{texture, texture->m_version, texId, (uint16_t) surface->m_surface->w, (uint16_t) surface->m_surface->h}
	);
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

struct GLES2MeshDestroyContext {
	OpenGLES2Renderer* renderer;
	Uint32 id;
};

void OpenGLES2Renderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	auto* ctx = new GLES2MeshDestroyContext{this, id};
	mesh->AddDestroyCallback(
		[](IDirect3DRMObject*, void* arg) {
			auto* ctx = static_cast<GLES2MeshDestroyContext*>(arg);
			auto& cache = ctx->renderer->m_meshs[ctx->id];
			cache.meshGroup = nullptr;
			glDeleteBuffers(1, &cache.vboPositions);
			glDeleteBuffers(1, &cache.vboNormals);
			glDeleteBuffers(1, &cache.vboTexcoords);
			glDeleteBuffers(1, &cache.ibo);
			delete ctx;
		},
		ctx
	);
}

Uint32 OpenGLES2Renderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				cache = std::move(GLES2UploadMesh(*meshGroup));
			}
			return i;
		}
	}

	auto newCache = GLES2UploadMesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (!cache.meshGroup) {
			cache = std::move(newCache);
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshs.push_back(std::move(newCache));
	AddMeshDestroyCallback((Uint32) (m_meshs.size() - 1), mesh);
	return (Uint32) (m_meshs.size() - 1);
}

HRESULT OpenGLES2Renderer::BeginFrame()
{
	m_dirty = true;

	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	glUseProgram(m_shaderProgram);

	SceneLightGLES2 lightData[3];
	int lightCount = std::min(static_cast<int>(m_lights.size()), 3);

	for (int i = 0; i < lightCount; ++i) {
		const auto& src = m_lights[i];
		lightData[i].color[0] = src.color.r;
		lightData[i].color[1] = src.color.g;
		lightData[i].color[2] = src.color.b;
		lightData[i].color[3] = src.color.a;

		lightData[i].position[0] = src.position.x;
		lightData[i].position[1] = src.position.y;
		lightData[i].position[2] = src.position.z;
		lightData[i].position[3] = src.positional;

		lightData[i].direction[0] = src.direction.x;
		lightData[i].direction[1] = src.direction.y;
		lightData[i].direction[2] = src.direction.z;
		lightData[i].direction[3] = src.directional;
	}

	for (int i = 0; i < lightCount; ++i) {
		glUniform4fv(u_lightLocs[i][0], 1, lightData[i].color);
		glUniform4fv(u_lightLocs[i][1], 1, lightData[i].position);
		glUniform4fv(u_lightLocs[i][2], 1, lightData[i].direction);
	}
	glUniform1i(m_lightCountLoc, lightCount);
	return DD_OK;
}

void OpenGLES2Renderer::EnableTransparency()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
}

void OpenGLES2Renderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	auto& mesh = m_meshs[meshId];

	glUniformMatrix4fv(m_modelViewMatrixLoc, 1, GL_FALSE, &modelViewMatrix[0][0]);
	glUniformMatrix3fv(m_normalMatrixLoc, 1, GL_FALSE, &normalMatrix[0][0]);
	glUniformMatrix4fv(m_projectionMatrixLoc, 1, GL_FALSE, &m_projection[0][0]);

	glUniform4f(
		m_colorLoc,
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	);

	glUniform1f(m_shinLoc, appearance.shininess);

	if (appearance.textureId != NO_TEXTURE_ID) {
		glUniform1i(m_useTextureLoc, 1);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_textures[appearance.textureId].glTextureId);
		glUniform1i(m_textureLoc, 0);
	}
	else {
		glUniform1i(m_useTextureLoc, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, mesh.vboPositions);
	glEnableVertexAttribArray(m_posLoc);
	glVertexAttribPointer(m_posLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glBindBuffer(GL_ARRAY_BUFFER, mesh.vboNormals);
	glEnableVertexAttribArray(m_normLoc);
	glVertexAttribPointer(m_normLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	if (appearance.textureId != NO_TEXTURE_ID) {
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vboTexcoords);
		glEnableVertexAttribArray(m_texLoc);
		glVertexAttribPointer(m_texLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, nullptr);

	glDisableVertexAttribArray(m_normLoc);
	glDisableVertexAttribArray(m_texLoc);
}

HRESULT OpenGLES2Renderer::FinalizeFrame()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);

	return DD_OK;
}

void OpenGLES2Renderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
	if (m_renderedImage) {
		SDL_DestroySurface(m_renderedImage);
	}
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_RGBA32);
	glViewport(0, 0, m_width, m_height);
}

void OpenGLES2Renderer::Clear(float r, float g, float b)
{
	m_dirty = true;
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearColor(r, g, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLES2Renderer::Flip()
{
	if (m_dirty) {
		SDL_GL_SwapWindow(DDWindow);
		m_dirty = false;
	}
}

void OpenGLES2Renderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect)
{
	m_dirty = true;

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glUseProgram(m_shaderProgram);

	float color[] = {1.0f, 1.0f, 1.0f, 1.0f};
	float blank[] = {0.0f, 0.0f, 0.0f, 0.0f};
	glUniform4fv(u_lightLocs[0][0], 1, color);
	glUniform4fv(u_lightLocs[0][1], 1, blank);
	glUniform4fv(u_lightLocs[0][2], 1, blank);
	glUniform1i(m_lightCountLoc, 1);

	glUniform4f(m_colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
	glUniform1f(m_shinLoc, 0.0f);

	const GLES2TextureCacheEntry& texture = m_textures[textureId];
	float scaleX = static_cast<float>(dstRect.w) / srcRect.w;
	float scaleY = static_cast<float>(dstRect.h) / srcRect.h;
	SDL_Rect expandedDstRect = {
		static_cast<int>(std::round(dstRect.x - srcRect.x * scaleX)),
		static_cast<int>(std::round(dstRect.y - srcRect.y * scaleY)),
		static_cast<int>(std::round(texture.width * scaleX)),
		static_cast<int>(std::round(texture.height * scaleY))
	};

	D3DRMMATRIX4D modelView, projection;
	Create2DTransformMatrix(
		expandedDstRect,
		m_viewportTransform.scale,
		m_viewportTransform.offsetX,
		m_viewportTransform.offsetY,
		modelView
	);

	glUniformMatrix4fv(m_modelViewMatrixLoc, 1, GL_FALSE, &modelView[0][0]);
	Matrix3x3 identity = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
	glUniformMatrix3fv(m_normalMatrixLoc, 1, GL_FALSE, &identity[0][0]);
	CreateOrthographicProjection((float) m_width, (float) m_height, projection);
	glUniformMatrix4fv(m_projectionMatrixLoc, 1, GL_FALSE, &projection[0][0]);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(m_useTextureLoc, 1);
	glBindTexture(GL_TEXTURE_2D, texture.glTextureId);
	glUniform1i(m_textureLoc, 0);

	glEnable(GL_SCISSOR_TEST);
	glScissor(
		static_cast<int>(std::round(dstRect.x * m_viewportTransform.scale + m_viewportTransform.offsetX)),
		m_height - static_cast<int>(
					   std::round((dstRect.y + dstRect.h) * m_viewportTransform.scale + m_viewportTransform.offsetY)
				   ),
		static_cast<int>(std::round(dstRect.w * m_viewportTransform.scale)),
		static_cast<int>(std::round(dstRect.h * m_viewportTransform.scale))
	);

	glBindBuffer(GL_ARRAY_BUFFER, m_uiMeshCache.vboPositions);
	glEnableVertexAttribArray(m_posLoc);
	glVertexAttribPointer(m_posLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glBindBuffer(GL_ARRAY_BUFFER, m_uiMeshCache.vboTexcoords);
	glEnableVertexAttribArray(m_texLoc);
	glVertexAttribPointer(m_texLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uiMeshCache.ibo);
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_uiMeshCache.indices.size()), GL_UNSIGNED_SHORT, nullptr);

	glDisableVertexAttribArray(m_texLoc);
	glDisable(GL_SCISSOR_TEST);
}

void OpenGLES2Renderer::Download(SDL_Surface* target)
{
	glFinish();
	glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_renderedImage->pixels);

	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};

	SDL_Surface* bufferClone = SDL_CreateSurface(target->w, target->h, SDL_PIXELFORMAT_RGBA32);
	if (!bufferClone) {
		SDL_Log("SDL_CreateSurface: %s", SDL_GetError());
		return;
	}

	SDL_BlitSurfaceScaled(m_renderedImage, &srcRect, bufferClone, nullptr, SDL_SCALEMODE_NEAREST);

	// Flip image vertically into target
	SDL_Rect rowSrc = {0, 0, bufferClone->w, 1};
	SDL_Rect rowDst = {0, 0, bufferClone->w, 1};
	for (int y = 0; y < bufferClone->h; ++y) {
		rowSrc.y = y;
		rowDst.y = bufferClone->h - 1 - y;
		SDL_BlitSurface(bufferClone, &rowSrc, target, &rowDst);
	}

	SDL_DestroySurface(bufferClone);
}
