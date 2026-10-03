// Linux/GLES compatibility shim.
// macOS ships <OpenGL/gl3.h> for the core profile; Linux has no such header,
// so map it onto the Khronos core header. GL_GLEXT_PROTOTYPES makes
// glcorearb.h declare the entry points instead of only the typedefs; glvnd's
// libOpenGL exports them, so no extension loader is needed.
#pragma once
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>

// glcorearb.h omits a few extension enums that Apple's gl3.h exposes.
// Where the extension was promoted to core, alias the core spelling;
// the rest carry their values from the Khronos registry.

// EXT_texture_filter_anisotropic -> core in 4.6
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT GL_TEXTURE_MAX_ANISOTROPY
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT GL_MAX_TEXTURE_MAX_ANISOTROPY
#endif

// EXT_texture_mirror_clamp -> core in 4.4
#ifndef GL_MIRROR_CLAMP_TO_EDGE_EXT
#define GL_MIRROR_CLAMP_TO_EDGE_EXT GL_MIRROR_CLAMP_TO_EDGE
#endif

// EXT_texture_sRGB: never promoted, so no core spelling exists
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT       0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
