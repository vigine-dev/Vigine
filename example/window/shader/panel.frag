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
    if (inWorldPosition.z > 1.2085)
    {
        // Frame bounds should match TextEditorSystem focus frame geometry.
        const float leftX   = -2.412;
        const float rightX  =  2.412;
        const float topY    =  2.412;
        const float bottomY =  0.888;

        const float width   = rightX - leftX;
        const float height  = topY - bottomY;
        const float perim   = 2.0 * (width + height);

        // Parameterize perimeter clockwise from top-left corner.
        float s = 0.0;
        float dTop = abs(inWorldPosition.y - topY);
        float dRight = abs(inWorldPosition.x - rightX);
        float dBottom = abs(inWorldPosition.y - bottomY);
        float dLeft = abs(inWorldPosition.x - leftX);
        float minD = min(min(dTop, dRight), min(dBottom, dLeft));

        if (minD == dTop)
            s = clamp(inWorldPosition.x - leftX, 0.0, width);
        else if (minD == dRight)
            s = width + clamp(topY - inWorldPosition.y, 0.0, height);
        else if (minD == dBottom)
            s = width + height + clamp(rightX - inWorldPosition.x, 0.0, width);
        else
            s = width + height + width + clamp(inWorldPosition.y - bottomY, 0.0, height);

        const float speed = 0.24;           // 2x slower movement
        const float segLen = 0.66;          // medium tail length
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
