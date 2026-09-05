# Post-install deliverable verification for the install-smoke test.
# Expects PREFIX - the installation prefix used.

set(missing "")

# Library: Core (static with debug postfix, static release, or shared)
if (NOT EXISTS "${PREFIX}/lib/libCored.a"
	AND NOT EXISTS "${PREFIX}/lib/libCore.a"
	AND NOT EXISTS "${PREFIX}/lib/libCore.so")
	list(APPEND missing "libCore (static or shared) in lib/")
endif()

# Public headers
foreach (header run.h window.h applet.h event.h export.h)
	if (NOT EXISTS "${PREFIX}/include/FCG/${header}")
		list(APPEND missing "include/FCG/${header}")
	endif()
endforeach()

# CMake package: config + version file, plus at least one target export set
foreach (file FCGConfig.cmake FCGConfigVersion.cmake)
	if (NOT EXISTS "${PREFIX}/lib/cmake/FCG/${file}")
		list(APPEND missing "lib/cmake/FCG/${file}")
	endif()
endforeach()
if (NOT EXISTS "${PREFIX}/lib/cmake/FCG/FCG-static-targets.cmake"
	AND NOT EXISTS "${PREFIX}/lib/cmake/FCG/FCG-shared-targets.cmake")
	list(APPEND missing "lib/cmake/FCG/FCG-<type>-targets.cmake (static or shared)")
endif()

if (missing)
	message(FATAL_ERROR "install-smoke: missing deliverables:\n  ${missing}")
endif()

message(STATUS "install-smoke-verify: all expected deliverables present in ${PREFIX}")
