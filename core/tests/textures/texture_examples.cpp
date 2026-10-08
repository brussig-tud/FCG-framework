
//////
//
// Includes
//

// C++ STL
#include <expected>

// Framework
#include <FCG/fullscreen.h>



//////
//
// Functions
//

/// Compose two reusable passes in a caller-owned command buffer, without intermediate submission.
/// [chain]
[[nodiscard]] auto compose (
	SDL_GPUCommandBuffer *commands,
	const fcg::FullscreenPass &effect,
	const fcg::FullscreenPass &presentation,
	const fcg::Texture &scene,
	const fcg::Texture &intermediate,
	const fcg::Sampler &sampler,
	const fcg::FullscreenTarget &finalTarget
) -> std::expected<void, fcg::FullscreenError>
{
	auto target = fcg::FullscreenTarget::fromTexture(intermediate);
	if (!target)
		return std::unexpected(target.error());
	const fcg::FullscreenTextureBinding first{&scene, &sampler};
	if (auto result = effect.record(commands, *target, {.samplers={&first, 1}}); !result)
		return result;
	const fcg::FullscreenTextureBinding second{&intermediate, &sampler};
	return presentation.record(commands, finalTarget, {.samplers={&second, 1}});
}
/// [chain]

/// Copy one completed depth texel without exposing a typed reference into driver memory.
/// [readback]
[[nodiscard]] auto depthAt (const fcg::Texture &depth, glm::uvec2 pixel) -> std::expected<float, fcg::TextureError>
{
	auto ticket = depth.readback();
	if (!ticket)
		return std::unexpected(ticket.error());
	if (auto result = ticket->wait(); !result)
		return std::unexpected(result.error());
	auto mapping = ticket->map();
	if (!mapping)
		return std::unexpected(mapping.error());
	return mapping->readTexel<float>(glm::uvec3(pixel, 0));
}
/// [readback]
