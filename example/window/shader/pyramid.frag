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
    vec3 sunDir        = normalize(pushData.sunDirectionIntensity.xyz);
    float sunIntensity = max(pushData.sunDirectionIntensity.w, 0.0);
    float ambient      = pushData.lightingParams.x;
    float diffuseMult  = pushData.lightingParams.y;
    float ndotl        = max(dot(normalize(inWorldNormal), -sunDir), 0.0);
    float lightFactor  = clamp(ambient + diffuseMult * ndotl * sunIntensity, 0.0, 2.5);

    outFragColor = vec4(inColor * lightFactor, 1.0);
}
