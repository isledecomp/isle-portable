#include "isledebug.h"

#include "isleapp.h"
#include "lego/sources/roi/legoroi.h"
#include "legobuildingmanager.h"
#include "legoentity.h"
#include "legogamestate.h"
#include "legoplantmanager.h"
#include "legosoundmanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "mxticklemanager.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

#ifdef ISLE_VALGRIND
#include <valgrind/callgrind.h>
#endif

#define SCANCODE_KEY_PAUSE SDL_SCANCODE_KP_0
#define SCANCODE_KEY_RESUME SDL_SCANCODE_KP_PERIOD

static bool g_debugEnabled;
static bool g_debugPaused;
static bool g_debugDoStep;
static SDL_Window* g_debugWindow;
static SDL_Renderer* g_debugRenderer;

static SDL_Texture* g_videoPalette;
static IDirect3DRMMiniwinDevice* g_d3drmMiniwinDevice;

#ifdef ISLE_VALGRIND
static bool g_instrumentationEnabled;
#endif

class DebugViewer {
public:
	static void InsidePlantManager()
	{
		LegoPlantManager* plantManager = Lego()->GetPlantManager();
		ImGui::Value("#plants", plantManager->GetNumPlants());
		ImGui::Value("#entries", plantManager->m_numEntries);
		if (plantManager->m_numEntries) {
			if (ImGui::BeginTable("Animated Entries", 4, ImGuiTableFlags_Borders)) {
				ImGui::TableSetupColumn("ROI Name");
				ImGui::TableSetupColumn("ROI m_sharedLodList");
				ImGui::TableSetupColumn("Entity Name");
				ImGui::TableSetupColumn("Time");
				ImGui::TableHeadersRow();
				for (MxS8 i = 0; i < plantManager->m_numEntries; i++) {
					const auto* entry = plantManager->m_entries[i];
					ImGui::TableNextRow();
					ImGui::Text("%s", entry->m_roi->m_name);
					ImGui::TableNextColumn();
					ImGui::Text("%d", entry->m_roi->m_sharedLodList);
					ImGui::TableNextColumn();
					ImGui::Text("%s", entry->m_roi->m_entity->ClassName());
					ImGui::TableNextColumn();
					ImGui::Text("%d", entry->m_time);
				}
				ImGui::EndTable();
			}
		}
	}
	static void InsideBuildingManager()
	{
		auto buildingManager = Lego()->GetBuildingManager();
		ImGui::Text("nextVariant: %u", buildingManager->m_nextVariant);
		ImGui::Text("m_boundariesDetermined: %d", buildingManager->m_boundariesDetermined);
		ImGui::Text("m_hideAfterAnimation: %d", buildingManager->m_hideAfterAnimation);
		ImGui::Text("#Animated Entries: %d", buildingManager->m_numEntries);
		if (buildingManager->m_numEntries) {
			if (ImGui::BeginTable("Animated Entries", 6, ImGuiTableFlags_Borders)) {
				ImGui::TableSetupColumn("ROI Name");
				ImGui::TableSetupColumn("ROI m_sharedLodList");
				ImGui::TableSetupColumn("Entity Name");
				ImGui::TableSetupColumn("Time");
				ImGui::TableSetupColumn("Y");
				ImGui::TableSetupColumn("Muted");
				ImGui::TableHeadersRow();
				for (MxS8 i = 0; i < buildingManager->m_numEntries; i++) {
					const auto* entry = buildingManager->m_entries[i];
					ImGui::TableNextRow();
					ImGui::Text("%s", entry->m_roi->m_name);
					ImGui::TableNextColumn();
					ImGui::Text("%d", entry->m_roi->m_sharedLodList);
					ImGui::TableNextColumn();
					ImGui::Text("%s", entry->m_roi->m_entity->ClassName());
					ImGui::TableNextColumn();
					ImGui::Text("%d", entry->m_time);
					ImGui::TableNextColumn();
					ImGui::Text("%f", entry->m_y);
					ImGui::TableNextColumn();
					ImGui::Text("%d", entry->m_muted);
				}
				ImGui::EndTable();
			}
		}
	}
	static void InsideTickleManager()
	{
		auto tickleManager = Lego()->GetTickleManager();
		ImGui::Value("#clients", static_cast<int>(tickleManager->m_clients.size()));
		if (ImGui::BeginTable("Clients", 3, ImGuiTableFlags_Borders)) {
			ImGui::TableSetupColumn("Client");
			ImGui::TableSetupColumn("Interval");
			ImGui::TableSetupColumn("Flags");
			ImGui::TableHeadersRow();
			for (const auto* ticleClient : tickleManager->m_clients) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%s", ticleClient->GetClient()->ClassName());
				ImGui::TableNextColumn();
				ImGui::Text("%d", ticleClient->GetTickleInterval());
				ImGui::TableNextColumn();
				ImGui::Text("0x%x", ticleClient->GetFlags());
			}
			ImGui::EndTable();
		}
	}
	static void InsideVideoManager()
	{
		auto videoManager = Lego()->GetVideoManager();
		SDL_UpdateTexture(g_videoPalette, NULL, videoManager->m_paletteEntries, 4 * 16);
		ImGui::Text("Elapsed: %gs", static_cast<float>(videoManager->GetElapsedSeconds()));
		ImGui::Text("Render3D: %d", videoManager->GetRender3D());
		ImGui::Text("unk0xe5: %d", videoManager->m_unk0xe5);
		ImGui::Text("unk0xe5: %d", videoManager->m_unk0xe6);
		ImGui::Text("unk0x54c: %f", videoManager->m_unk0x54c);
		ImGui::Text("unk0x54c: %f", videoManager->m_unk0x550);
		ImGui::Text("unk0x54c: %d", videoManager->m_unk0x554);
		ImGui::Text("unk0x70: %u", videoManager->m_unk0x70);
		ImGui::Text("Dither: %d", videoManager->m_dither);
		ImGui::Text("BufferCount: %u", videoManager->m_bufferCount);
		ImGui::Text("Paused: %d", videoManager->m_paused);
		ImGui::Text("back: %g", videoManager->m_back);
		ImGui::Text("front: %g", videoManager->m_front);
		ImGui::Text("cameraWidth: %g", videoManager->m_cameraWidth);
		ImGui::Text("cameraHeight: %g", videoManager->m_cameraHeight);
		ImGui::Text("fov: %g", videoManager->m_fov);
		ImVec2 uv_min = ImVec2(0.0f, 0.0f);
		ImVec2 uv_max = ImVec2(1.0f, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, SDL_max(1.0f, ImGui::GetStyle().ImageBorderSize));
		ImGui::ImageWithBg(
			(ImTextureID) (uintptr_t) g_videoPalette,
			ImVec2(200, 200),
			uv_min,
			uv_max,
			ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
		);
		ImGui::PopStyleVar();
	}
};

void IsleDebug_Init()
{
	do {
		if (!g_debugEnabled) {
			break;
		}
		if (!SDL_CreateWindowAndRenderer(
				"Debug ISLE",
				640,
				480,
				SDL_WINDOW_RESIZABLE,
				&g_debugWindow,
				&g_debugRenderer
			)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create debug window");
			break;
		}
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		if (!ImGui_ImplSDL3_InitForSDLRenderer(g_debugWindow, g_debugRenderer)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ImGui_ImplSDL3_InitForSDLRenderer failed");
			g_debugEnabled = false;
			break;
		}
		g_videoPalette =
			SDL_CreateTexture(g_debugRenderer, SDL_PIXELFORMAT_RGBX32, SDL_TEXTUREACCESS_STREAMING, 16, 16);
#if SDL_VERSION_ATLEAST(3, 3, 0)
		SDL_SetTextureScaleMode(g_videoPalette, SDL_SCALEMODE_PIXELART);
#else
		SDL_SetTextureScaleMode(g_videoPalette, SDL_SCALEMODE_NEAREST);
#endif
		if (!ImGui_ImplSDLRenderer3_Init(g_debugRenderer)) {
			g_debugEnabled = false;
			break;
		}
		g_d3drmMiniwinDevice = GetD3DRMMiniwinDevice();
	} while (0);
	if (!g_debugEnabled) {
		if (g_debugRenderer) {
			SDL_DestroyRenderer(g_debugRenderer);
			g_debugRenderer = nullptr;
		}
		if (g_debugWindow) {
			SDL_DestroyWindow(g_debugWindow);
			g_debugWindow = nullptr;
		}
	}
}

void IsleDebug_Quit()
{
	SDL_DestroyRenderer(g_debugRenderer);
	SDL_DestroyWindow(g_debugWindow);
	if (g_d3drmMiniwinDevice) {
		g_d3drmMiniwinDevice->Release();
	}
}

bool IsleDebug_Event(SDL_Event* event)
{
	if (!g_debugEnabled) {
		return false;
	}
	if (event->type == SDL_EVENT_KEY_DOWN) {
		if (event->key.scancode == SCANCODE_KEY_PAUSE) {
			if (!g_debugPaused) {
				IsleDebug_SetPaused(true);
			}
			else {
				g_debugDoStep = true;
			}
			return true;
		}
		if (event->key.scancode == SCANCODE_KEY_RESUME) {
			g_debugDoStep = false;
			if (g_debugPaused) {
				IsleDebug_SetPaused(false);
			}
			return true;
		}
	}
	if (SDL_GetWindowFromEvent(event) != g_debugWindow) {
		return false;
	}
	ImGui_ImplSDL3_ProcessEvent(event);
	return true;
}

void IsleDebug_Render()
{
	if (!g_debugEnabled) {
		return;
	}
	const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGuiIO& io = ImGui::GetIO();
	{
		ImGui::BeginMainMenuBar();
		if (ImGui::MenuItem(g_debugPaused ? "Resume" : "Pause")) {
			g_debugPaused = !g_debugPaused;
			if (g_debugPaused) {
				g_isle->SetWindowActive(false);
				Lego()->Pause();
			}
			else {
				g_isle->SetWindowActive(true);
				Lego()->Resume();
			}
		}
#ifdef ISLE_VALGRIND
		if (ImGui::MenuItem(g_instrumentationEnabled ? "Disable instrumentation" : "Enable instrumentation")) {
			g_instrumentationEnabled = !g_instrumentationEnabled;
			if (g_instrumentationEnabled) {
				CALLGRIND_START_INSTRUMENTATION;
				CALLGRIND_TOGGLE_COLLECT;
			}
			else {
				CALLGRIND_TOGGLE_COLLECT;
				CALLGRIND_STOP_INSTRUMENTATION;
			}
		}
#endif
		ImGui::EndMainMenuBar();
		ImGui::ShowDemoWindow(nullptr);
		LegoOmni* lego = Lego();
		if (ImGui::Begin("LEGO")) {
			if (ImGui::TreeNode("Game State")) {
				LegoGameState* gameState = lego->GetGameState();
				ImGui::Value("Actor Id", gameState->GetActorId());
				ImGui::Text("Actor Name: %s", gameState->GetActorName());
				ImGui::Text("Current act: %d", gameState->GetCurrentAct());
				ImGui::Text("Loaded act: %d", gameState->GetLoadedAct());
				ImGui::Text("Previous area: %d", gameState->m_previousArea);
				ImGui::Text("Saved previous area: %d", gameState->m_savedPreviousArea);
				ImGui::Value("Player count", gameState->m_playerCount);
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Renderer")) {
				if (g_d3drmMiniwinDevice) {
					ImGui::Text("Using miniwin driver");
				}
				else {
					ImGui::Text("No miniwin driver");
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Sound Manager")) {
				LegoSoundManager* soundManager = lego->GetSoundManager();
				Sint32 oldVolume = soundManager->GetVolume();
				int volume = oldVolume;
				ImGui::SliderInt("volume", &volume, 0, 100);
				if (volume != oldVolume) {
					soundManager->SetVolume(volume);
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Video Manager")) {
				DebugViewer::InsideVideoManager();
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Plant Manager")) {
				DebugViewer::InsidePlantManager();
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Building Manager")) {
				DebugViewer::InsideBuildingManager();
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Tickle Manager")) {
				DebugViewer::InsideTickleManager();
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	ImGui::Render();
	SDL_RenderClear(g_debugRenderer);
	SDL_SetRenderScale(g_debugRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	SDL_SetRenderDrawColor(
		g_debugRenderer,
		(Uint8) (clear_color.x * 255),
		(Uint8) (clear_color.y * 255),
		(Uint8) (clear_color.z * 255),
		(Uint8) (clear_color.w * 255)
	);
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), g_debugRenderer);
	SDL_RenderPresent(g_debugRenderer);
}

bool IsleDebug_Enabled()
{
	return g_debugEnabled;
}

void IsleDebug_SetEnabled(bool v)
{
	if (v) {
		SDL_Log(
			"Press \"%s\" for pausing/stepping the game",
			SDL_GetKeyName(SDL_GetKeyFromScancode(SCANCODE_KEY_PAUSE, 0, false))
		);
		SDL_Log(
			"Press \"%s\" for resuming the game",
			SDL_GetKeyName(SDL_GetKeyFromScancode(SCANCODE_KEY_RESUME, 0, false))
		);
	}
	g_debugEnabled = v;
}

void IsleDebug_SetPaused(bool v)
{
	SDL_assert(g_debugPaused == !v);
	g_debugPaused = v;
	if (g_debugPaused) {
		g_isle->SetWindowActive(false);
		Lego()->Pause();
	}
	else {
		g_isle->SetWindowActive(true);
		Lego()->Resume();
	}
}

bool IsleDebug_Paused()
{
	return g_debugPaused;
}

void IsleDebug_ResetStepMode()
{
	g_debugDoStep = false;
}

bool IsleDebug_StepModeEnabled()
{
	return g_debugDoStep;
}
