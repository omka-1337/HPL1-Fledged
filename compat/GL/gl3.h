// Copyright (C) 2026 - Omka1337, written with the help of Claude Code
// SPDX-License-Identifier: GPL-3.0-or-later
// GL header shim.
//
// macOS ships <OpenGL/gl3.h>; Linux has no such header, and handhelds have
// GLES instead of desktop GL. This picks the right one and fills in the few
// constants the engine names that a given header does not declare.
#pragma once

#if defined(__arm__) || defined(__aarch64__)

	#include <GLES3/gl3.h>
	#include <GLES2/gl2ext.h>

	// Targets and queries that simply do not exist in GLES. The code still
	// names them, but guards their use at runtime (see TextureTargetToGL and
	// cOcclusionQueryOGL::QueryTarget), so only the symbols are needed.
	#ifndef GL_TEXTURE_1D
	#define GL_TEXTURE_1D 0x0DE0
	#endif
	#ifndef GL_SAMPLES_PASSED
	#define GL_SAMPLES_PASSED 0x8914
	#endif

	// EXT_texture_filter_anisotropic / EXT_texture_mirror_clamp
	#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
	#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
	#endif
	#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
	#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
	#endif
	#ifndef GL_MIRROR_CLAMP_TO_EDGE_EXT
	#define GL_MIRROR_CLAMP_TO_EDGE_EXT 0x8743
	#endif

	// EXT_texture_compression_s3tc, present on most of these GPUs as an
	// extension even though the header may not declare it.
	#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
	#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT  0x83F0
	#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
	#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
	#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
	#endif

	// GLES clamps to edge only; border clamp is the nearest behaviour, and the
	// one place that asked for a border (the shadow map) checks bounds itself.
	#ifndef GL_CLAMP_TO_BORDER
	#define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
	#endif

	// Named in the pixel-format table but never produced: the loaders hand over
	// RGB/RGBA. Defined so the table compiles.
	#ifndef GL_BGR
	#define GL_BGR GL_RGB
	#endif
	#ifndef GL_BGRA
	#define GL_BGRA GL_RGBA
	#endif

	// No quad primitive in GLES. The only quad buffer left is the old skybox
	// one, which is built but never drawn - the sky is a full-screen pass now.
	#ifndef GL_QUADS
	#define GL_QUADS GL_TRIANGLES
	#endif

#elif defined(__APPLE__)

	#include <OpenGL/gl3.h>

#else

	// glvnd's libOpenGL exports the entry points directly, so no loader is
	// needed; GL_GLEXT_PROTOTYPES makes glcorearb declare them.
	#define GL_GLEXT_PROTOTYPES 1
	#include <GL/glcorearb.h>

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

#endif

// EXT_texture_sRGB was never promoted, so no header declares a core spelling.
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT       0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
