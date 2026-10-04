// Copyright (C) 2026 - Omka1337, written with the help of Claude Code
// SPDX-License-Identifier: GPL-3.0-or-later
// Fragment func for one additive light pass over the scene.
// Runs once per light; results sum in the framebuffer onto the ambient pass.
#version 410

in vec3 oWorldPos;
in vec3 oLightVecTS;
in vec4 oColor;
in vec2 oUV;

layout(location = 0) out vec4 fragColor;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform float useBump;
uniform float alphaCutoff;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float lightRadius;

// Spot lights cone the contribution; point lights leave this at 0.
// A float because iGpuProgram has no integer setter.
uniform float lightIsSpot;
uniform vec3 lightDir;
uniform float lightCosFov;

// Shadows. shadowKind: 0 none, 1 spot (2D map), 2 point (cube map).
// A float because iGpuProgram has no integer setter.
uniform float shadowKind;

uniform sampler2DShadow shadowMap;
uniform mat4 lightViewProj;

uniform samplerCubeShadow shadowCube;
uniform float shadowNear;
uniform float shadowFar;

float ShadowFactor(vec3 avWorldPos) {
	vec4 lightSpace = lightViewProj * vec4(avWorldPos, 1.0);
	if (lightSpace.w <= 0.0) return 1.0;

	vec3 proj = (lightSpace.xyz / lightSpace.w) * 0.5 + 0.5;

	// Outside the map, or past its far plane, nothing was recorded to occlude
	// against - the edge clamp would otherwise smear the border sample.
	if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0) {
		return 1.0;
	}

	// The polygon offset in the depth pass does the real work; keep this tiny,
	// or the shadow lifts away from where the caster meets the floor.
	return texture(shadowMap, vec3(proj.xy, proj.z - 0.0004));
}

float CubeShadowFactor(vec3 avWorldPos) {
	vec3 dir = avWorldPos - lightPos;

	// Each face was rendered with a 90 degree frustum, so the depth that was
	// recorded for this direction belongs to its dominant axis. Rebuild that
	// same non-linear value here instead of storing distance separately.
	vec3 a = abs(dir);
	float localZ = max(a.x, max(a.y, a.z));
	if (localZ <= 0.0) return 1.0;

	float n = shadowNear;
	float f = shadowFar;
	float normZ = (f + n) / (f - n) - (2.0 * f * n) / ((f - n) * localZ);
	float ref = normZ * 0.5 + 0.5;

	if (ref > 1.0) return 1.0;

	return texture(shadowCube, vec4(dir, ref - 0.001));
}

void main() {
	vec4 diffuse = texture(diffuseMap, oUV);
	// Cut-outs must agree with the ambient pass or their holes would light up.
	if (diffuse.a < alphaCutoff) discard;

	vec3 toLight = lightPos - oWorldPos;
	float dist = length(toLight);
	if (dist >= lightRadius) discard;

	// Kept in world space for the spot cone below.
	vec3 Lworld = toLight / max(dist, 0.0001);

	// Lit against the normal map rather than the interpolated vertex normal -
	// this is what gives stone and wood their relief. Materials without a map
	// are handed a flat one, so there is no special case here.
	// useBump=0 falls back to the flat tangent-space normal, so the normal map
	// can be taken out of the picture without a rebuild.
	vec3 bumpVec = (useBump > 0.5)
		? texture(normalMap, oUV).xyz * 2.0 - 1.0
		: vec3(0.0, 0.0, 1.0);
	float bumpLen = length(bumpVec);
	bumpVec = (bumpLen > 0.0001) ? bumpVec / bumpLen : vec3(0.0, 0.0, 1.0);

	float lightLen = length(oLightVecTS);
	vec3 Lts = (lightLen > 0.0001) ? oLightVecTS / lightLen : vec3(0.0, 0.0, 1.0);

	// No discard here. A normal map makes this flip sign from texel to texel,
	// and killing fragments on it speckles every dim grazing surface. Zero
	// simply contributes nothing to an additive pass.
	float ndotl = max(dot(Lts, bumpVec), 0.0);

	// The original read this out of core_falloff_linear, indexed by
	// dot(lightDir, lightDir) - the SQUARED normalised distance, not the
	// distance (see Bump_Light_fp.cg). Fitted against all 256 texels with that
	// argument, max error 0.066; getting the argument wrong makes every light
	// pool far too tight. A texture lookup would be exact, but it is a 1D
	// texture and GLES has none.
	float x2 = (dist * dist) / (lightRadius * lightRadius);
	float t = clamp(1.0 - x2, 0.0, 1.0);
	float atten = t * t * (3.0 - 2.0 * t);

	float cone = 1.0;
	if (lightIsSpot > 0.5) {
		float cosAngle = dot(-Lworld, normalize(lightDir));
		cone = smoothstep(lightCosFov, mix(lightCosFov, 1.0, 0.25), cosAngle);
	}

	float shadow = 1.0;
	if (shadowKind > 1.5) {
		shadow = CubeShadowFactor(oWorldPos);
	}
	else if (shadowKind > 0.5) {
		shadow = ShadowFactor(oWorldPos);
	}
	// Likewise left to multiply out rather than discard: the shadow term is
	// filtered, so cutting on it would alias along every shadow edge.

	vec3 lit = diffuse.rgb * oColor.rgb * lightColor * (ndotl * atten * cone * shadow);
	fragColor = vec4(lit, 1.0);
}
