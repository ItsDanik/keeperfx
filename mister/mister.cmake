# MiSTer hybrid core build (-DMISTER_HYBRID=ON), included by the top level
# CMakeLists.txt in place of the Windows build. The sources are the ones of
# linux.mk; the libraries are static and come from CMAKE_PREFIX_PATH (SDL2
# with the hybrid core drivers, and the ones built by tools/build_deps.sh in
# the core's repository).

file(STRINGS "${CMAKE_SOURCE_DIR}/version.mk" _version)
foreach(_line IN LISTS _version)
    if(_line MATCHES "^(VER_MAJOR|VER_MINOR|VER_RELEASE|VER_BUILD)=([0-9]+)")
        set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endif()
endforeach()
set(PACKAGE_SUFFIX "MiSTer")
set(VER_STRING "${VER_MAJOR}.${VER_MINOR}.${VER_RELEASE}.${VER_BUILD} ${PACKAGE_SUFFIX}")
execute_process(COMMAND git describe --always WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
configure_file("${CMAKE_SOURCE_DIR}/ver_defs.h.in" "${CMAKE_BINARY_DIR}/generated/ver_defs.h")

# The source list of the Linux build
file(READ "${CMAKE_SOURCE_DIR}/linux.mk" _linux_mk)
string(REGEX MATCHALL "src/[A-Za-z0-9_/]+\\.(cpp|c)" _files "${_linux_mk}")
list(REMOVE_DUPLICATES _files)
list(TRANSFORM _files PREPEND "${CMAKE_SOURCE_DIR}/" OUTPUT_VARIABLE KEEPERFX_SOURCES)
# Not on the MiSTer: the online lobby (curl) and port forwarding (miniupnpc,
# natpmp). mister/net_stubs.c answers for them; games on the local network
# and by address still work.
list(REMOVE_ITEM KEEPERFX_SOURCES
    "${CMAKE_SOURCE_DIR}/src/net_matchmaking.c"
    "${CMAKE_SOURCE_DIR}/src/net_portforward.cpp")
list(APPEND KEEPERFX_SOURCES
    "${CMAKE_SOURCE_DIR}/deps/centitoml/toml_api.c"
    "${CMAKE_SOURCE_DIR}/mister/mister_port.c"
    "${CMAKE_SOURCE_DIR}/mister/net_stubs.c"
    "${CMAKE_SOURCE_DIR}/mister/lua/lua_compat.c")

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_net CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED)
find_package(Threads REQUIRED)
function(mister_library var name)
    find_library(${var} NAMES ${name} REQUIRED)
endfunction()
foreach(_lib avformat avcodec swresample avutil lua spng minizip z astronomy json enet6)
    mister_library(LIB_${_lib} ${_lib})
    list(APPEND MISTER_LIBS "${LIB_${_lib}}")
endforeach()
find_path(MISTER_DEPS_INCLUDE lua.h REQUIRED)

# the code leaves out parameter names (C23); the toolchain is gcc 10
set(CMAKE_C_STANDARD 23)
add_executable(keeperfx ${KEEPERFX_SOURCES})
target_compile_definitions(keeperfx PRIVATE MISTER_HYBRID=1 DEBUG=0 BFDEBUG_LEVEL=0 SPNG_STATIC=1 AL_LIBTYPE_STATIC
    AL_ALEXT_PROTOTYPES)    # the loopback device of bflib_sndlib.cpp
target_include_directories(keeperfx PRIVATE
    "${CMAKE_SOURCE_DIR}/mister/lua"    # before the Lua headers, see lua_compat.h
    "${CMAKE_SOURCE_DIR}/mister"
    "${CMAKE_BINARY_DIR}/generated"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/deps/centitoml"
    "${MISTER_DEPS_INCLUDE}")
target_compile_options(keeperfx PRIVATE -Wall -Wno-unused-parameter -Wno-unknown-pragmas -Wno-sign-compare
    -Wno-format-truncation -fno-strict-aliasing
    -fsigned-char   # the code is written for x86, where char has a sign; on ARM it has none
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>)
# The engine is many small functions in many files that call each other for
# every map block and polygon: optimised across the files it is 5% faster
set_property(TARGET keeperfx PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
target_link_libraries(keeperfx PRIVATE
    SDL2_mixer::SDL2_mixer-static SDL2_net::SDL2_net-static OpenAL::OpenAL SDL2::SDL2-static
    ${MISTER_LIBS} Threads::Threads m dl)
