#version 450 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;

layout (push_constant) uniform Uniforms
{
    vec2 Scale;
    vec2 Translate;
} u_Uniforms;

struct VertexOutput
{
    vec4 Color;
    vec2 TexCoord;
};

layout(location = 0) out VertexOutput Output;

void main()
{
    gl_Position.xy = a_Position.xy * u_Uniforms.Scale + u_Uniforms.Translate;
    gl_Position.y = -gl_Position.y;
    gl_Position.zw = vec2(0, 1);

    Output.Color = a_Color;
    Output.TexCoord = a_TexCoord;
}
