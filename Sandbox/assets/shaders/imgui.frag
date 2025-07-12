#version 450 core

struct VertexOutput
{
    vec4 Color;
    vec2 TexCoord;
};

layout(location = 0) in VertexOutput Input;

layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_Texture;

void main()
{
    o_Color = texture(u_Texture, Input.TexCoord) * Input.Color;
}