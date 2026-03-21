#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
    mat4 modelMatrix;
} pushData;

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

layout(location = 0) in  vec2  inUV;
layout(location = 1) in  float inWorldY;
layout(location = 0) out vec4  outFragColor;

void main()
{
    // animationData.z = clipYMin (world), animationData.w = clipYMax (world).
    // Clipping is active when clipYMax > 0.
    if (pushData.animationData.w > 0.0 &&
        (inWorldY < pushData.animationData.z || inWorldY > pushData.animationData.w))
        discard;

    float dist  = texture(fontAtlas, inUV).r;
    // smoothstep edges: 0.45..0.55 gives crisp antialiased text at any scale
    float alpha = smoothstep(0.45, 0.55, dist);
    if (alpha < 0.01)
        discard;
    outFragColor = vec4(1.0, 1.0, 1.0, alpha);
}
