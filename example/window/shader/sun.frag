#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outFragColor;

void main()
{
    // Сонце світить без затінення - просто яскравий емісійний колір
    // Множимо на 2, щоб сонце було яскраво помітне навіть при камері на помірній відстані
    outFragColor = vec4(inColor * 2.0, 1.0);
}
