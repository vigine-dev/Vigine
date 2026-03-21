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
layout(location = 2) flat out int outSurfaceKind;
layout(location = 3) out vec3 outWorldNormal;

const int cubeVertexCount = 36;
const int pyramidVertexCount = 18;
const int gridVertexCount = 6;
const int gridStartVertex = cubeVertexCount + pyramidVertexCount;

const vec3 positions[60] = vec3[](
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
    vec3(-0.4, -0.5, -0.4), vec3( 0.4, -0.5,  0.4), vec3(-0.4, -0.5,  0.4),

    vec3(-24.0, -0.75, -26.0), vec3( 24.0, -0.75, -26.0), vec3( 24.0, -0.75,  18.0),
    vec3(-24.0, -0.75, -26.0), vec3( 24.0, -0.75,  18.0), vec3(-24.0, -0.75,  18.0)
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

void main()
{
    int vertexIndex = gl_VertexIndex;
    bool isCube = vertexIndex < cubeVertexCount;
    bool isPyramid = vertexIndex >= cubeVertexCount && vertexIndex < gridStartVertex;
    bool isGrid = vertexIndex >= gridStartVertex && vertexIndex < (gridStartVertex + gridVertexCount);
    vec3 p = positions[vertexIndex];
    vec4 world;
    vec3 worldNormal;

    if (isGrid)
    {
        world = vec4(p, 1.0);
        outColor = vec3(0.0);
        outSurfaceKind = 1;
        outWorldNormal = vec3(0.0, 1.0, 0.0);
    }
    else
    {
        float objectAngle = isCube ? pushData.animationData.x : pushData.animationData.y;
        mat4 model = rotateY(objectAngle) * rotateX(objectAngle * 0.6);

        int triStart = (vertexIndex / 3) * 3;
        vec3 lp0 = positions[triStart + 0];
        vec3 lp1 = positions[triStart + 1];
        vec3 lp2 = positions[triStart + 2];
        vec3 localNormal = normalize(cross(lp1 - lp0, lp2 - lp0));
        worldNormal = normalize(mat3(model) * localNormal);

        world = model * vec4(p, 1.0);

        world.x += isCube ? -0.9 : 0.9;
        world.z -= isCube ? 2.75 : 2.2;
        outSurfaceKind = 0;

        if (isCube)
        {
            outColor = cubeColors[vertexIndex / 6];
        }
        else if (isPyramid)
        {
            int pyramidVertex = vertexIndex - cubeVertexCount;
            int faceIndex = pyramidVertex < 12 ? (pyramidVertex / 3) : 4;
            outColor = pyramidColors[faceIndex];
        }

        outWorldNormal = worldNormal;
    }

    gl_Position = pushData.viewProjection * world;
    outWorldPosition = world.xyz;
}
