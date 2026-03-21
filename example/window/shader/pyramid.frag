#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
} pushData;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) in vec3 inWorldNormal;
layout(location = 0) out vec4 outFragColor;

void main()
{
    vec3 sunDir       = normalize(pushData.sunDirectionIntensity.xyz);
    float ndotl       = max(dot(normalize(inWorldNormal), -sunDir), 0.0);
    float lightFactor = 0.55 + 0.45 * ndotl;
    outFragColor      = vec4(inColor * lightFactor, 1.0);
}
