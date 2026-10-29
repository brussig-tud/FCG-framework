
//////
//
// Includes
//

// C++ STL
#include <format>
#include <span>
#include <stdexcept>
#include <utility>

// SDL3
#include <SDL3/SDL.h>

// Local includes
#include "FCG/player.h"
#include "FCG/gui.h"
#include "FCG/window.h"
#include "scene_format.h"
#include "FCG/viewing.h"



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
// Player

Player::Player(Device &device, Window *mainWindow, std::span<std::unique_ptr<Applet>> applets)
	: m_device(device), window(mainWindow)
{
	// Classify once, retaining registration order within each collection
	for (const auto &applet : applets)
		if (auto *camera = dynamic_cast<Camera*>(applet.get()))
			cameraEntries.push_back({camera, applet.get()});
		else
			otherApplets.push_back(applet.get());
	if (!cameraEntries.empty())
		currentCamera = &cameraEntries.front();
}

Player::~Player() {
	shutdownPresentation();
	depthMapping.reset();
	depthReadback.reset();
}

auto Player::guiUnitScale () const noexcept -> float {
	return gui ? gui->unitScale() : 1.f;
}

void Player::setWindowTitle (const std::string &title)
{
	// A player that was not created by fcg::run has no main window to talk to
	if (!window) {
		SDL_LogWarn(
			SDL_LOG_CATEGORY_APPLICATION,
			"fcg::Player::setWindowTitle() called on a player that is not connected to a main window"
		);
		return;
	}
	window->setTitle(title);
}

void Player::requireSceneRedraw () noexcept {
	sceneRedrawRequired = true;
}

void Player::pushContinuousRedraw () {
	if (continuousRedrawRequests == 0)
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: starting continuous redraw");
	++continuousRedrawRequests;
}

void Player::setPresentationRecorder (PresentationRecorder recorder) {
	if (presenting)
		throw std::logic_error("Cannot replace presentation during recorder invocation");
	m_presentationRecorder = std::move(recorder);
}

void Player::clearPresentationRecorder () {
	if (presenting)
		throw std::logic_error("Cannot clear presentation during recorder invocation");
	m_presentationRecorder = {};
}

auto Player::initializePresentation () -> std::expected<void, FullscreenError>
{
	// No presentation without window
	if (!window)
		return std::unexpected(FullscreenError{FullscreenErrorCode::InvalidState, "No main window to present"});

	// Create our canonical sRGB fullscreen pass
	auto pass = FullscreenPass::linearToSRGB(m_device, window->swapchainFormat());
	if (!pass)
		return std::unexpected(pass.error());

	// Define the default presentation recorder.
	m_defaultPresentationRecorder = [this, pass = std::move(*pass)](const PresentationContext &context)
	-> std::expected<void, FullscreenError> {
		const FullscreenTextureBinding binding{&context.scene, &context.sceneSampler};
		const std::span<const std::byte> uniforms[]{std::as_bytes(std::span(&m_gammaCorrection, 1))};
		return pass.record(
			context.cmdBuffer, context.target, {.samplers = {&binding, 1}, .uniforms = uniforms}
		);
	};
	return {};
}

void Player::shutdownPresentation () noexcept {
	m_presentationRecorder = {};
	m_defaultPresentationRecorder = {};
}

auto Player::presentFrame () -> std::expected<void, FullscreenError>
{
	if (!frame)
		return std::unexpected(FullscreenError{FullscreenErrorCode::InvalidState, "No acquired frame to present"});
	auto &recorder = m_presentationRecorder ? m_presentationRecorder : m_defaultPresentationRecorder;
	presenting = true;
	try {
		auto result = frame->present(recorder);
		presenting = false;
		return result;
	} catch (...) {
		presenting = false;
		throw;
	}
}

void Player::popContinuousRedraw ()
{
	// Clamp at zero – an unbalanced pop is a bug in the calling applet, but not fatal
	if (continuousRedrawRequests < 1) {
		SDL_LogWarn(
			SDL_LOG_CATEGORY_APPLICATION,
			"fcg::Player::popContinuousRedraw() called without a matching pushContinuousRedraw()"
		);
		return;
	}
	--continuousRedrawRequests;
	if (continuousRedrawRequests == 0)
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: stopping continuous redraw");
}

auto Player::continuousRedrawRequested () const -> bool {
	return continuousRedrawRequests > 0;
}

void Player::requestClose ()
{
	m_closeRequested.store(true);
	if (!window) {
		// A player without a window still records the request so callers can observe it via shouldClose().
		return;
	}
	window->requestClose();
}

auto Player::shouldClose () const -> bool {
	if (m_closeRequested.load())
		return true;
	return window && window->shouldClose();
}

auto Player::cameras () const -> std::vector<Camera*> {
	std::vector<Camera*> result;
	result.reserve(cameraEntries.size());
	for (const auto &entry : cameraEntries)
		result.push_back(entry.camera);
	return result;
}

auto Player::activeCamera () const noexcept -> std::optional<Camera*> {
	return currentCamera ? std::optional(currentCamera->camera) : std::nullopt;
}

auto Player::findCamera (std::string_view name) const -> std::optional<Camera*> {
	for (const auto &entry : cameraEntries)
		if (entry.applet->name() == name)
			return entry.camera;
	return {};
}

void Player::setActiveCamera (Camera *camera)
{
	// Null is a selection in its own right, distinct from no pending request
	if (!camera) {
		pendingCamera = nullptr;
		return;
	}
	for (auto &entry : cameraEntries)
		if (entry.camera == camera) {
			pendingCamera = &entry;
			return;
		}
	throw std::invalid_argument("Cannot activate an unregistered camera");
}

void Player::setActiveCamera (std::string_view name) {
	const auto camera = findCamera(name);
	if (!camera)
		throw std::invalid_argument("No registered camera has the requested name");
	setActiveCamera(*camera);
}

void Player::applyCameraSelection (std::optional<CameraEntry*> selection) {
	if (!selection || *selection == currentCamera)
		return;
	deactivateCamera();
	currentCamera = *selection;
	requireSceneRedraw();
}

void Player::prepareCameraPipelines () {
	if (!currentCamera || !currentCamera->pipelinesDirty)
		return;
	currentCamera->applet->recreatePipelines(m_device, *this);
	currentCamera->pipelinesDirty = false;
}

void Player::prepareCameraViewport ()
{
	if (!currentCamera || !currentCamera->viewportDirty)
		return;
	currentCamera->applet->onViewportResize(m_device, currentCamera->viewport, *this);
	currentCamera->viewport = viewportSize();
	currentCamera->viewportDirty = false;
}

void Player::activateCamera () {
	if (!currentCamera || cameraActivated)
		return;
	cameraActivated = true;
	currentCamera->camera->onActivate(*this);
}

void Player::deactivateCamera () noexcept {
	if (std::exchange(cameraActivated, false))
		currentCamera->camera->onDeactivate(*this);
}

auto Player::swapchainFormat () const -> SDL_GPUTextureFormat {
	return window ? window->swapchainFormat() : SDL_GPU_TEXTUREFORMAT_INVALID;
}

auto Player::mainRenderTargetInfo () const -> std::optional<RenderTargetInfo> {
	return window ? window->renderTargetInfo() : std::nullopt;
}

auto Player::setSceneFormat (SDL_GPUTextureFormat format) -> std::expected<void, TextureError>
{
	if (!SDL_IsMainThread())
		throw std::logic_error("Player::setSceneFormat requires the main thread");
	const auto target = mainRenderTargetInfo();
	if (!target)
		return std::unexpected(TextureError{TextureErrorCode::InvalidState, "No claimed main window"});
	if (auto result = detail::validateSceneFormat(&m_device, format); !result)
		return result;
	if (format == target->colorFormat)
		pendingSceneFormat.reset();
	else
		pendingSceneFormat = format;
	return {};
}

auto Player::applySceneFormat () -> bool
{
	if (!pendingSceneFormat)
		return false;
	const auto previous = mainRenderTargetInfo();
	if (auto result = window->setSceneFormat(*pendingSceneFormat); !result)
		throw std::runtime_error("Applying scene format: " + result.error().message);
	pendingSceneFormat.reset();
	return previous->colorFormat != mainRenderTargetInfo()->colorFormat;
}

auto Player::viewportSize() const -> glm::uvec2 {
	return window ? window->viewportSize() : glm::uvec2(0);
}

auto Player::scheduleDepthReadback () -> uint64_t
{
	if (depthReadback && depthToken == readbackToken)
		return depthToken;
	if (depthReadback && !depthMapping)
		throw std::logic_error("Player: orphaned depth readback operation");
	if (!frame || !frame->depthTexture())
		throw std::runtime_error("Player: no depth texture is available");
	depthMapping.reset();
	depthReadback.reset();
	const auto extent = viewportSize();
	const SDL_GPUTextureRegion source{frame->depthTexture(), 0, 0, 0, 0, 0, extent.x, extent.y, 1};
	auto ticket = TextureReadback::create(m_device, source, SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
	if (!ticket)
		throw std::runtime_error("Player: depth readback: " + ticket.error().message);
	depthReadback = std::make_unique<TextureReadback>(std::move(*ticket));
	depthToken = readbackToken;
	return depthToken;
}

auto Player::getDepthReadbackResult (uint64_t token) -> const TextureReadback::Mapping&
{
	if (!depthReadback)
		throw std::logic_error("Player: no depth readback was scheduled");
	if (token != depthToken)
		throw std::runtime_error("Player: depth readback result queried with invalid token");
	if (!depthMapping)
	{
		if (auto result = depthReadback->wait(); !result)
			throw std::runtime_error("Player: waiting for depth: " + result.error().message);
		auto mapping = depthReadback->map();
		if (!mapping)
			throw std::runtime_error("Player: mapping depth: " + mapping.error().message);
		depthMapping.emplace(std::move(*mapping));
	}
	return *depthMapping;
}

void Player::invalidateReadbacks () {
	depthMapping.reset();
	depthReadback.reset();
}

void Player::collectReadbackResults () {
	if (depthReadback && !depthMapping)
		(void)getDepthReadbackResult(depthToken);
	++readbackToken;
}



//////
//
// Module namespace close
//

} // namespace fcg
