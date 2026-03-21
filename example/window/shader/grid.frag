#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
} pushData;

layout(location = 0) in vec3 inWorldPosition;
layout(location = 0) out vec4 outFragColor;

float gridMask(vec2 coord)
{
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    return 1.0 - min(min(grid.x, grid.y), 1.0);
}

void main()
{
    vec3 sunDir        = normalize(pushData.sunDirectionIntensity.xyz);
    float sunIntensity = max(pushData.sunDirectionIntensity.w, 0.0);
    float ambient      = pushData.lightingParams.x;
    float diffuseMult  = pushData.lightingParams.y;
    vec3 gridNormal    = vec3(0.0, 1.0, 0.0);
    float ndotl        = max(dot(gridNormal, -sunDir), 0.0);
    float lightFactor  = clamp(ambient + diffuseMult * ndotl * sunIntensity, 0.0, 2.5);

    vec2  gridCoord = inWorldPosition.xz;
    float minorLine = gridMask(gridCoord);
    float majorLine = gridMask(gridCoord / 5.0);
    float axisX     = 1.0 - min(abs(inWorldPosition.z + 2.45) / 0.04, 1.0);
    float axisZ     = 1.0 - min(abs(inWorldPosition.x) / 0.04, 1.0);
    float fade      = clamp(1.0 - length(inWorldPosition.xz - vec2(0.0, -2.45)) / 32.0, 0.0, 1.0);

    vec3 baseColor  = vec3(0.10, 0.11, 0.13);
    vec3 minorColor = vec3(0.24, 0.25, 0.29);
    vec3 majorColor = vec3(0.38, 0.40, 0.45);
    vec3 axisColor  = vec3(axisX, 0.15, axisZ);

    vec3 color  = mix(baseColor, minorColor, minorLine * 0.55 * fade);
    color       = mix(color, majorColor, majorLine * 0.85 * fade);
    color       = mix(color, axisColor, max(axisX, axisZ) * fade);
    color      *= lightFactor + pushData.lightingParams.z;
    outFragColor = vec4(color, 1.0);
}
