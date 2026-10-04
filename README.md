# HPL1 Fledged

The HPL1 engine — Frictional Games' engine behind the Penumbra series —
continued from [HPL1 Rehatched](https://github.com/FrictionalGames/HPL1Engine)
with the renderer finished and a path onto ARM handhelds.

Built as a static library: a game links `HPL` and ships the shaders from
`shaders/` as `rehatched/core/programs` beside its data.

## What was added

- Materials: blend modes, a working transparent pass, illumination maps,
  normal mapping, and the `DepthTest` flag the format always carried.
- Lighting: an ambient base pass and one additive pass per light, with the
  attenuation curve the original sampled from a texture.
- Shadows: depth maps for spot lights and cube maps for point lights.
- A post-process stage with gamma and bloom, and the sky.
- Per-light geometry culling, which is what makes a weak CPU viable.
- OpenGL ES 3.0 alongside desktop GL, from one set of shaders.

## Building

    cmake -S . -B build && cmake --build build

Needs SDL2, OpenAL, and either desktop GL or GLES. Everything else — Newton,
AngelScript, Dear ImGui, stb, cgltf, tinyXML — is vendored under
`dependencies/`.
