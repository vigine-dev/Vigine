#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
} pushData;

layout(location = 0) out vec3 outWorldPosition;

const vec3 positions[6] = vec3[](
    vec3(-24.0, -0.75, -26.0), vec3( 24.0, -0.75, -26.0), vec3( 24.0, -0.75,  18.0),
    vec3(-24.0, -0.75, -26.0), vec3( 24.0, -0.75,  18.0), vec3(-24.0, -0.75,  18.0)
);

void main()
{
    vec3 p           = positions[gl_VertexIndex];
    outWorldPosition = p;
    gl_Position      = pushData.viewProjection * vec4(p, 1.0);
}
