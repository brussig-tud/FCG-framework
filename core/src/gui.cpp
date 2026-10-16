
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

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
#include "gui_scaling.h"



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// Gui

Gui::Gui() = default;

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

	// Keep the pixel-snapped bitmap font for 1x output, but use the scalable font for scaled displays.
	ImFont *bitmapFont = io.Fonts->AddFontDefaultBitmap();
	ImFont *vectorFont = io.Fonts->AddFontDefaultVector();
	if (!bitmapFont || !vectorFont) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Loading the default ImGui fonts failed");
		ImGui::DestroyContext();
		return nullptr;
	}
	io.FontDefault = bitmapFont;

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
	gui->m_window = mainWindow.handle();
	gui->m_scaling = std::make_unique<detail::GuiScaling>(mainWindow.id(), bitmapFont, vectorFont);
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
	m_scaling->processEvent(event);
	ImGui_ImplSDL3_ProcessEvent(&event);
}

void Gui::newFrame (Frame *frame)
{
	ImGui_ImplSDLGPU3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	m_scaling->refresh([this] {
		return detail::GuiScaling::DisplayScale{
			SDL_GetWindowDisplayScale(m_window), SDL_GetWindowPixelDensity(m_window)
		};
	});
	ImGuiIO &io = ImGui::GetIO();
	const auto extent = frame ? frame->extent() : glm::uvec2(0);
	io.DisplayFramebufferScale = detail::GuiScaling::framebufferScale(
		io.DisplaySize, ImVec2((float)extent.x, (float)extent.y), io.DisplayFramebufferScale
	);
	m_scaling->selectFont();
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
	{
		// The window can be resized between acquiring the swapchain texture and finalizing this frame, so the extent
		// ImGui derives from the window may exceed the render pass dimensions. ImGui projects its clip/scissor
		// rectangles through the draw data's framebuffer scale, so pin that scale to the actual target extent to keep
		// every scissor rectangle within the render pass (out-of-bounds scissors abort under graphics API validation).
		ImDrawData *drawData = ImGui::GetDrawData();
		const glm::uvec2 extent = frame->extent();
		if (drawData)
			drawData->FramebufferScale = detail::GuiScaling::framebufferScale(
				drawData->DisplaySize, ImVec2((float)extent.x, (float)extent.y), drawData->FramebufferScale
			);
		ImGui_ImplSDLGPU3_PrepareDrawData(drawData, frame->commandBuffer());
	}
}

void Gui::renderDrawData (SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass) {
	ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderPass);
}

auto Gui::wantsKeyboard () const -> bool { return ImGui::GetIO().WantCaptureKeyboard; }
auto Gui::wantsMouse () const -> bool { return ImGui::GetIO().WantCaptureMouse; }

auto Gui::wantsTextInput () const -> bool {
	return ImGui::GetIO().WantTextInput;
}

auto Gui::needsPeriodicRedraw () const -> bool
{
	const ImGuiIO &io = ImGui::GetIO();
	const bool visible = !(SDL_GetWindowFlags(m_window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
	return (visible && m_scaling->retryPending())
		|| (io.ConfigInputTextCursorBlink && ImGui::IsAnyItemActive() && io.WantTextInput);
}



//////
//
// Module namespace close
//

} // namespace fcg
