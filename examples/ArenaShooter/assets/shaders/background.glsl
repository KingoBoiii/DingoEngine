#type vertex
#version 450

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 vUV;

void main()
{
    vUV = inPosition * 0.5 + 0.5;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}

#type fragment
#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(std140, set = 0, binding = 0) uniform Background
{
    vec2 uResolution;
    float uTime;
    float uPad;
};

void main()
{
    vec2 uv = vUV;
    vec2 p = (uv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    vec3 top = vec3(0.03, 0.05, 0.12);
    vec3 bottom = vec3(0.10, 0.04, 0.16);
    vec3 base = mix(bottom, top, uv.y);

    vec2 grid = uv * 24.0;
    grid.x += sin(uv.y * 6.2831 + uTime * 0.6) * 0.35;
    grid.y += cos(uv.x * 6.2831 - uTime * 0.4) * 0.35;
    vec2 gf = abs(fract(grid) - 0.5);
    float line = smoothstep(0.46, 0.5, max(gf.x, gf.y));
    vec3 gridColor = vec3(0.15, 0.45, 0.6) * line * (0.35 + 0.25 * sin(uTime));

    float pulse = 0.5 + 0.5 * sin(uTime * 1.5);
    float glow = 0.12 / (0.12 + dot(p, p) * 3.0);
    vec3 center = vec3(0.20, 0.35, 0.55) * glow * (0.6 + 0.4 * pulse);

    float vign = smoothstep(1.1, 0.2, length(p));

    vec3 color = (base + gridColor + center) * vign;
    outColor = vec4(color, 1.0);
}
