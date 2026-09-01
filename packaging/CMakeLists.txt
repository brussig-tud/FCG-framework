
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (NOT DEFINED FCG_INSTALL_CMAKEDIR)
	set(FCG_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/FCG" CACHE STRING "Path to FCG Framework CMake files")
endif()
