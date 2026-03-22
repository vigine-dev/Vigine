#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
    mat4 modelMatrix;
} pushData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2  outUV;
layout(location = 1) out float outWorldY;

void main()
{
    gl_Position = pushData.viewProjection * pushData.modelMatrix * vec4(inPosition, 1.0);
    outUV       = inUV;
    // modelMatrix is identity for SDF text (vertices already in world space).
    outWorldY   = inPosition.y;
}
