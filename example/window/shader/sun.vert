#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
} pushData;

layout(location = 0) out vec3 outColor;

// Параметрична UV-сфера
vec3 sphereVertex(int index)
{
    const int LON_SEGS = 16;
    const int LAT_SEGS = 8;
    const float RADIUS = 0.25;
    const float PI = 3.14159265359;

    // З індексу вершини розраховуємо позицію на сіткі
    int tri_idx = index / 3;
    int vert_in_tri = index % 3;

    // Яка пара широт обробляється?
    int lat_ring = tri_idx / (LON_SEGS * 2);
    int lon_pair = tri_idx % (LON_SEGS * 2);
    int lon_idx = lon_pair / 2;
    int is_lower_tri = lon_pair % 2;

    // Розраховуємо сіткові координати вершини
    int grid_lat, grid_lon;

    if (vert_in_tri == 0)
    {
        grid_lat = lat_ring;
        grid_lon = lon_idx;
    }
    else if (vert_in_tri == 1)
    {
        if (is_lower_tri == 0)
        {
            grid_lat = lat_ring + 1;
            grid_lon = lon_idx;
        }
        else
        {
            grid_lat = lat_ring;
            grid_lon = lon_idx + 1;
        }
    }
    else  // vert_in_tri == 2
    {
        if (is_lower_tri == 0)
        {
            grid_lat = lat_ring;
            grid_lon = lon_idx + 1;
        }
        else
        {
            grid_lat = lat_ring + 1;
            grid_lon = lon_idx + 1;
        }
    }

    // Циклічна довгота, обмежена широта
    grid_lon = grid_lon % LON_SEGS;
    grid_lat = clamp(grid_lat, 0, LAT_SEGS);

    // Сферичні координати
    float theta = 2.0 * PI * float(grid_lon) / float(LON_SEGS);
    float phi = PI * float(grid_lat) / float(LAT_SEGS);

    // Декартові координати
    float sin_phi = sin(phi);
    vec3 local_pos;
    local_pos.x = RADIUS * sin_phi * cos(theta);
    local_pos.y = RADIUS * cos(phi);
    local_pos.z = RADIUS * sin_phi * sin(theta);

    return local_pos;
}

void main()
{
    vec3 local_pos = sphereVertex(gl_VertexIndex);

    // Позиція сонця в світовому просторі
    vec3 sunDirection = normalize(pushData.sunDirectionIntensity.xyz);
    vec3 sunPosition = -sunDirection * 20.0;

    vec4 world = vec4(sunPosition + local_pos, 1.0);
    outColor = vec3(1.0, 0.95, 0.7);  // Теплий жовтий
    gl_Position = pushData.viewProjection * world;
}

