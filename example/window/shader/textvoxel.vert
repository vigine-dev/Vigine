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

const vec3 faceColors[6] = vec3[](
    vec3(0.95, 0.92, 0.90),
    vec3(0.82, 0.80, 0.78),
    vec3(0.90, 0.88, 0.86),
    vec3(0.72, 0.70, 0.68),
    vec3(0.86, 0.84, 0.82),
    vec3(0.76, 0.74, 0.72)
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
    int vi = gl_VertexIndex;
    vec3 p = positions[vi];

    int triStart     = (vi / 3) * 3;
    vec3 lp0         = positions[triStart + 0];
    vec3 lp1         = positions[triStart + 1];
    vec3 lp2         = positions[triStart + 2];
    vec3 localNormal = normalize(cross(lp1 - lp0, lp2 - lp0));

    float angle      = pushData.animationData.x * 1.35;
    mat4 spin        = rotateY(angle) * rotateX(angle * 0.65);
    mat4 instanceMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    mat4 modelMatrix = instanceMatrix * spin;
    vec4 world       = modelMatrix * vec4(p, 1.0);

    vec3 seedColor = fract(abs(instanceMatrix[3].xyz) * vec3(0.73, 1.31, 1.91));
    outColor       = mix(faceColors[vi / 6], seedColor, 0.55);
    outWorldNormal   = normalize(mat3(modelMatrix) * localNormal);
    outWorldPosition = world.xyz;
    gl_Position      = pushData.viewProjection * world;
}
