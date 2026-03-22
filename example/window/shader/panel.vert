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
layout(location = 3) out vec2 outLocalPosition;
layout(location = 4) out float outPanelTag;

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
    const float sz   = length(pushData.modelMatrix[2].xyz);

    // Classify UI panel variants by geometry proportions, not absolute Z.
    // This keeps colors/styling stable when editor is moved in depth.
    const float sx = length(pushData.modelMatrix[0].xyz);
    const float sy = length(pushData.modelMatrix[1].xyz);
    const float thin = min(sx, sy);
    const float wide = max(sx, sy);

    // Focus frame lines are very thin strips (thickness ~0.022) with one long side.
    if (thin <= 0.03 && wide >= 1.0)
        outColor = vec3(1.0); // focus frame base (rainbow segment in frag)
    // Scrollbar track is a narrow vertical bar (~0.06 x ~1.3+).
    else if (sx <= 0.08 && sy >= 0.8)
        outColor = vec3(0.22, 0.26, 0.40); // track
    // Scrollbar thumb is also narrow but shorter than the track.
    else if (sx <= 0.08 && sy >= 0.12 && sy < 0.8)
        outColor = vec3(0.78, 0.82, 0.95); // thumb
    else
        outColor = vec3(0.12, 0.16, 0.32); // background panel
    outWorldNormal   = normalize(normalBasis * vec3(0.0, 0.0, 1.0));
    outWorldPosition = world.xyz;
    outLocalPosition = p.xy;
    outPanelTag      = sz;
    gl_Position      = pushData.viewProjection * world;
}
