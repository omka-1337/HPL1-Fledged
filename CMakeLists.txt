cmake_minimum_required(VERSION 3.16)
project(HPL1 CXX C)

# The HPL1 engine as a library. A game links HPL and ships the shaders from
# HPL1_SHADER_DIR next to its data - the engine looks for them under
# rehatched/core/programs at runtime (see Resources.cpp).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies)

find_package(SDL2 REQUIRED)
find_package(OpenAL REQUIRED)

# SDL2 only started exporting an imported target fairly late; the build
# container ships 2.0.10, which just sets the old variables.
if(TARGET SDL2::SDL2)
	set(HPL_SDL2_LIBRARIES SDL2::SDL2)
	set(HPL_SDL2_INCLUDE_DIRS "")
else()
	set(HPL_SDL2_LIBRARIES ${SDL2_LIBRARIES})
	set(HPL_SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
	message(STATUS "SDL2: using variables (${SDL2_LIBRARIES})")
endif()

# Handhelds have GLES and no desktop GL at all, so the two are not
# interchangeable here: pick by target architecture.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm")
	find_library(GLES_LIBRARY NAMES GLESv2 REQUIRED)
	find_library(EGL_LIBRARY NAMES EGL REQUIRED)
	set(HPL_GL_LIBRARIES ${GLES_LIBRARY} ${EGL_LIBRARY})
	message(STATUS "Graphics: OpenGL ES (${GLES_LIBRARY})")
else()
	find_package(OpenGL REQUIRED)
	set(HPL_GL_LIBRARIES OpenGL::OpenGL)
	message(STATUS "Graphics: desktop OpenGL")
endif()

# ---- Newton 2.36 -------------------------------------------------------------
# SIMD is x86-only in this version; _SCALAR_ARITHMETIC_ONLY selects the
# portable path, which is also what an aarch64 build needs.
file(GLOB_RECURSE newton_src
    ${DEPS}/sources/Newton/source/core/*.cpp
    ${DEPS}/sources/Newton/source/physics/*.cpp
    ${DEPS}/sources/Newton/source/newton/*.cpp)
# dgVehicleConstraint is bitrotted dead code: it is not in Newton.xcodeproj
# either, and does not compile.
list(FILTER newton_src EXCLUDE REGEX "dgVehicleConstraint")
add_library(newton STATIC ${newton_src})
target_include_directories(newton PUBLIC
    ${DEPS}/sources/Newton/source/core
    ${DEPS}/sources/Newton/source/physics
    ${DEPS}/sources/Newton/source/newton)
target_compile_definitions(newton PUBLIC _SCALAR_ARITHMETIC_ONLY)
target_compile_options(newton PRIVATE -w)

# ---- AngelScript -------------------------------------------------------------
file(GLOB angelscript_src ${DEPS}/sources/AngelScript/source/*.cpp)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    list(APPEND angelscript_src ${DEPS}/sources/AngelScript/source/as_callfunc_arm64_gcc.S)
    enable_language(ASM)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    list(APPEND angelscript_src ${DEPS}/sources/AngelScript/source/as_callfunc_arm_gcc.S)
    enable_language(ASM)
endif()
add_library(angelscript STATIC ${angelscript_src})
target_include_directories(angelscript PUBLIC ${DEPS}/sources/AngelScript/include)

# AngelScript's threading uses pthreads, which older glibc keeps separate.
find_package(Threads REQUIRED)
target_link_libraries(angelscript PUBLIC Threads::Threads)
target_compile_options(angelscript PRIVATE -w)

# ---- vendored single-file libs ------------------------------------------------
add_library(vendor STATIC
    ${DEPS}/sources/tinyXML/tinyxml.cpp
    ${DEPS}/sources/tinyXML/tinyxmlerror.cpp
    ${DEPS}/sources/tinyXML/tinyxmlparser.cpp
    ${DEPS}/sources/tinyXML/tinystr.cpp
    ${DEPS}/sources/stb/stb_image.c
    ${DEPS}/sources/stb/stb_vorbis.c
    ${DEPS}/sources/cgltf/cgltf_write.c)   # also the CGLTF_IMPLEMENTATION TU
target_include_directories(vendor PUBLIC ${DEPS}/sources ${DEPS}/include)
target_compile_options(vendor PRIVATE -w)

# ---- Dear ImGui (debug menu, SDL2 + GL3 backends) -----------------------------
add_library(imgui STATIC
    ${DEPS}/sources/imgui/imgui.cpp
    ${DEPS}/sources/imgui/imgui_draw.cpp
    ${DEPS}/sources/imgui/imgui_tables.cpp
    ${DEPS}/sources/imgui/imgui_widgets.cpp
    ${DEPS}/sources/imgui/backends/imgui_impl_sdl2.cpp
    ${DEPS}/sources/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${DEPS}/sources ${DEPS}/sources/imgui)
# ImGui's GL loader calls dlsym, which on older glibc lives in libdl.
target_link_libraries(imgui PUBLIC ${HPL_SDL2_LIBRARIES} ${HPL_GL_LIBRARIES} ${CMAKE_DL_LIBS})
target_include_directories(imgui PUBLIC ${HPL_SDL2_INCLUDE_DIRS})
target_compile_options(imgui PRIVATE -w)

# ---- HPL engine --------------------------------------------------------------
file(GLOB_RECURSE hpl_src ${CMAKE_CURRENT_SOURCE_DIR}/HPL/sources/*.cpp)
list(FILTER hpl_src EXCLUDE REGEX "LowLevelSystemMac")
add_library(HPL STATIC ${hpl_src})
target_include_directories(HPL PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/HPL/sources
    ${CMAKE_CURRENT_SOURCE_DIR}/compat          # provides GL/gl3.h on non-Apple
    ${DEPS}/include
    ${DEPS}/sources)
target_link_libraries(HPL PUBLIC
    newton angelscript vendor imgui
    ${HPL_SDL2_LIBRARIES} ${HPL_GL_LIBRARIES} ${OPENAL_LIBRARY})
target_include_directories(HPL PUBLIC ${OPENAL_INCLUDE_DIR} ${HPL_SDL2_INCLUDE_DIRS})

# Where a game can find the shaders it has to ship.
set(HPL1_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders CACHE PATH "HPL1 shader sources")
