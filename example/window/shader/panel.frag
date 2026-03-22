#version 450

layout(push_constant) uniform Push
{
    mat4 viewProjection;
    vec4 animationData;
    vec4 sunDirectionIntensity;
    vec4 lightingParams;
} pushData;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) in vec3 inWorldNormal;
layout(location = 3) in vec2 inLocalPosition;
layout(location = 4) in float inPanelTag;
layout(location = 0) out vec4 outFragColor;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float wrappedDistance(float a, float b, float period)
{
    float d = abs(a - b);
    return min(d, period - d);
}

void main()
{
    // Focus frame pass: white frame + moving rainbow segment.
    // Frame side ID is encoded in scale.z (panel tag) from CPU.
    if (inPanelTag > 0.0205 && inPanelTag < 0.0245)
    {
        // Must match TextEditorSystem values: panel(4.8 x 1.5) + 2*padding(0.012).
        const float frameWidth  = 4.824;
        const float frameHeight = 1.524;
        const float perim       = 2.0 * (frameWidth + frameHeight);

        const float u = clamp(inLocalPosition.x + 0.5, 0.0, 1.0);
        const float v = clamp(inLocalPosition.y + 0.5, 0.0, 1.0);

        // Clockwise perimeter parameterization from top-left.
        float s = 0.0;
        if (inPanelTag < 0.0215) // top
            s = u * frameWidth;
        else if (inPanelTag < 0.0225) // right
            s = frameWidth + (1.0 - v) * frameHeight;
        else if (inPanelTag < 0.0235) // bottom
            s = frameWidth + frameHeight + (1.0 - u) * frameWidth;
        else // left
            s = frameWidth + frameHeight + frameWidth + v * frameHeight;

        const float speed = 0.24;
        const float segLen = 0.66;
        float head = fract(pushData.animationData.x * speed) * perim;
        float dist = wrappedDistance(s, head, perim);

        vec3 baseWhite = vec3(1.0);
        if (dist < segLen * 0.5)
        {
            float local = 1.0 - (dist / (segLen * 0.5));
            float hue = fract(pushData.animationData.x * 0.85 + s * 0.14);
            vec3 rainbow = hsv2rgb(vec3(hue, 0.95, 1.0));
            vec3 color = mix(baseWhite, rainbow, local);
            outFragColor = vec4(color, 1.0);
        }
        else
        {
            outFragColor = vec4(baseWhite, 1.0);
        }
        return;
    }

    vec3 sunDir        = normalize(pushData.sunDirectionIntensity.xyz);
    float sunIntensity = max(pushData.sunDirectionIntensity.w, 0.0);
    float ambient      = 0.22;
    float diffuseMult  = 0.55;
    float ndotl        = max(dot(normalize(inWorldNormal), -sunDir), 0.0);
    float lightFactor  = clamp(ambient + diffuseMult * ndotl * sunIntensity, 0.0, 3.0);

    outFragColor = vec4(inColor * lightFactor, 1.0);
}
