#version 450

layout(push_constant) uniform Push
{
    float cubeAngle;
    float pyramidAngle;
    float aspect;
} pushData;

layout(location = 0) out vec3 outColor;

const int cubeVertexCount = 36;

const vec3 positions[54] = vec3[](
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
    vec3(-0.5, -0.5, -0.5), vec3(-0.5,  0.5,  0.5), vec3(-0.5,  0.5, -0.5),

    // Pyramid local geometry (18 vertices): 4 side triangles + 2 base triangles.
    vec3(-0.4, -0.5,  0.4), vec3( 0.4, -0.5,  0.4), vec3( 0.0,  0.5,  0.0),
    vec3( 0.4, -0.5,  0.4), vec3( 0.4, -0.5, -0.4), vec3( 0.0,  0.5,  0.0),
    vec3( 0.4, -0.5, -0.4), vec3(-0.4, -0.5, -0.4), vec3( 0.0,  0.5,  0.0),
    vec3(-0.4, -0.5, -0.4), vec3(-0.4, -0.5,  0.4), vec3( 0.0,  0.5,  0.0),
    vec3(-0.4, -0.5, -0.4), vec3( 0.4, -0.5, -0.4), vec3( 0.4, -0.5,  0.4),
    vec3(-0.4, -0.5, -0.4), vec3( 0.4, -0.5,  0.4), vec3(-0.4, -0.5,  0.4)
);

const vec3 cubeColors[6] = vec3[](
    vec3(1.0, 0.1, 0.1),
    vec3(0.1, 1.0, 0.1),
    vec3(0.1, 0.1, 1.0),
    vec3(1.0, 1.0, 0.1),
    vec3(0.1, 1.0, 1.0),
    vec3(1.0, 0.1, 1.0)
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

mat4 perspective(float fovy, float aspect, float zNear, float zFar)
{
    float f = 1.0 / tan(fovy * 0.5);
    return mat4(
        f / aspect, 0, 0, 0,
        0, -f, 0, 0,
        0, 0, zFar / (zNear - zFar), -1,
        0, 0, (zNear * zFar) / (zNear - zFar), 0
    );
}

void main()
{
    int vertexIndex = gl_VertexIndex;
    bool isCube = vertexIndex < cubeVertexCount;
    vec3 p = positions[vertexIndex];
    float objectAngle = isCube ? pushData.cubeAngle : pushData.pyramidAngle;
    mat4 model = rotateY(objectAngle) * rotateX(objectAngle * 0.6);
    float aspect = max(pushData.aspect, 0.0001);
    mat4 proj = perspective(radians(60.0), aspect, 0.1, 10.0);
    vec4 world = model * vec4(p, 1.0);

    world.x += isCube ? -0.9 : 0.9;
    world.z -= isCube ? 2.75 : 2.2;

    gl_Position = proj * world;
    if (isCube)
    {
        outColor = cubeColors[vertexIndex / 6];
    }
    else
    {
        int pyramidVertex = vertexIndex - cubeVertexCount;
        int faceIndex = pyramidVertex < 12 ? (pyramidVertex / 3) : 4;
        outColor = pyramidColors[faceIndex];
    }
}
