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

const vec3 positions[36] = vec3[](
    vec3(-0.5, -0.5,  0.5), vec3( 0.5, -0.5,  0.5), vec3( 0.5,  0.5,  0.5),
    vec3(-0.5, -0.5,  0.5), vec3( 0.5,  0.5,  0.5), vec3(-0.5,  0.5,  0.5),

    vec3( 0.5, -0.5, -0.5), vec3(-0.5, -0.5, -0.5), vec3(-0.5,  0.5, -0.5),
    vec3( 0.5, -0.5, -0.5), vec3(-0.5,  0.5, -0.5), vec3( 0.5,  0.5, -0.5),

    vec3(-0.5,  0.5, -0.5), vec3( 0.5,  0.5, -0.5), vec3( 0.5,  0.5,  0.5),
    vec3(-0.5,  0.5, -0.5), vec3( 0.5,  0.5,  0.5), vec3(-0.5,  0.5,  0.5),

    vec3(-0.5, -0.5,  0.5), vec3( 0.5, -0.5,  0.5), vec3( 0.5, -0.5, -0.5),
    vec3(-0.5, -0.5,  0.5), vec3( 0.5, -0.5, -0.5), vec3(-0.5, -0.5, -0.5),

    vec3( 0.5, -0.5,  0.5), vec3( 0.5, -0.5, -0.5), vec3( 0.5,  0.5, -0.5),
    vec3( 0.5, -0.5,  0.5), vec3( 0.5,  0.5, -0.5), vec3( 0.5,  0.5,  0.5),

    vec3(-0.5, -0.5, -0.5), vec3(-0.5, -0.5,  0.5), vec3(-0.5,  0.5,  0.5),
    vec3(-0.5, -0.5, -0.5), vec3(-0.5,  0.5,  0.5), vec3(-0.5,  0.5, -0.5)
);

const vec3 cubeColors[6] = vec3[](
    vec3(1.0, 0.1, 0.1),
    vec3(0.1, 1.0, 0.1),
    vec3(0.1, 0.1, 1.0),
    vec3(1.0, 1.0, 0.1),
    vec3(0.1, 1.0, 1.0),
    vec3(1.0, 0.1, 1.0)
);

mat4 rotateX(float a)
{
    float c = cos(a);
    float s = sin(a);
    return mat4(
        1, 0, 0, 0,
        0, c, s, 0,
        0,-s, c, 0,
        0, 0, 0, 1
    );
}

mat4 rotateY(float a)
{
    float c = cos(a);
    float s = sin(a);
    return mat4(
         c, 0,-s, 0,
         0, 1, 0, 0,
         s, 0, c, 0,
         0, 0, 0, 1
    );
}

void main()
{
    int vi  = gl_VertexIndex;
    vec3 p  = positions[vi];

    float angle = pushData.animationData.x;
    mat4 model  = rotateY(angle) * rotateX(angle * 0.6);

    int triStart     = (vi / 3) * 3;
    vec3 lp0         = positions[triStart + 0];
    vec3 lp1         = positions[triStart + 1];
    vec3 lp2         = positions[triStart + 2];
    vec3 localNormal = normalize(cross(lp1 - lp0, lp2 - lp0));

    mat4 modelMatrix = pushData.modelMatrix * model;
    vec4 world       = modelMatrix * vec4(p, 1.0);

    outColor         = cubeColors[vi / 6];
    outWorldNormal   = normalize(mat3(modelMatrix) * localNormal);
    outWorldPosition = world.xyz;
    gl_Position      = pushData.viewProjection * world;
}
