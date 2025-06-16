#include "d3drmrenderer_opengles2.h"
#include "meshutils.h"

#include <GLES2/gl2.h>
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
		return nullptr;
	}

	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	// Setup FBO
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Create color texture
	GLuint colorTex;
	glGenTextures(1, &colorTex);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

	// Create depth renderbuffer
	GLuint depthRb;
	glGenRenderbuffers(1, &depthRb);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		return nullptr;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

	return new OpenGLES2Renderer(width, height, context, fbo, colorTex, depthRb, shaderProgram);
}

OpenGLES2Renderer::OpenGLES2Renderer(
	DWORD width,
	DWORD height,
	SDL_GLContext context,
	GLuint fbo,
	GLuint colorTex,
	GLuint depthRb,
	GLuint shaderProgram
)
	: m_width(width), m_height(height), m_context(context), m_fbo(fbo), m_colorTex(colorTex), m_depthRb(depthRb),
	  m_shaderProgram(shaderProgram)
{
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_ABGR8888);
}

OpenGLES2Renderer::~OpenGLES2Renderer()
{
	SDL_DestroySurface(m_renderedImage);
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteRenderbuffers(1, &m_depthRb);
	glDeleteProgram(m_shaderProgram);
	glDeleteTextures(1, &m_colorTex);
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
	m_projection[1][1] *= -1.0f; // OpenGL is upside down
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

Uint32 OpenGLES2Renderer::GetTextureId(IDirect3DRMTexture* iTexture)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				glDeleteTextures(1, &tex.glTextureId);
				glGenTextures(1, &tex.glTextureId);
				glBindTexture(GL_TEXTURE_2D, tex.glTextureId);

				SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_ABGR8888);
				if (!surf) {
					return NO_TEXTURE_ID;
				}
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
				SDL_DestroySurface(surf);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				tex.version = texture->m_version;
			}
			return i;
		}
	}

	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	SDL_Surface* surf = SDL_ConvertSurface(surface->m_surface, SDL_PIXELFORMAT_ABGR8888);
	if (!surf) {
		return NO_TEXTURE_ID;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
	SDL_DestroySurface(surf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (!tex.texture) {
			tex.texture = texture;
			tex.version = texture->m_version;
			tex.glTextureId = texId;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back({texture, texture->m_version, texId});
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

GLES2MeshCacheEntry GLES2UploadMesh(const MeshGroup& meshGroup)
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
			meshGroup.texture != nullptr,
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
	if (meshGroup.texture) {
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

	if (meshGroup.texture) {
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

DWORD OpenGLES2Renderer::GetWidth()
{
	return m_width;
}

DWORD OpenGLES2Renderer::GetHeight()
{
	return m_height;
}

void OpenGLES2Renderer::GetDesc(D3DDEVICEDESC* halDesc, D3DDEVICEDESC* helDesc)
{
	halDesc->dcmColorModel = D3DCOLORMODEL::RGB;
	halDesc->dwFlags = D3DDD_DEVICEZBUFFERBITDEPTH;
	halDesc->dwDeviceZBufferBitDepth = DDBD_16;
	helDesc->dwDeviceRenderBitDepth = DDBD_32;
	halDesc->dpcTriCaps.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE;
	halDesc->dpcTriCaps.dwShadeCaps = D3DPSHADECAPS_ALPHAFLATBLEND;
	halDesc->dpcTriCaps.dwTextureFilterCaps = D3DPTFILTERCAPS_LINEAR;

	memset(helDesc, 0, sizeof(D3DDEVICEDESC));
}

const char* OpenGLES2Renderer::GetName()
{
	return "OpenGL ES 2.0 HAL";
}

HRESULT OpenGLES2Renderer::BeginFrame()
{
	SDL_GL_MakeCurrent(DDWindow, m_context);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	glUseProgram(m_shaderProgram);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
		std::string base = "u_lights[" + std::to_string(i) + "]";
		glUniform4fv(glGetUniformLocation(m_shaderProgram, (base + ".color").c_str()), 1, lightData[i].color);
		glUniform4fv(glGetUniformLocation(m_shaderProgram, (base + ".position").c_str()), 1, lightData[i].position);
		glUniform4fv(glGetUniformLocation(m_shaderProgram, (base + ".direction").c_str()), 1, lightData[i].direction);
	}
	glUniform1i(glGetUniformLocation(m_shaderProgram, "u_lightCount"), lightCount);

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
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	auto& mesh = m_meshs[meshId];

	glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "u_modelViewMatrix"), 1, GL_FALSE, &modelViewMatrix[0][0]);
	glUniformMatrix3fv(glGetUniformLocation(m_shaderProgram, "u_normalMatrix"), 1, GL_FALSE, &normalMatrix[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "u_projectionMatrix"), 1, GL_FALSE, &m_projection[0][0]);

	glUniform4f(
		glGetUniformLocation(m_shaderProgram, "u_color"),
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	);

	glUniform1f(glGetUniformLocation(m_shaderProgram, "u_shininess"), appearance.shininess);

	if (appearance.textureId != NO_TEXTURE_ID) {
		glUniform1i(glGetUniformLocation(m_shaderProgram, "u_useTexture"), 1);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_textures[appearance.textureId].glTextureId);
		glUniform1i(glGetUniformLocation(m_shaderProgram, "u_texture"), 0);
	}
	else {
		glUniform1i(glGetUniformLocation(m_shaderProgram, "u_useTexture"), 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, mesh.vboPositions);
	GLint posLoc = glGetAttribLocation(m_shaderProgram, "a_position");
	glEnableVertexAttribArray(posLoc);
	glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glBindBuffer(GL_ARRAY_BUFFER, mesh.vboNormals);
	GLint normLoc = glGetAttribLocation(m_shaderProgram, "a_normal");
	glEnableVertexAttribArray(normLoc);
	glVertexAttribPointer(normLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	GLint texLoc = glGetAttribLocation(m_shaderProgram, "a_texCoord");
	if (appearance.textureId != NO_TEXTURE_ID) {
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vboTexcoords);
		glEnableVertexAttribArray(texLoc);
		glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, nullptr);

	glDisableVertexAttribArray(posLoc);
	glDisableVertexAttribArray(normLoc);
	glDisableVertexAttribArray(texLoc);
}

HRESULT OpenGLES2Renderer::FinalizeFrame()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);

	glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_renderedImage->pixels);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Composite onto SDL backbuffer
	SDL_BlitSurface(m_renderedImage, nullptr, DDBackBuffer, nullptr);

	return DD_OK;
}
