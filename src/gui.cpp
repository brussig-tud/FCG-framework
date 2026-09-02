
//////
//
// Includes
//

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

// Local includes
#include "FCG/gui.h"
#include "FCG/device.h"
#include "FCG/frame.h"
#include "FCG/window.h"



//////
//
// Module namespace open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// Gui

auto Gui::create (Device &device, Window &mainWindow) -> std::unique_ptr<Gui>
{
	// Set up the framework-wide ImGui context
	IMGUI_CHECKVERSION();
	if (!ImGui::CreateContext()) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Creating the ImGui context failed");
		return nullptr;
	}
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // enable keyboard controls
	ImGui::StyleColorsDark();

	// Initialize the SDL3 platform backend
	if (!ImGui_ImplSDL3_InitForSDLGPU(mainWindow.handle())) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Initializing the ImGui SDL3 platform backend failed");
		ImGui::DestroyContext();
		return nullptr;
	}

	// Initialize the SDL GPU renderer backend, targeting the swapchain format of the main window
	ImGui_ImplSDLGPU3_InitInfo initInfo = { };
	initInfo.Device = device.handle();
	initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device.handle(), mainWindow.handle());
	initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
	if (!ImGui_ImplSDLGPU3_Init(&initInfo)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Initializing the ImGui SDL GPU renderer backend failed");
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		return nullptr;
	}

	// Assemble and return the object
	std::unique_ptr<Gui> gui(new Gui());
	gui->m_device = device.handle();
	gui->updateContentScale(mainWindow);
	return gui;
}

Gui::~Gui ()
{
	// We require the GPU device is to still be valid here by contract
	SDL_WaitForGPUIdle(m_device);
	if (ImGui::GetIO().BackendRendererUserData)
		ImGui_ImplSDLGPU3_Shutdown();
	if (ImGui::GetIO().BackendPlatformUserData)
		ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void Gui::processEvent (const SDL_Event &event) {
	ImGui_ImplSDL3_ProcessEvent(&event);
}

void Gui::updateContentScale (const Window &window)
{
	// Query the content scale of the monitor the window is currently on. A failing query (e.g. no display
	// information available yet) reports 0, which we treat as "no scaling".
	float scale = SDL_GetWindowDisplayScale(window.handle());
	if (scale <= 0.0f)
		scale = 1.0f;

	// Only touch the ImGui style when the scale actually changed
	if (scale == m_contentScale)
		return;

	// Apply the new scale: ImGui 1.92 bakes fonts dynamically, so setting FontScaleDpi is all that is needed to
	// scale text. Style sizes (paddings, spacings, thicknesses) are rescaled by the ratio relative to the
	// previously applied scale, keeping repeated changes idempotent.
	ImGuiStyle &style = ImGui::GetStyle();
	style.FontScaleDpi = scale;
	style.ScaleAllSizes(scale / m_contentScale);
	m_contentScale = scale;
}

void Gui::newFrame () {
	ImGui_ImplSDLGPU3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void Gui::prepareRender (Frame *frame)
{
	// Finalize the ImGui frame – this must happen even if there is no frame to render into (e.g. when the window
	// is minimized), otherwise the ImGui frame stack would be left in an inconsistent state
	ImGui::Render();

	// Upload vertex/index buffers and process texture updates on the frame's command buffer. The SDL GPU backend
	// requires this to be called before the render pass that RenderDrawData will be recorded into.
	if (frame)
		ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), frame->commandBuffer());
}

void Gui::renderDrawData (SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass) {
	ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderPass);
}

auto Gui::wantsTextInput () const -> bool {
	return ImGui::GetIO().WantTextInput;
}

auto Gui::needsPeriodicRedraw () const -> bool
{
	// The only GUI element that changes over time without user input is the blinking text input caret. Only ask
	// the main loop for periodic redraws while a text field is actually being edited.
	const ImGuiIO &io = ImGui::GetIO();
	return io.ConfigInputTextCursorBlink && ImGui::IsAnyItemActive() && io.WantTextInput;
}



//////
//
// Module namespace close
//

} // namespace fcg
