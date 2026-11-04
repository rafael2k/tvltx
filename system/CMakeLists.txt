cmake_minimum_required(VERSION 3.10)

project(tvltx C CXX)

# Input drivers:
#  - V4L2 + ALSA (generic Linux capture devices): needs libasound
#  - Blackmagic DeckLink: SDK 16.0 headers are bundled; libDeckLinkAPI (Desktop Video)
#    is loaded at run time, so it is only needed on the capture machine
option(WITH_V4L2 "Build the Video4Linux2 + ALSA input driver" ON)
option(WITH_DECKLINK "Build the Blackmagic DeckLink input driver" ON)

file(GLOB_RECURSE PROJ_SOURCES
     tmm/src/*.cpp libmpeg2/src/*.cpp encoder/*.c encoder/*.cpp
     bts_tx/*.cpp bts_tx/DekTec/*.cpp)

find_path(FDKAAC_INCLUDE_DIR aacenc_lib.h PATH_SUFFIXES fdk-aac)
find_library(FDKAAC_LIBRARY fdk-aac)
if (NOT FDKAAC_INCLUDE_DIR OR NOT FDKAAC_LIBRARY)
    message(FATAL_ERROR "fdk-aac not found (Debian/Ubuntu: apt install libfdk-aac-dev)")
endif()

# falling off the end of a non-void function traps (ud2/SIGILL) with modern GCC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wno-multichar -fno-rtti -std=c++11 -Werror=return-type")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -std=gnu99 -Werror=return-type")

set(PROJ_DEFINES "")
set(HAVE_V4L2_INPUT OFF)
set(PROJ_LIBRARIES ${FDKAAC_LIBRARY} m dl rt pthread
                   ${CMAKE_CURRENT_SOURCE_DIR}/bts_tx/DekTec/DTAPI64.o)
set(PROJ_INCLUDES encoder bts_tx tmm/include libmpeg2/include ${FDKAAC_INCLUDE_DIR})

if (WITH_V4L2)
    find_package(ALSA)
    if (ALSA_FOUND)
        set(HAVE_V4L2_INPUT ON)
        list(APPEND PROJ_DEFINES HAS_V4L2__)
        list(APPEND PROJ_LIBRARIES ${ALSA_LIBRARIES})
        list(APPEND PROJ_INCLUDES ${ALSA_INCLUDE_DIRS})
        message(STATUS "V4L2 + ALSA input: enabled")
    else()
        message(WARNING "ALSA not found (apt install libasound2-dev): V4L2 + ALSA input disabled")
    endif()
endif()
if (NOT HAVE_V4L2_INPUT)
    list(FILTER PROJ_SOURCES EXCLUDE REGEX "encoder/input_v4l2\\.c$")
endif()

if (WITH_DECKLINK)
    list(APPEND PROJ_DEFINES HAS_BLACKMAGIC__)
    message(STATUS "Blackmagic DeckLink input: enabled (SDK 16.0, libDeckLinkAPI loaded at run time)")
else()
    list(FILTER PROJ_SOURCES EXCLUDE REGEX "encoder/bmd/")
    message(STATUS "Blackmagic DeckLink input: disabled")
endif()

add_executable(tvltx ${PROJ_SOURCES})
target_include_directories(tvltx PRIVATE ${PROJ_INCLUDES})
target_compile_definitions(tvltx PRIVATE ${PROJ_DEFINES})
target_link_libraries(tvltx ${PROJ_LIBRARIES})
