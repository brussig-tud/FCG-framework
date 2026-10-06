
# SDL_shadercross: runtime SPIR-V translation for the non-Vulkan SDL GPU backends (Metal via SPIRV-Cross, D3D12 via
# DXC). No upstream releases yet, hence the pinned commit. DXC off until the Windows strategy is settled.
CPMAddPackage(
	NAME              SDL_shadercross
	GITHUB_REPOSITORY libsdl-org/SDL_shadercross
	GIT_TAG           1ff05bec573988a98ef9e0260b4da44f512b8367  # main @ 2026-09-05
	OPTIONS
		"SDLSHADERCROSS_DXC OFF"           # SPIR-V passthrough + MSL unaffected
		"SDLSHADERCROSS_VENDORED ON"       # bundle SPIRV-Cross
		"SDLSHADERCROSS_SPIRVCROSS_SHARED OFF"
		"SDLSHADERCROSS_SHARED OFF"        # statically linked into Core
		"SDLSHADERCROSS_STATIC ON"
		"SDLSHADERCROSS_CLI OFF"
		"SDLSHADERCROSS_INSTALL OFF"
)
