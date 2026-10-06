//////
//
// Includes
//

// C++ STL
#include <utility>

// Local includes
#include "FCG/image.h"


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
// Image

auto Image::adopt (SDL_Surface *surface) -> std::expected<Image, ImageError>
{
	if (!surface || surface->w <= 0 || surface->h <= 0 || surface->pitch <= 0 || !surface->pixels)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Image requires a nonempty pixel surface"});
	return Image(surface);
}

Image::~Image ()
{
	if (m_handle)
		SDL_DestroySurface(m_handle);
}

Image::Image (Image &&other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}

auto Image::operator= (Image &&other) noexcept -> Image&
{
	if (this != &other) {
		if (m_handle)
			SDL_DestroySurface(m_handle);
		m_handle = std::exchange(other.m_handle, nullptr);
	}
	return *this;
}


//////
//
// Namespaces close
//

// namespace fcg
}
