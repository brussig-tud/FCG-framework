
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/Image/image.h>

// Implemented header
#include "FCG/Extras/gui.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Name a framework pixel storage organization.
[[nodiscard]] auto name (fcg::PixelStorage value) -> const char*
{
	switch (value)
	{
		case fcg::PixelStorage::Interleaved: return "Interleaved";
		case fcg::PixelStorage::Packed: return "Packed";
		case fcg::PixelStorage::Indexed: return "Indexed";
		case fcg::PixelStorage::YUV: return "YUV";
		case fcg::PixelStorage::Compressed: return "Compressed";
		case fcg::PixelStorage::External: return "External";
	}
	return nullptr;
}

/// Name the numeric interpretation of stored samples.
[[nodiscard]] auto name (fcg::PixelRepresentation value) -> const char*
{
	switch (value)
	{
		case fcg::PixelRepresentation::UnsignedInteger: return "Unsigned normalized integer";
		case fcg::PixelRepresentation::Binary16: return "IEEE binary16 float";
		case fcg::PixelRepresentation::Binary32: return "IEEE binary32 float";
		case fcg::PixelRepresentation::PaletteIndex: return "Palette index";
		case fcg::PixelRepresentation::Unknown: return "Unknown";
	}
	return nullptr;
}

/// Name a predefined colorspace, leaving custom encodings to the numeric fallback.
[[nodiscard]] auto name (SDL_Colorspace value) -> const char*
{
	switch (value)
	{
		case SDL_COLORSPACE_UNKNOWN: return "Unknown";
		case SDL_COLORSPACE_SRGB: return "sRGB";
		case SDL_COLORSPACE_SRGB_LINEAR: return "Linear sRGB";
		case SDL_COLORSPACE_HDR10: return "HDR10";
		case SDL_COLORSPACE_JPEG: return "JPEG";
		case SDL_COLORSPACE_BT601_LIMITED: return "BT.601 limited";
		case SDL_COLORSPACE_BT601_FULL: return "BT.601 full";
		case SDL_COLORSPACE_BT709_LIMITED: return "BT.709 limited";
		case SDL_COLORSPACE_BT709_FULL: return "BT.709 full";
		case SDL_COLORSPACE_BT2020_LIMITED: return "BT.2020 limited";
		case SDL_COLORSPACE_BT2020_FULL: return "BT.2020 full";
		default: return nullptr;
	}
}

/// Name the colorspace component model.
[[nodiscard]] auto name (SDL_ColorType value) -> const char*
{
	switch (value) {
		case SDL_COLOR_TYPE_UNKNOWN: return "Unknown";
		case SDL_COLOR_TYPE_RGB: return "RGB";
		case SDL_COLOR_TYPE_YCBCR: return "YCbCr";
	}
	return nullptr;
}

/// Name the encoded component range.
[[nodiscard]] auto name (SDL_ColorRange value) -> const char*
{
	switch (value) {
		case SDL_COLOR_RANGE_UNKNOWN: return "Unknown";
		case SDL_COLOR_RANGE_LIMITED: return "Limited";
		case SDL_COLOR_RANGE_FULL: return "Full";
	}
	return nullptr;
}

/// Name the color primaries and reference white.
[[nodiscard]] auto name (SDL_ColorPrimaries value) -> const char*
{
	switch (value)
	{
		case SDL_COLOR_PRIMARIES_UNKNOWN: return "Unknown";
		case SDL_COLOR_PRIMARIES_UNSPECIFIED: return "Unspecified";
		case SDL_COLOR_PRIMARIES_BT709: return "BT.709";
		case SDL_COLOR_PRIMARIES_BT470M: return "BT.470 M";
		case SDL_COLOR_PRIMARIES_BT470BG: return "BT.470 BG";
		case SDL_COLOR_PRIMARIES_BT601: return "BT.601";
		case SDL_COLOR_PRIMARIES_SMPTE240: return "SMPTE 240M";
		case SDL_COLOR_PRIMARIES_GENERIC_FILM: return "Generic film";
		case SDL_COLOR_PRIMARIES_BT2020: return "BT.2020";
		case SDL_COLOR_PRIMARIES_XYZ: return "XYZ (SMPTE ST 428-1)";
		case SDL_COLOR_PRIMARIES_SMPTE431: return "SMPTE RP 431-2";
		case SDL_COLOR_PRIMARIES_SMPTE432: return "SMPTE EG 432-1 (DCI P3)";
		case SDL_COLOR_PRIMARIES_EBU3213: return "EBU 3213";
		case SDL_COLOR_PRIMARIES_CUSTOM: return "Custom";
	}
	return nullptr;
}

/// Name the encoded transfer characteristics.
[[nodiscard]] auto name (SDL_TransferCharacteristics value) -> const char*
{
	switch (value)
	{
		case SDL_TRANSFER_CHARACTERISTICS_UNKNOWN: return "Unknown";
		case SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED: return "Unspecified";
		case SDL_TRANSFER_CHARACTERISTICS_BT709: return "BT.709";
		case SDL_TRANSFER_CHARACTERISTICS_GAMMA22: return "Gamma 2.2";
		case SDL_TRANSFER_CHARACTERISTICS_GAMMA28: return "Gamma 2.8";
		case SDL_TRANSFER_CHARACTERISTICS_BT601: return "BT.601";
		case SDL_TRANSFER_CHARACTERISTICS_SMPTE240: return "SMPTE 240M";
		case SDL_TRANSFER_CHARACTERISTICS_LINEAR: return "Linear";
		case SDL_TRANSFER_CHARACTERISTICS_LOG100: return "Logarithmic (100:1)";
		case SDL_TRANSFER_CHARACTERISTICS_LOG100_SQRT10: return "Logarithmic (100 sqrt(10):1)";
		case SDL_TRANSFER_CHARACTERISTICS_IEC61966: return "IEC 61966-2-4";
		case SDL_TRANSFER_CHARACTERISTICS_BT1361: return "BT.1361";
		case SDL_TRANSFER_CHARACTERISTICS_SRGB: return "sRGB";
		case SDL_TRANSFER_CHARACTERISTICS_BT2020_10BIT: return "BT.2020 (10-bit)";
		case SDL_TRANSFER_CHARACTERISTICS_BT2020_12BIT: return "BT.2020 (12-bit)";
		case SDL_TRANSFER_CHARACTERISTICS_PQ: return "PQ (SMPTE ST 2084)";
		case SDL_TRANSFER_CHARACTERISTICS_SMPTE428: return "SMPTE ST 428-1";
		case SDL_TRANSFER_CHARACTERISTICS_HLG: return "HLG";
		case SDL_TRANSFER_CHARACTERISTICS_CUSTOM: return "Custom";
	}
	return nullptr;
}

/// Name the matrix used to encode chroma components.
[[nodiscard]] auto name (SDL_MatrixCoefficients value) -> const char*
{
	switch (value)
	{
		case SDL_MATRIX_COEFFICIENTS_IDENTITY: return "Identity";
		case SDL_MATRIX_COEFFICIENTS_BT709: return "BT.709";
		case SDL_MATRIX_COEFFICIENTS_UNSPECIFIED: return "Unspecified";
		case SDL_MATRIX_COEFFICIENTS_FCC: return "FCC";
		case SDL_MATRIX_COEFFICIENTS_BT470BG: return "BT.470 BG";
		case SDL_MATRIX_COEFFICIENTS_BT601: return "BT.601";
		case SDL_MATRIX_COEFFICIENTS_SMPTE240: return "SMPTE 240M";
		case SDL_MATRIX_COEFFICIENTS_YCGCO: return "YCgCo";
		case SDL_MATRIX_COEFFICIENTS_BT2020_NCL: return "BT.2020 non-constant luminance";
		case SDL_MATRIX_COEFFICIENTS_BT2020_CL: return "BT.2020 constant luminance";
		case SDL_MATRIX_COEFFICIENTS_SMPTE2085: return "SMPTE ST 2085";
		case SDL_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL: return "Chroma-derived non-constant luminance";
		case SDL_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL: return "Chroma-derived constant luminance";
		case SDL_MATRIX_COEFFICIENTS_ICTCP: return "ICtCp";
		case SDL_MATRIX_COEFFICIENTS_CUSTOM: return "Custom";
	}
	return nullptr;
}

/// Name the chroma sample location.
[[nodiscard]] auto name (SDL_ChromaLocation value) -> const char*
{
	switch (value) {
		case SDL_CHROMA_LOCATION_NONE: return "None";
		case SDL_CHROMA_LOCATION_LEFT: return "Left";
		case SDL_CHROMA_LOCATION_CENTER: return "Center";
		case SDL_CHROMA_LOCATION_TOPLEFT: return "Top left";
	}
	return nullptr;
}

/// Format a known enum name or retain its numeric value when unrecognized.
template<class Enum>
[[nodiscard]] auto enumText (Enum value) -> std::string {
	if (const auto *text = name(value))
		return text;
	return std::format("Unrecognized ({})", std::to_underlying(value));
}

/// Format a boolean metadata property.
[[nodiscard]] auto yesNo (bool value) -> const char* {
	return value ? "Yes" : "No";
}

/// Format an optional metadata value or explain why it is absent.
template<class Value>
[[nodiscard]] auto optionalText (const std::optional<Value> &value, const char *absent) -> std::string {
	return value ? std::format("{}", *value) : absent;
}

/// Format logical RGBA channel widths or precision.
[[nodiscard]] auto channelText (const std::array<unsigned, 4> &values) -> std::string {
	return std::format("R: {}  G: {}  B: {}  A: {}", values[0], values[1], values[2], values[3]);
}

/// Begin a property/value table with wrapped, proportionally sized columns.
[[nodiscard]] auto beginTable (const char *id) -> bool
{
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		return false;
	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.45f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
	return true;
}

/// Append one property and its wrapped value to the current table.
void row (const char *label, const std::string &value)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextWrapped("%s", label);
	ImGui::TableNextColumn();
	ImGui::TextWrapped("%s", value.c_str());
}

/// Describe provenance in the annotation and its tooltip without assigning confidence scores.
[[nodiscard]] auto originText (fcg::ImageMetadataOrigin origin) -> const char*
{
	switch (origin)
	{
		case fcg::ImageMetadataOrigin::Unknown: return "unclassified";
		case fcg::ImageMetadataOrigin::Declared: return "declared";
		case fcg::ImageMetadataOrigin::FormatDefined: return "format-defined";
		case fcg::ImageMetadataOrigin::DecoderReported: return "decoder-reported";
		case fcg::ImageMetadataOrigin::Assumed: return "assumed";
		case fcg::ImageMetadataOrigin::Derived: return "derived";
		case fcg::ImageMetadataOrigin::Assigned: return "assigned";
	}
	return "unclassified";
}

/// Append current evidence with a subdued dotted underline hinting at the explanatory tooltip.
void annotatedRow (const char *label, const std::string &value, const fcg::ImagePropertyProvenance &provenance)
{
	if (provenance.origin == fcg::ImageMetadataOrigin::Unknown && provenance.explanation.empty()
		&& !provenance.adapted && !provenance.dependsOnAssumptions) {
		row(label, value);
		return;
	}

	// Keep the value and every wrapped annotation line in one tooltip hover target.
	std::string annotation = std::string("(") + originText(provenance.origin);
	if (provenance.adapted)
		annotation += "; adapted";
	if (provenance.dependsOnAssumptions && provenance.origin != fcg::ImageMetadataOrigin::Assumed)
		annotation += "; assumed inputs";
	annotation += ')';
	const auto tooltip = std::format("Origin: {}.{}{}\n{}", originText(provenance.origin),
		provenance.adapted ? " Samples were adapted to this interpretation." : "",
		provenance.dependsOnAssumptions ? " A known assumption contributes to this value or its processing." : "",
		provenance.explanation);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextWrapped("%s", label);
	ImGui::TableNextColumn();
	ImGui::BeginGroup();
	const float available = ImGui::GetContentRegionAvail().x;
	ImGui::TextWrapped("%s", value.c_str());
	if (ImGui::CalcTextSize(value.c_str()).x + ImGui::GetStyle().ItemInnerSpacing.x
		+ ImGui::CalcTextSize(annotation.c_str()).x <= available)
		ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);

	// Wrap with public ImGui font metrics and underline each actual line independently.
	std::vector<std::pair<ImVec2, ImVec2>> lines;
	const char *begin = annotation.data();
	const char *end = begin + annotation.size();
	while (begin < end)
	{
		const auto *lineEnd = ImGui::GetFont()->CalcWordWrapPosition(ImGui::GetFontSize(), begin, end,
			std::max(1.f, ImGui::GetContentRegionAvail().x));
		if (lineEnd <= begin)
			lineEnd = begin + 1;
		ImGui::TextUnformatted(begin, lineEnd);
		lines.emplace_back(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
		begin = lineEnd;
		while (begin < end && *begin == ' ')
			++begin;
	}
	ImGui::EndGroup();
	const bool hovered = ImGui::IsItemHovered();
	if (ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 38.f);
		ImGui::TextUnformatted(tooltip.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	// Scale the cue with the font; table clipping also applies to these draw-list primitives.
	const float scale = ImGui::GetFontSize() / 16.f;
	const float step = std::max(2.f, 3.f * scale);
	const auto color = ImGui::GetColorU32(ImGuiCol_TextDisabled, hovered ? 1.f : .65f);
	for (const auto &[minimum, maximum] : lines)
		for (float x = minimum.x; x < maximum.x; x += step)
			ImGui::GetWindowDrawList()->AddLine(ImVec2(x, maximum.y - scale),
				ImVec2(std::min(x + scale, maximum.x), maximum.y - scale), color, std::max(1.f, scale));
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library's namespace for assorted extra utilities.
namespace fcg::extra {



//////
//
// Function implementations
//

void displayImageMetadata (const Image &image)
{
	// Inspect the current metadata and display failures in place.
	const auto metadata = image.pixelInfo();
	ImGui::BeginGroup();
	ImGui::Text("%d × %d pixels", image.width(), image.height());
	if (!metadata) {
		ImGui::TextWrapped("Image metadata unavailable: %s", metadata.error().message.c_str());
		ImGui::EndGroup();
		return;
	}

	// Prepare the values shared by the summary and detail tables.
	const auto &info = *metadata;
	const auto &format = info.pixelFormat;
	const auto channelOrder = format.channelOrder.empty() ? "Unavailable" : format.channelOrder;
	const auto representation = enumText(format.representation);
	const auto colorspace = info.colorspace;
	const auto *colorspaceName = name(colorspace);
	const auto colorspaceDetails = std::format("{} (0x{:08X})",
		colorspaceName ? colorspaceName : "Custom or unrecognized", std::to_underlying(colorspace));
	const auto colorType = enumText(SDL_COLORSPACETYPE(colorspace));
	ImGui::PushID("fcg::extra::displayImageMetadata");
	ImGui::PushID(&image);

	// Keep the most frequently needed properties visible above the expandable details.
	if (beginTable("summary"))
	{
		row("Channel order", channelOrder);
		row("Numeric representation", representation);
		row("Colorspace", colorspaceName ? colorspaceName : colorspaceDetails);
		row("Color type", colorType);
		ImGui::EndTable();
	}

	// Describe physical storage separately from color encoding.
	if (ImGui::CollapsingHeader("Pixel storage") && beginTable("storage"))
	{
		row("Pixel format", std::format("{} (0x{:08X})", format.name, std::to_underlying(format.format)));
		row("Storage organization", enumText(format.storage));
		row("Numeric representation", representation);
		row("Channel order", channelOrder);
		row("Channel precision known", yesNo(format.channelPrecisionKnown));
		const bool indexed = format.storage == fcg::PixelStorage::Indexed;
		row(indexed ? "Palette channel bits" : "Channel sample bits",
			format.channelPrecisionKnown ? channelText(format.channelBits) : "Unavailable");
		row(indexed ? "Palette precision bits" : "Channel precision bits",
			format.channelPrecisionKnown ? channelText(format.channelPrecision) : "Unavailable");
		row("Bits per pixel", optionalText(format.bitsPerPixel, "Unavailable"));
		row("Bytes per pixel", optionalText(format.bytesPerPixel, "Unavailable"));
		// The library uses zero for unused bits when physical storage size is unavailable.
		row("Unused storage bits", format.bytesPerPixel ? std::format("{}", format.unusedBits) : "Unavailable");
		row("Row pitch", std::format("{} bytes", info.pitch));
		row("YUV sample bits", optionalText(format.yuvSampleBits, "Unavailable"));
		row("YUV storage bits", optionalText(format.yuvStorageBits, "Unavailable"));
		row("Native typed pixel access", yesNo(format.directAccess));
		ImGui::EndTable();
	}

	// Annotate only the individual color properties reported by the image.
	if (ImGui::CollapsingHeader("Color encoding") && beginTable("encoding"))
	{
		row("Colorspace", colorspaceDetails);
		row("Color type", colorType);
		annotatedRow("Color range", enumText(SDL_COLORSPACERANGE(colorspace)), info.provenance.colorRange);
		annotatedRow("Color primaries", enumText(SDL_COLORSPACEPRIMARIES(colorspace)),
			info.provenance.colorPrimaries);
		annotatedRow("Transfer characteristics", enumText(SDL_COLORSPACETRANSFER(colorspace)),
			info.provenance.transferCharacteristics);
		// Zero means identity/no chroma for a known colorspace, but conveys no such information for UNKNOWN.
		row("Matrix coefficients", colorspace == SDL_COLORSPACE_UNKNOWN
			? "Unknown" : enumText(SDL_COLORSPACEMATRIX(colorspace)));
		row("Chroma location", colorspace == SDL_COLORSPACE_UNKNOWN
			? "Unknown" : enumText(SDL_COLORSPACECHROMA(colorspace)));
		annotatedRow("SDR white level (colorspace units)", optionalText(info.sdrWhitePoint, "Not specified"), info.provenance.sdrWhitePoint);
		annotatedRow("HDR headroom (relative to SDR white)", optionalText(info.hdrHeadroom, "Not specified"), info.provenance.hdrHeadroom);
		ImGui::EndTable();
	}

	// Show palette and transparency properties without expanding or converting pixels.
	if (ImGui::CollapsingHeader("Palette and transparency") && beginTable("transparency")) {
		row("Palette attached", yesNo(info.hasPalette));
		row("Palette entries", std::format("{}", info.paletteSize));
		row("Alpha capable", yesNo(format.hasAlpha));
		row("Color key (sample or index)", info.colorKey ? std::format("0x{:08X}", *info.colorKey) : "None");
		ImGui::EndTable();
	}

	ImGui::PopID();
	ImGui::PopID();
	ImGui::EndGroup();
}

void showImageMetadataWindow (const Image &image, const GuiWindowOptions &options)
{
	// Keep a closed inspector dormant until the application reopens it.
	if (options.open && !*options.open)
		return;

	// Seed only windows without saved settings, retaining later user adjustments.
	if (options.initialPosition)
		ImGui::SetNextWindowPos(*options.initialPosition, ImGuiCond_FirstUseEver);
	if (options.initialSize)
		ImGui::SetNextWindowSize(*options.initialSize, ImGuiCond_FirstUseEver);
	const char *title = options.title ? options.title->c_str() : "Image information";

	// End is required even when Begin reports a collapsed or clipped window.
	if (ImGui::Begin(title, options.open, options.flags))
		displayImageMetadata(image);
	ImGui::End();
}



//////
//
// Module namespace close
//

// namespace fcg::extra
}
