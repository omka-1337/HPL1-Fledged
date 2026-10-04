# HPL1 Fledged

This engine stands on **HPL1 Rehatched** by **zenmumbler**, and would not exist
without it. (The upstream `zenmumbler/HPL1R` now 404s; the surviving copy is the
[sysfce2/HPL1R](https://github.com/sysfce2/HPL1R) mirror.) Frictional Games released the
HPL1 sources in 2010, but that code is unbuildable today: its 69 shaders are
written in NVIDIA Cg, a toolkit that was closed, abandoned in 2012, and never
had an ARM build at all. zenmumbler did the hard, unglamorous half — tore Cg
out, replaced it with GLSL, dropped the prebuilt 32-bit physics blob for Newton
built from source, and modernised the C++ until it compiled as one coherent
program again. Everything below is continuation of that work, not a
replacement for it. Thank you.

What was left unfinished there was the renderer: `Renderer3D` was about half the
size of the original's, the twenty-one material classes were folded into a
single `Material_Universal`, and lighting, shadows, normal mapping and the sky
had no implementation behind them. This fork finishes that and adds OpenGL ES,
so the engine runs on ARM Linux generally — single-board computers, handhelds,
anything with GLES 3.0 — and not on one device family. Tested on x86_64 and
aarch64 Linux; the macOS path HPL1 Rehatched had is still in the tree but is not
exercised here, and its `arm64` would currently take the GLES branch wrongly.

## Status

Working: multi-pass lighting with the original's attenuation curve, shadow maps
(2D for spot lights, cube for point lights), tangent-space normal mapping,
illumination maps, blend modes and a transparent pass, the sky, a post-process
stage with gamma and bloom, per-light geometry culling, and desktop GL 4.1 and
OpenGL ES 3.0 from one set of shaders.

Not implemented: specular highlights (`BumpSpecular` materials draw diffuse
only), water, and refraction. Occlusion-query halo fading is binary under GLES,
which has no `GL_SAMPLES_PASSED`.

## Building

    cmake -S . -B build && cmake --build build

Needs SDL2, OpenAL and either desktop GL or GLES. Everything else — Newton
2.36, AngelScript, Dear ImGui, stb, cgltf, tinyXML — is vendored under
`dependencies/` and built from source, so there is nothing to install and no
binary blob in the tree.

GLES is selected by target architecture, not by a flag: an `aarch64` or `arm`
`CMAKE_SYSTEM_PROCESSOR` links `GLESv2` and `EGL`, anything else links desktop
GL. Most ARM systems have no desktop GL to fall back to, which is why this is
not a runtime choice. If yours does, override `HPL_GL_LIBRARIES`.

## Using it in a game

The build produces a static library. A game adds this directory and links
`HPL`:

```cmake
set(HPL1_ENGINE_DIR "${CMAKE_SOURCE_DIR}/../HPL1-Fledged"
    CACHE PATH "Where the HPL1 engine is checked out")
add_subdirectory(${HPL1_ENGINE_DIR} engine)
target_link_libraries(mygame PRIVATE HPL)
```

The shaders are data, not code: the engine loads them at runtime from
`rehatched/core/programs` beside the game's data (the path is hardcoded in
`resources/Resources.cpp`). `HPL1_SHADER_DIR` points at them so a game can copy
them into place:

```cmake
file(COPY ${HPL1_SHADER_DIR}/ DESTINATION ${CMAKE_SOURCE_DIR}/rehatched/core/programs)
```

Shaders are written once, for desktop GL. `GLSLProgram` rewrites the `#version`
line and prepends a precision block when the context is ES, so there is no
second copy to keep in sync.

The Penumbra: Overture PortMaster port is the first game built on this, and a
worked example of all of the above — but nothing here is tied to PortMaster or
to any particular handheld.

## License

GPL-3.0-or-later, and it cannot be anything else. Frictional Games released the
HPL1 engine under the GPL; HPL1 Rehatched is a derivative of it, this is a
derivative of that, and the licence travels with the code. A permissive licence
here would simply be void.

`LICENSE` is the GPL v3 text verbatim. Who wrote what is readable from the file
headers: the 2006–2010 Frictional Games notice marks the original engine
sources, an Omka1337 notice marks files written for this fork — with the help of
Claude Code, which is stated there rather than hidden — and files with neither
are zenmumbler's HPL1 Rehatched additions, which carried no header. All of it is
under the same licence as one work.

The vendored dependencies are all under permissive, GPL-compatible terms — see
`NOTICE` for which is which. OpenAL and SDL2 are linked as system libraries and
are not redistributed here.

No game data is included or implied. Penumbra: Overture's assets stay the
property of Frictional Games; you need your own copy of the game.
