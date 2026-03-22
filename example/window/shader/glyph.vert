#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
} pushData;

// Per-instance model matrix passed via instanced vertex buffer (binding 0).
layout(location = 0) in vec4 instanceMatrix0;
layout(location = 1) in vec4 instanceMatrix1;
layout(location = 2) in vec4 instanceMatrix2;
layout(location = 3) in vec4 instanceMatrix3;

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
    mat4 instanceMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    vec3 p              = positions[gl_VertexIndex];
    vec4 world          = instanceMatrix * vec4(p, 1.0);
    mat3 normalBasis    = mat3(instanceMatrix);

    outColor         = vec3(0.95, 0.95, 0.98);
    outWorldNormal   = normalize(normalBasis * vec3(0.0, 0.0, 1.0));
    outWorldPosition = world.xyz;
    gl_Position      = pushData.viewProjection * world;
}
