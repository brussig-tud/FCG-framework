
//////
//
// Includes
//

// C++ STL
#include <array>
#include <expected>
#include <limits>
#include <span>

// Framework
#include <FCG/fullscreen.h>
#include <FCG/texture.h>



//////
//
// Function implementations
//

/// Create a small texture and upload its initial pixels.
/// [create_upload]
[[nodiscard]] auto makeCheckerTexture (fcg::Device &device)
	-> std::expected<fcg::Texture, fcg::TextureError>
{
	SDL_GPUTextureCreateInfo info{};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	info.width = 2;
	info.height = 2;
	info.layer_count_or_depth = 1;
	info.num_levels = 1;
	auto texture = fcg::Texture::create(device, info);
	if (!texture)
		return texture;

	// Upload the whole base level; a row pitch of 0 means tightly packed rows.
	const std::array pixels{
		std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
		std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0xff},
		std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0xff},
		std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
	};
	if (auto result = texture->upload(std::as_bytes(std::span(pixels))); !result)
		return std::unexpected(result.error());
	return texture;
}
/// [create_upload]

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
	const fcg::GammaUniforms parameters{.gamma = glm::vec3(1)};
	const std::span<const std::byte> uniforms[]{std::as_bytes(std::span(&parameters, 1))};
	return presentation.record(commands, finalTarget, {.samplers = {&second, 1}, .uniforms = uniforms});
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

/// \brief Apply gamma correction to a floating-point intermediate texture, retaining RGB values above 1.
///
/// \pre The source has finite, nonnegative RGB and \p target describes a valid floating-point color attachment.
///
/// \param commands The unsubmitted command buffer, with no active pass.
/// \param scene The sampled source texture, kept alive until GPU completion.
/// \param sampler The sampler, kept alive until GPU completion.
/// \param target The floating-point output target, kept alive until GPU completion.
///
/// \return Nothing on success, or an error from pass creation or recording.
/// [gamma_intermediate]
[[nodiscard]] auto correctGamma (
	SDL_GPUCommandBuffer *commands, const fcg::Texture &scene, const fcg::Sampler &sampler,
	const fcg::FullscreenTarget &target
) -> std::expected<void, fcg::FullscreenError>
{
	auto pass = fcg::FullscreenPass::gammaCorrection(*target.device, target.format);
	if (!pass)
		return std::unexpected(pass.error());
	const fcg::FullscreenTextureBinding binding{&scene, &sampler};
	const fcg::GammaUniforms parameters{.clampRange = {0.f, std::numeric_limits<float>::infinity()}};
	const std::span<const std::byte> uniforms[] = {std::as_bytes(std::span(&parameters, 1))};
	return pass->record(commands, target, {.samplers = {&binding, 1}, .uniforms = uniforms});
}
/// [gamma_intermediate]

/// Create a built-in pass and record one draw that copies the scene into the given target.
/// [present]
[[nodiscard]] auto presentScene (
	SDL_GPUCommandBuffer *commands, const fcg::Texture &scene, const fcg::Sampler &sampler,
	const fcg::FullscreenTarget &target
) -> std::expected<void, fcg::FullscreenError>
{
	auto pass = fcg::FullscreenPass::passthrough(*target.device, target.format);
	if (!pass)
		return std::unexpected(pass.error());
	const fcg::FullscreenTextureBinding binding{&scene, &sampler};
	return pass->record(commands, target, {.samplers = {&binding, 1}});
}
/// [present]
