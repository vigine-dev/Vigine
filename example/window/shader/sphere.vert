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

vec3 sphereVertex(int index)
{
    const int LON_SEGS = 16;
    const int LAT_SEGS = 8;
    const float PI = 3.14159265359;

    int triIndex = index / 3;
    int vertInTri = index % 3;

    int latRing = triIndex / (LON_SEGS * 2);
    int lonPair = triIndex % (LON_SEGS * 2);
    int lonIndex = lonPair / 2;
    int lowerTri = lonPair % 2;

    int gridLat;
    int gridLon;

    if (vertInTri == 0)
    {
        gridLat = latRing;
        gridLon = lonIndex;
    }
    else if (vertInTri == 1)
    {
        if (lowerTri == 0)
        {
            gridLat = latRing + 1;
            gridLon = lonIndex;
        }
        else
        {
            gridLat = latRing;
            gridLon = lonIndex + 1;
        }
    }
    else
    {
        if (lowerTri == 0)
        {
            gridLat = latRing;
            gridLon = lonIndex + 1;
        }
        else
        {
            gridLat = latRing + 1;
            gridLon = lonIndex + 1;
        }
    }

    gridLon = gridLon % LON_SEGS;
    gridLat = clamp(gridLat, 0, LAT_SEGS);

    float theta = 2.0 * PI * float(gridLon) / float(LON_SEGS);
    float phi = PI * float(gridLat) / float(LAT_SEGS);

    float sinPhi = sin(phi);
    return vec3(sinPhi * cos(theta), cos(phi), sinPhi * sin(theta));
}

void main()
{
    vec3 local = sphereVertex(gl_VertexIndex);
    vec4 world = pushData.modelMatrix * vec4(local, 1.0);
    outColor = vec3(0.0, 0.0, 0.0);
    gl_Position = pushData.viewProjection * world;
}
