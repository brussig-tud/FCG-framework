
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>

// Framework
#include <FCG/texture.h>
#include <FCG/fullscreen.h>
#include <FCG/window.h>
#include <FCG/Image/image.h>
#include <FCG/Render/quad_renderer.h>
#include <texture-test-resources.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

static_assert(!std::is_default_constructible_v<fcg::Texture> && !std::is_copy_constructible_v<fcg::Texture>);
static_assert(!std::is_default_constructible_v<fcg::Sampler> && !std::is_copy_constructible_v<fcg::Sampler>);
static_assert(!std::is_default_constructible_v<fcg::TextureReadback> && !std::is_copy_constructible_v<fcg::TextureReadback>);
static_assert(!std::is_default_constructible_v<fcg::FullscreenPass> && !std::is_copy_constructible_v<fcg::FullscreenPass>);
static_assert(std::is_nothrow_move_constructible_v<fcg::Texture> && std::is_nothrow_move_assignable_v<fcg::Texture>);

/// Assertions remain enabled in Release builds.
void require (bool value, const char *message) {
	if (!value)
		throw std::runtime_error(message);
}

/// Unwrap a successful resource and surface any owned diagnostic.
template <class T, class E>
auto take (std::expected<T, E> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Check a recoverable operation.
template <class E>
void check (std::expected<void, E> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// Check an expected texture rejection.
template <class T>
void fails (const std::expected<T, fcg::TextureError> &result, fcg::TextureErrorCode code) {
	require(!result && result.error().code == code, "Texture error category differs");
}

/// Create a portable single-sample texture descriptor.
auto descriptor (
	SDL_GPUTextureFormat format,
	glm::uvec3 extent={2, 2, 1},
	SDL_GPUTextureUsageFlags usage=SDL_GPU_TEXTUREUSAGE_SAMPLER
) -> SDL_GPUTextureCreateInfo
{
	SDL_GPUTextureCreateInfo info{};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = format;
	info.usage = usage;
	info.width = extent.x;
	info.height = extent.y;
	info.layer_count_or_depth = extent.z;
	info.num_levels = 1;
	info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	return info;
}

/// Return independent copied texels; mapping dies before the stationary ticket.
template <class T>
auto read (const fcg::Texture &texture) -> std::vector<T>
{
	auto ticket = take(texture.readback());
	if (!ticket.ready()) {
		auto early = ticket.map();
		if (!early)
			fails(early, fcg::TextureErrorCode::NotReady);
	}
	check(ticket.wait());
	require(ticket.ready(), "Waited texture readback is not ready");
	auto mapping = take(ticket.map());
	require(mapping.layout().rowPitch % 256 == 0, "Automatic readback rows are not aligned");
	fails(ticket.map(), fcg::TextureErrorCode::InvalidState);
	fails(mapping.template readTexel<T>(mapping.extent()), fcg::TextureErrorCode::InvalidArgument);
	fails(mapping.template readTexel<std::array<std::byte, 37>>({0, 0, 0}), fcg::TextureErrorCode::InvalidArgument);
	std::vector<T> values;
	for (unsigned z = 0; z < mapping.extent().z; ++z)
		for (unsigned y = 0; y < mapping.extent().y; ++y)
			for (unsigned x = 0; x < mapping.extent().x; ++x)
				values.push_back(take(mapping.template readTexel<T>({x, y, z})));
	return values;
}

/// Test uploads, copies, layout validation, layered geometry, volumes, ownership, and abandoned fences.
void transfers (fcg::Device &device, fcg::Device &otherDevice)
{
	using Code = fcg::TextureErrorCode;
	constexpr auto rgba = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	auto invalid = descriptor(rgba);
	invalid.width = 0;
	fails(fcg::Texture::create(device, invalid), Code::InvalidArgument);
	invalid = descriptor(rgba);
	invalid.num_levels = 4;
	fails(fcg::Texture::create(device, invalid), Code::InvalidArgument);
	invalid = descriptor(rgba);
	invalid.usage |= SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ;
	fails(fcg::Texture::create(device, invalid), Code::InvalidArgument);
	fails(fcg::textureTexelSize(SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM), Code::UnsupportedFormat);
	fails(fcg::textureTexelSize(SDL_GPU_TEXTUREFORMAT_D24_UNORM), Code::UnsupportedFormat);

	auto texture = take(fcg::Texture::create(device, descriptor(rgba, {3, 2, 1})));
	const std::array<Uint32, 8> padded{11, 12, 13, 999, 21, 22, 23, 999};
	check(texture.upload(std::as_bytes(std::span(padded)), fcg::TextureTransferLayout{0, 16, 0}));
	require(read<Uint32>(texture) == std::vector<Uint32>({11, 12, 13, 21, 22, 23}), "Padded upload differs");
	const Uint32 changed = 123;
	check(texture.upload(std::as_bytes(std::span(&changed, 1)), {0, 0, {1, 1, 0}, {1, 1, 1}}));
	require(read<Uint32>(texture) == std::vector<Uint32>({11, 12, 13, 21, 123, 23}), "Partial upload lost previous contents");
	fails(texture.upload(std::as_bytes(std::span(padded)), fcg::TextureTransferLayout{0, 11, 0}), Code::InvalidArgument);
	std::array<std::byte, 33> unaligned{};
	std::memcpy(unaligned.data() + 1, padded.data(), sizeof(padded));
	check(texture.upload(unaligned, fcg::TextureTransferLayout{1, 16, 0}));
	check(texture.upload(std::as_bytes(std::span(&changed, 1)), {0, 0, {1, 1, 0}, {1, 1, 1}}));
	fails(texture.upload(std::as_bytes(std::span(padded)), fcg::TextureTransferLayout{0, 16, 31}), Code::InvalidArgument);
	fails(texture.upload(std::as_bytes(std::span(padded)), fcg::TextureTransferLayout{SIZE_MAX, 0, 0}), Code::InvalidArgument);
	fails(texture.validateRegion({UINT32_MAX, 0, {}, {1, 1, 1}}), Code::InvalidArgument);
	fails(texture.validateRegion({0, 0, {UINT32_MAX, 0, 0}, {1, 1, 1}}), Code::InvalidArgument);
	fails(texture.validateRegion({0, 0, {}, {UINT32_MAX, 1, 1}}), Code::InvalidArgument);
	fails(texture.validateRegion({0, 1, {}, {1, 1, 1}}), Code::InvalidArgument);

	auto destination = take(fcg::Texture::create(device, texture.info()));
	auto upload = take(fcg::TransferBuffer::create(device, 64, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD));
	auto download = take(fcg::TransferBuffer::create(device, 64, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD));
	auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
	require(command, SDL_GetError());
	auto *pass = SDL_BeginGPUCopyPass(command);
	require(pass, SDL_GetError());
	fails(texture.copyTo(pass, texture), Code::InvalidArgument);
	fails(texture.uploadFrom(pass, download), Code::InvalidArgument);
	fails(texture.uploadFrom(pass, upload, fcg::TextureTransferLayout{1, 16, 0}), Code::InvalidArgument);
	fails(texture.uploadFrom(pass, upload, fcg::TextureTransferLayout{0, 14, 0}), Code::InvalidArgument);
	fails(texture.downloadTo(pass, upload), Code::InvalidArgument);
	{
		auto mapped = take(upload.map());
		fails(texture.uploadFrom(pass, upload), Code::InvalidState);
		auto changedPadded = padded;
		changedPadded[5] = changed;
		std::memcpy(mapped.data().data(), changedPadded.data(), sizeof(changedPadded));
	}
	check(texture.uploadFrom(pass, upload, fcg::TextureTransferLayout{0, 16, 0}));
	check(texture.copyTo(pass, destination));
	check(destination.downloadTo(pass, download, fcg::TextureTransferLayout{4, 16, 0}));
	SDL_EndGPUCopyPass(pass);
	auto *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
	require(fence && SDL_WaitForGPUFences(device.handle(), true, &fence, 1), SDL_GetError());
	SDL_ReleaseGPUFence(device.handle(), fence);
	{
		auto mapped = take(download.map());
		Uint32 texel = 0;
		std::memcpy(&texel, mapped.data().data() + 4 + 16 + 4, 4);
		require(texel == 123, "Recorded padded download differs");
	}
	require(read<Uint32>(destination) == read<Uint32>(texture), "GPU copy differs");

	for (auto type : {SDL_GPU_TEXTURETYPE_2D_ARRAY, SDL_GPU_TEXTURETYPE_CUBE, SDL_GPU_TEXTURETYPE_3D})
	{
		auto info = descriptor(rgba, {4, 4, type == SDL_GPU_TEXTURETYPE_CUBE ? 6u : 4u});
		info.type = type;
		info.num_levels = 3;
		auto layered = take(fcg::Texture::create(device, info));
		check(layered.validateRegion({2, type == SDL_GPU_TEXTURETYPE_3D ? 0u : 3u, {}, {1, 1, 1}}));
		fails(layered.validateRegion({3, 0, {}, {1, 1, 1}}), Code::InvalidArgument);
		fails(layered.validateRegion({2, 0, {}, {2, 1, 1}}), Code::InvalidArgument);
		fails(layered.validateRegion({0, info.layer_count_or_depth, {}, {1, 1, 1}}), Code::InvalidArgument);
		const fcg::TextureRegion region{1, type == SDL_GPU_TEXTURETYPE_3D ? 0u : 2u, {},
			{2, 2, type == SDL_GPU_TEXTURETYPE_3D ? 2u : 1u}};
		std::array<Uint32, 16> data{};
		data.fill(345);
		check(layered.upload(std::as_bytes(std::span(data)), region, {0, 12, 36}));
		auto ticket = take(layered.readback(region));
		require(ticket.region().mipLevel == region.mipLevel && ticket.region().layer == region.layer, "Readback lost source geometry");
		check(ticket.wait());
		auto mapping = take(ticket.map());
		require(take(mapping.readTexel<Uint32>(region.extent - glm::uvec3(1))) == 345, "Mip/layer/volume upload differs");
	}

	SDL_GPUSamplerCreateInfo samplerInfo{};
	samplerInfo.min_lod = std::numeric_limits<float>::quiet_NaN();
	fails(fcg::Sampler::create(device, samplerInfo), Code::InvalidArgument);
	samplerInfo.min_lod = 0.f;
	auto sampleOwner = take(fcg::Sampler::create(device, samplerInfo));
	auto movedSampler = std::move(sampleOwner);
	require(!sampleOwner.handle() && !sampleOwner.device(), "Moved sampler retained ownership");
	sampleOwner = std::move(movedSampler);
	require(sampleOwner.handle() && sampleOwner.device() == &device, "Sampler move assignment lost ownership");

	auto foreign = take(fcg::TransferBuffer::create(otherDevice, 64, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD));
	command = SDL_AcquireGPUCommandBuffer(device.handle());
	pass = SDL_BeginGPUCopyPass(command);
	fails(texture.uploadFrom(pass, foreign), Code::InvalidArgument);
	SDL_EndGPUCopyPass(pass);
	SDL_CancelGPUCommandBuffer(command);

	for (unsigned i = 0; i < 16; ++i)
		(void)take(texture.readback());
	auto ticket = take(texture.readback());
	auto movedTicket = std::move(ticket);
	require(!ticket.ready(), "Moved-from ticket reported ready");
	fails(ticket.wait(), Code::InvalidState);
	fails(ticket.map(), Code::InvalidState);
	check(movedTicket.wait());
	auto moved = std::move(texture);
	require(!texture.handle() && !texture.device(), "Moved texture retained ownership");
	fails(texture.readback(), Code::InvalidState);
	fails(texture.upload(std::as_bytes(std::span(padded))), Code::InvalidState);
	texture = std::move(moved);
	require(texture.info().width == 3, "Move assignment lost metadata");
}

/// Nearest clamped sampler for exact pixel checks.
auto sampler (fcg::Device &device) -> fcg::Sampler {
	SDL_GPUSamplerCreateInfo info{};
	info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	return take(fcg::Sampler::create(device, info));
}

/// Render an image into float storage, exercising top-left UV and differing output dimensions.
auto sample (fcg::Device &device, const fcg::Texture &input, glm::uvec2 size={2, 2}) -> std::vector<glm::vec4>
{
	auto output = take(fcg::Texture::create(device, descriptor(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
		{size.x, size.y, 1}, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)));
	auto pass = take(fcg::FullscreenPass::passthrough(device, output.info().format));
	auto filtering = sampler(device);
	const fcg::FullscreenTextureBinding binding{&input, &filtering};
	auto target = take(fcg::FullscreenTarget::fromTexture(output));
	auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
	check(pass.record(command, target, {.samplers={&binding, 1}}));
	require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());
	return read<glm::vec4>(output);
}

/// Tolerances appropriate to an 8-bit sRGB round trip. One sRGB8 step is about 3.6e-3 in linear light around 0.216,
/// so half a step admits any driver's conversion precision while a missing or wrong gamma still fails.
constexpr float srgb8Tolerance = 1.8e-3f;

/// Verify floats with tolerances appropriate to sRGB8 quantization or precise linear data.
void near (float actual, float expected, float tolerance=1e-5f) {
	if (std::abs(actual - expected) > tolerance)
		throw std::runtime_error("Color differs: " + std::to_string(actual) + " versus " + std::to_string(expected));
}

/// Exercise native linear precision, format expansion, alpha, color keys, source preservation, and HDR.
void images (fcg::Device &device)
{
	for (auto format : {SDL_PIXELFORMAT_RGBA32, SDL_PIXELFORMAT_BGRA32})
	{
		auto image = take(fcg::Image::adopt(SDL_CreateSurface(2, 2, format)));
		require(SDL_WriteSurfacePixel(image.handle(), 0, 0, 128, 0, 0, 64), SDL_GetError());
		require(SDL_WriteSurfacePixel(image.handle(), 1, 0, 0, 255, 0, 255), SDL_GetError());
		require(SDL_WriteSurfacePixel(image.handle(), 0, 1, 0, 0, 255, 255), SDL_GetError());
		require(SDL_WriteSurfacePixel(image.handle(), 1, 1, 255, 255, 255, 0), SDL_GetError());
		const auto bytes = std::span<const std::byte>((const std::byte*)image.handle()->pixels, image.pitch() * image.height());
		const std::vector<std::byte> saved(bytes.begin(), bytes.end());
		auto texture = take(image.upload(device));
		auto pixels = sample(device, texture, {4, 4});
		near(pixels[0].r, .2158605f, srgb8Tolerance);
		near(pixels[0].a, 64.f / 255);
		near(pixels[3].g, 1.f);
		near(pixels[12].b, 1.f);
		near(pixels[15].a, 0.f);
		require(std::equal(saved.begin(), saved.end(), bytes.begin()), "Image upload changed source pixels");
	}
	std::array<Uint8, 16> rgb{128, 0, 0, 0, 255, 0, 77, 77, 0, 0, 255, 255, 255, 255, 77, 77};
	auto padded = take(fcg::Image::adopt(SDL_CreateSurfaceFrom(2, 2, SDL_PIXELFORMAT_RGB24, rgb.data(), 8)));
	const auto savedRGB = rgb;
	auto paddedTexture = take(padded.upload(device));
	const auto paddedPixels = sample(device, paddedTexture);
	near(paddedPixels[0].r, .2158605f, srgb8Tolerance);
	near(paddedPixels[2].b, 1.f);
	near(paddedPixels[0].a, 1.f);
	require(rgb == savedRGB, "Padded source changed");

	auto indexed = take(fcg::Image::adopt(SDL_CreateSurface(2, 1, SDL_PIXELFORMAT_INDEX8)));
	auto *palette = SDL_CreateSurfacePalette(indexed.handle());
	require(palette, SDL_GetError());
	const std::array<SDL_Color, 2> colors{{{128, 0, 0, 255}, {0, 255, 0, 255}}};
	require(SDL_SetPaletteColors(palette, colors.data(), 0, 2), SDL_GetError());
	((Uint8*)indexed.handle()->pixels)[0] = 0;
	((Uint8*)indexed.handle()->pixels)[1] = 1;
	require(SDL_SetSurfaceColorKey(indexed.handle(), true, 1), SDL_GetError());
	auto indexedTexture = take(indexed.upload(device));
	const auto indexedPixels = sample(device, indexedTexture, {2, 1});
	near(indexedPixels[0].r, .2158605f, srgb8Tolerance);
	near(indexedPixels[1].a, 0.f);
	Uint32 key = 0;
	require(SDL_GetSurfaceColorKey(indexed.handle(), &key) && key == 1 && palette->colors[1].a == 255,
		"Palette/color-key metadata changed");

	// Duplicate palette colors must still honor index-based color keys.
	const std::array<SDL_Color, 2> duplicateColors{{{128, 0, 0, 255}, {128, 0, 0, 255}}};
	require(SDL_SetPaletteColors(palette, duplicateColors.data(), 0, 2), SDL_GetError());
	auto duplicateTexture = take(indexed.upload(device));
	const auto duplicatePixels = sample(device, duplicateTexture, {2, 1});
	near(duplicatePixels[0].a, 1.f);
	near(duplicatePixels[1].a, 0.f);
	// A keyed 10-bit representation must promote to float before applying alpha without quantizing RGB.
	auto keyed = take(fcg::Image::adopt(SDL_CreateSurface(2, 1, SDL_PIXELFORMAT_ARGB2101010)));
	require(SDL_SetSurfaceColorspace(keyed.handle(), SDL_COLORSPACE_SRGB_LINEAR), SDL_GetError());
	const std::array<Uint32, 2> packed{(3u << 30) | (512u << 20) | (341u << 10) | 123u,
		(3u << 30) | (513u << 20) | (342u << 10) | 124u};
	std::memcpy(keyed.handle()->pixels, packed.data(), sizeof(packed));
	require(SDL_SetSurfaceColorKey(keyed.handle(), true, packed[0]), SDL_GetError());
	auto keyedTexture = take(keyed.upload(device));
	const auto keyedPixels = sample(device, keyedTexture, {2, 1});
	near(keyedPixels[0].a, 0.f);
	near(keyedPixels[1].a, 1.f);
	near(keyedPixels[1].r, 513.f / 1023.f);
	near(keyedPixels[1].g, 342.f / 1023.f);

	for (auto format : {SDL_PIXELFORMAT_RGBA64, SDL_PIXELFORMAT_RGBA64_FLOAT, SDL_PIXELFORMAT_RGBA128_FLOAT})
	{
		auto image = take(fcg::Image::adopt(SDL_CreateSurface(1, 1, format)));
		require(SDL_SetSurfaceColorspace(image.handle(), SDL_COLORSPACE_SRGB_LINEAR), SDL_GetError());
		float expected = .1234567f;
		if (format == SDL_PIXELFORMAT_RGBA64) {
			const std::array<Uint16, 4> pixel{12345, 23456, 34567, 45678};
			std::memcpy(image.handle()->pixels, pixel.data(), sizeof(pixel));
			expected = 12345.f / 65535.f;
		}
		else if (format == SDL_PIXELFORMAT_RGBA64_FLOAT) {
			const std::array<Uint16, 4> pixel{glm::packHalf1x16(.125f), glm::packHalf1x16(2.f), glm::packHalf1x16(.75f), glm::packHalf1x16(.5f)};
			std::memcpy(image.handle()->pixels, pixel.data(), sizeof(pixel));
			expected = .125f;
		}
		else {
			const glm::vec4 pixel{expected, 4.f, .75f, .5f};
			std::memcpy(image.handle()->pixels, &pixel, sizeof(pixel));
		}
		SDL_SetFloatProperty(SDL_GetSurfaceProperties(image.handle()), SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, 8.f);
		auto texture = take(image.upload(device));
		const auto pixels = sample(device, texture, {1, 1});
		near(pixels[0].r, expected);
		if (format != SDL_PIXELFORMAT_RGBA64)
			near(pixels[0].g, format == SDL_PIXELFORMAT_RGBA64_FLOAT ? 2.f : 4.f);
	}
	// Native 16-bit rows may include padding that is not a whole GPU texel.
	std::array<Uint16, 12> paddedNative{12345, 23456, 34567, 45678, 99, 99, 54321, 43210, 32109, 21098, 99, 99};
	auto native = take(fcg::Image::adopt(SDL_CreateSurfaceFrom(1, 2, SDL_PIXELFORMAT_RGBA64, paddedNative.data(), 12)));
	require(SDL_SetSurfaceColorspace(native.handle(), SDL_COLORSPACE_SRGB_LINEAR), SDL_GetError());
	auto nativeTexture = take(native.upload(device));
	const auto nativePixels = sample(device, nativeTexture, {1, 2});
	near(nativePixels[0].r, 12345.f / 65535.f);
	near(nativePixels[1].r, 54321.f / 65535.f);
	near(nativePixels[1].a, 21098.f / 65535.f);

	// A non-native linear float RGB layout must expand without losing HDR or applying the source tone-map setting.
	auto hdr = take(fcg::Image::adopt(SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGB96_FLOAT)));
	const std::array<float, 3> value{2.f, 4.f, .1234567f};
	std::memcpy(hdr.handle()->pixels, value.data(), sizeof(value));
	SDL_SetFloatProperty(SDL_GetSurfaceProperties(hdr.handle()), SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, 8.f);
	SDL_SetStringProperty(SDL_GetSurfaceProperties(hdr.handle()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "chrome");
	auto hdrTexture = take(hdr.upload(device));
	const auto hdrPixels = sample(device, hdrTexture, {1, 1});
	near(hdrPixels[0].r, 2.f);
	near(hdrPixels[0].g, 4.f);
	near(hdrPixels[0].b, value[2]);
	near(hdrPixels[0].a, 1.f);
	require(std::strcmp(SDL_GetStringProperty(SDL_GetSurfaceProperties(hdr.handle()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, ""), "chrome") == 0,
		"HDR source metadata changed");
	auto moved = std::move(hdr);
	require(!hdr.upload(device) && hdr.upload(device).error().code == fcg::ImageErrorCode::InvalidState, "Empty image upload accepted");
}

/// Verify all fullscreen binding classes and compose dependent custom passes in one command buffer.
void fullscreen (fcg::Device &device)
{
	constexpr auto format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
	auto input = take(fcg::Texture::create(device, descriptor(format, {1, 1, 1})));
	const glm::vec4 value{.1f, .2f, .3f, .4f};
	check(input.upload(std::as_bytes(std::span(&value, 1))));
	auto storage = take(fcg::Texture::create(device, descriptor(format, {1, 1, 1}, SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ)));
	check(storage.upload(std::as_bytes(std::span(&value, 1))));
	auto buffer = take(fcg::Buffer::create(device, std::as_bytes(std::span(&value, 1)), SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	auto filtering = sampler(device);
	auto shader = fcg::res::shader(texture_test::embedded::FS, "fullscreen_resources");
	require(shader.has_value(), "Custom fullscreen resources missing");
	auto encoder = take(fcg::FullscreenPass::create(device, *shader->stage(fcg::ShaderStage::FRAGMENT),
		{.uniformBuffers=1, .storageBuffers=1, .storageTextures=1, .samplers=2}, format));
	auto intermediate = take(fcg::Texture::create(device, descriptor(format, {3, 5, 1}, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)));
	auto output = take(fcg::Texture::create(device, descriptor(format, {5, 3, 1}, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)));
	std::array<fcg::FullscreenTextureBinding, 2> bindings{{{&input, &filtering}, {&input, &filtering}}};
	const fcg::Texture *textures[]{&storage};
	const fcg::Buffer *buffers[]{&buffer};
	const glm::vec4 gain{2.f};
	const std::span<const std::byte> uniforms[]{std::as_bytes(std::span(&gain, 1))};
	fcg::FullscreenBindings resources{bindings, textures, buffers, uniforms};
	auto first = take(fcg::FullscreenTarget::fromTexture(intermediate));
	auto second = take(fcg::FullscreenTarget::fromTexture(output));
	auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
	require(!encoder.record(command, first, {}), "Missing fullscreen resources accepted");
	check(encoder.record(command, first, resources));
	bindings[0].texture = &intermediate;
	check(encoder.record(command, second, resources));
	bindings[0].texture = &output;
	require(!encoder.record(command, second, resources), "Fullscreen feedback accepted");
	require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());
	for (auto pixel : read<glm::vec4>(output))
		for (unsigned i = 0; i < 4; ++i)
			near(pixel[i], value[i] * 22.f);
	// Exercise sRGB decoding and final encoding, including alpha preservation.
	auto srgb = take(fcg::Texture::create(device, descriptor(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB, {1, 1, 1})));
	const std::array<Uint8, 4> bytes{128, 64, 255, 77};
	check(srgb.upload(std::as_bytes(std::span(bytes))));
	auto final = take(fcg::Texture::create(device, descriptor(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, {7, 9, 1}, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET)));
	auto encoding = take(fcg::FullscreenPass::linearToSRGB(device, final.info().format));
	const fcg::FullscreenTextureBinding encodedBinding{&srgb, &filtering};
	command = SDL_AcquireGPUCommandBuffer(device.handle());
	check(encoding.record(command, take(fcg::FullscreenTarget::fromTexture(final)), {.samplers={&encodedBinding, 1}}));
	require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());
	for (const auto pixel : read<std::array<Uint8, 4>>(final))
		require(pixel == bytes, "Fullscreen triangle coverage, encoding, or alpha differs");
	auto moved = std::move(encoder);
	require(!encoder.handle() && !encoder.device(), "Moved fullscreen encoder retained ownership");
	require(!encoder.draw(nullptr, nullptr, {1, 1}, resources), "Moved fullscreen encoder accepted a draw");
}

/// Exercise uploaded top-first image rows, quad UV orientation, linear transparency, and presentation ordering.
void sceneAndPresentation (fcg::Device &device)
{
	auto image = take(fcg::Image::adopt(SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_RGBA32)));
	const std::array<std::array<Uint8, 4>, 4> source{{{128, 0, 0, 128}, {0, 128, 0, 128}, {0, 0, 128, 128}, {255, 255, 255, 128}}};
	for (unsigned y = 0; y < 2; ++y)
		for (unsigned x = 0; x < 2; ++x) {
			const auto &pixel = source[y * 2 + x];
			require(SDL_WriteSurfacePixel(image.handle(), x, y, pixel[0], pixel[1], pixel[2], pixel[3]), SDL_GetError());
		}
	auto texture = take(image.upload(device));
	auto filtering = sampler(device);
	auto scene = take(fcg::Texture::create(device, descriptor(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
		{4, 4, 1}, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)));
	auto renderer = take(fcg::QuadRenderer::create(device, {scene.info().format}, {.alphaBlending=true}));
	fcg::PrimitiveAttributes attributes(device);
	const std::array positions{glm::vec4(0.f, 0.f, .5f, 1.f)};
	check(attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.set<fcg::Attribute::Position>(std::span(positions));
		update.set<fcg::Attribute::Orientation>(glm::angleAxis(glm::radians(180.f), glm::vec3(1.f, 0.f, 0.f)));
	}));
	fcg::RenderState state(device);
	auto target = take(fcg::FullscreenTarget::fromTexture(scene, 0, 0, SDL_GPU_LOADOP_CLEAR));
	target.color.clear_color = {.1f, .2f, .3f, .25f};
	auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
	auto *pass = SDL_BeginGPURenderPass(command, &target.color, 1, nullptr);
	require(pass, SDL_GetError());
	check(renderer.draw(attributes, state, command, pass, {.texture=fcg::PrimitiveTexture{texture.handle(), filtering.handle()}}));
	SDL_EndGPURenderPass(pass);
	require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());
	const auto pixels = sample(device, scene, {4, 4});
	const float alpha = 128.f / 255.f;
	const std::array corners{0u, 3u, 12u, 15u};
	for (unsigned corner = 0; corner < 4; ++corner)
	{
		for (unsigned channel = 0; channel < 3; ++channel) {
			const float encoded = source[corner][channel] / 255.f;
			const float linear = encoded <= .04045f ? encoded / 12.92f : std::pow((encoded + .055f) / 1.055f, 2.4f);
			near(pixels[corners[corner]][channel], linear * alpha + (.1f + .1f * channel) * (1.f - alpha), .005f);
		}
		near(pixels[corners[corner]].a, alpha + .25f * (1.f - alpha), .005f);
	}

	auto window = fcg::Window::create({.width=16, .height=16});
	require(window && device.claimWindow(window), "Presentation window creation failed");
	require(window->renderTargetInfo()->colorFormat == scene.info().format, "Window does not report canonical scene format");
	auto *frame = window->beginFrame(device);
	require(frame && frame->colorTarget() == frame->sceneTarget().handle()
		&& frame->presentationTarget() != frame->colorTarget(), "Scene and presentation targets are not distinct");
	pass = frame->beginRenderPass({.1f, .2f, .3f, .4f});
	require(pass, SDL_GetError());
	require(!frame->present(), "Presentation accepted an active scene pass");
	frame->endRenderPass();
	check(frame->present(scene));
	require(!frame->present(), "Duplicate presentation accepted");
	pass = frame->beginOverlayRenderPass();
	require(pass, SDL_GetError());
	frame->endRenderPass();
	require(!frame->present(scene), "Presentation accepted after overlay rendering");
	window->endFrame();
	// Presentation also occurs when callers omit it before overlays or before frame completion.
	frame = window->beginFrame(device);
	require(frame->beginRenderPass({0.f, 0.f, 0.f, 1.f}), SDL_GetError());
	frame->endRenderPass();
	require(frame->beginOverlayRenderPass(), SDL_GetError());
	frame->endRenderPass();
	window->endFrame();
	frame = window->beginFrame(device);
	require(frame->beginRenderPass({0.f, 0.f, 0.f, 1.f}), SDL_GetError());
	frame->endRenderPass();
	window->endFrame();
	device.unclaimWindow(window);
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// GPU smoke tests use the selected real backend and fail on validation diagnostics.
auto main () -> int
{
	try {
		require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
		{
			auto device = fcg::Device::create();
			require(device.has_value(), SDL_GetError());
			auto otherDevice = fcg::Device::create();
			require(otherDevice.has_value(), SDL_GetError());
			transfers(*device, *otherDevice);
			images(*device);
			fullscreen(*device);
			sceneAndPresentation(*device);
			device->waitIdle();
		}
		SDL_Quit();
		std::cout << "Texture, Image upload, and fullscreen checks passed\n";
		return 0;
	}
	catch (const std::exception &failure) {
		std::cerr << failure.what() << '\n';
		SDL_Quit();
		return 1;
	}
}
