#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
} pushData;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outWorldPosition;
layout(location = 2) out vec3 outWorldNormal;

// 4 side triangles (12 vertices) + 2 base triangles (6 vertices) = 18
const vec3 positions[18] = vec3[](
    vec3(-0.4, -0.5,  0.4), vec3( 0.4, -0.5,  0.4), vec3( 0.0,  0.5,  0.0),
    vec3( 0.4, -0.5,  0.4), vec3( 0.4, -0.5, -0.4), vec3( 0.0,  0.5,  0.0),
    vec3( 0.4, -0.5, -0.4), vec3(-0.4, -0.5, -0.4), vec3( 0.0,  0.5,  0.0),
    vec3(-0.4, -0.5, -0.4), vec3(-0.4, -0.5,  0.4), vec3( 0.0,  0.5,  0.0),
    vec3(-0.4, -0.5, -0.4), vec3( 0.4, -0.5, -0.4), vec3( 0.4, -0.5,  0.4),
    vec3(-0.4, -0.5, -0.4), vec3( 0.4, -0.5,  0.4), vec3(-0.4, -0.5,  0.4)
);

const vec3 pyramidColors[5] = vec3[](
    vec3(1.0, 0.5, 0.1),
    vec3(0.9, 0.2, 0.2),
    vec3(0.2, 0.8, 0.3),
    vec3(0.2, 0.5, 0.9),
    vec3(0.7, 0.7, 0.7)
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

    float angle = pushData.animationData.y;
    mat4 model  = rotateY(angle) * rotateX(angle * 0.6);

    int triStart     = (vi / 3) * 3;
    vec3 lp0         = positions[triStart + 0];
    vec3 lp1         = positions[triStart + 1];
    vec3 lp2         = positions[triStart + 2];
    vec3 localNormal = normalize(cross(lp1 - lp0, lp2 - lp0));

    vec4 world   = model * vec4(p, 1.0);
    world.x     += 0.9;
    world.z     -= 2.2;

    // Faces 0-3 are sides, face 4 is the base (both bottom triangles)
    int faceIndex    = vi < 12 ? (vi / 3) : 4;
    outColor         = pyramidColors[faceIndex];
    outWorldNormal   = normalize(mat3(model) * localNormal);
    outWorldPosition = world.xyz;
    gl_Position      = pushData.viewProjection * world;
}
