#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
    mat4 modelMatrix;
} pushData;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outWorldPosition;
layout(location = 2) out vec3 outWorldNormal;

const vec3 positions[6] = vec3[](
    vec3(-0.5, -0.5, 0.0),
    vec3( 0.5, -0.5, 0.0),
    vec3( 0.5,  0.5, 0.0),
    vec3(-0.5, -0.5, 0.0),
    vec3( 0.5,  0.5, 0.0),
    vec3(-0.5,  0.5, 0.0)
);

void main()
{
    vec3 p           = positions[gl_VertexIndex];
    vec4 world       = pushData.modelMatrix * vec4(p, 1.0);
    mat3 normalBasis = mat3(pushData.modelMatrix);

    // UI layering by Z: background panel (base), scrollbar track (mid), thumb (bright).
    const float zLayer = pushData.modelMatrix[3][2];
    if (zLayer > 1.2085)
        outColor = vec3(1.0); // Focus frame base is white; moving rainbow segment is added in frag.
    else if (zLayer > 1.2055)
        outColor = vec3(0.78, 0.82, 0.95); // thumb
    else if (zLayer > 1.2030)
        outColor = vec3(0.22, 0.26, 0.40); // track
    else
        outColor = vec3(0.12, 0.16, 0.32); // background panel
    outWorldNormal   = normalize(normalBasis * vec3(0.0, 0.0, 1.0));
    outWorldPosition = world.xyz;
    gl_Position      = pushData.viewProjection * world;
}
