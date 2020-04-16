#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#define VSG_LOC_SHADOW_RAY 2

struct ShadowRayPayload {
    float distance;
};

layout(location = VSG_LOC_SHADOW_RAY) rayPayloadInNV ShadowRayPayload ShadowRay;

void main() {
    ShadowRay.distance = gl_HitTNV;
}
