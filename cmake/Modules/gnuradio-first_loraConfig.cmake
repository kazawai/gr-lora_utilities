find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_FIRST_LORA gnuradio-first_lora)

FIND_PATH(
    GR_FIRST_LORA_INCLUDE_DIRS
    NAMES gnuradio/first_lora/api.h
    HINTS $ENV{FIRST_LORA_DIR}/include
        ${PC_FIRST_LORA_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_FIRST_LORA_LIBRARIES
    NAMES gnuradio-first_lora
    HINTS $ENV{FIRST_LORA_DIR}/lib
        ${PC_FIRST_LORA_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-first_loraTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_FIRST_LORA DEFAULT_MSG GR_FIRST_LORA_LIBRARIES GR_FIRST_LORA_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_FIRST_LORA_LIBRARIES GR_FIRST_LORA_INCLUDE_DIRS)
