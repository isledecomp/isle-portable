#include "isledebug.h"

#include "isleapp.h"
#include "legobuildingmanager.h"
#include "legogamestate.h"
#include "legoplantmanager.h"
#include "legosoundmanager.h"
#include "legovideomanager.h"
#include "misc.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

bool g_debugEnabled;
bool g_debugPaused;
SDL_Window* g_debugWindow;
SDL_Renderer* g_debugRenderer;

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
		if (!ImGui_ImplSDLRenderer3_Init(g_debugRenderer)) {
			g_debugEnabled = false;
			break;
		}
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

bool IsleDebug_Event(SDL_Event* event)
{
	if (!g_debugEnabled) {
		return false;
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
		ImGui::EndMainMenuBar();
		LegoOmni* lego = Lego();
		if (ImGui::Begin("LEGO")) {
			if (ImGui::TreeNode("Game State")) {
				LegoGameState* gameState = lego->GetGameState();
				ImGui::Value("Actor Id", gameState->GetActorId());
				ImGui::Text("Actor Name: %s", gameState->GetActorName());
				ImGui::Text("Current act: %d", gameState->GetCurrentAct());
				ImGui::Text("Loaded act: %d", gameState->GetLoadedAct());
				ImGui::Text("Previous area: %d", gameState->GetPreviousArea());
				ImGui::Text("Unknown 0x42c: %d", gameState->GetUnknown0x42c());
				ImGui::Value("Player count", gameState->GetPlayerCount());
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Sound Manager")) {
				LegoSoundManager* soundManager = lego->GetSoundManager();
				Sint32 oldVolume = soundManager->GetVolume();
				Sint32 volume = oldVolume;
				ImGui::SliderInt("volume", &volume, 0, 100);
				if (volume != oldVolume) {
					soundManager->SetVolume(volume);
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Video Manager")) {
				LegoVideoManager* videoManager = lego->GetVideoManager();
				ImGui::Text("Elapsed: %g", static_cast<float>(videoManager->GetElapsedSeconds()));
				ImGui::Value("Render3D", videoManager->GetRender3D());
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Plant Manager")) {
				LegoPlantManager* plantManager = lego->GetPlantManager();
				ImGui::Value("#plants", plantManager->GetNumPlants());
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Building Manager")) {
				LegoBuildingManager* buildingManager = lego->GetBuildingManager();
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}
	ImGui::ShowDemoWindow(nullptr);

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
