// DingoUI's one shader: rounded rects with borders as a signed distance (solid or textured) and MSDF
// glyphs, each clipped to its own rect, so a frame of game UI is one draw. Vertices are in target
// pixels, origin top-left, +Y down. Mirrors Internal::UIVertex (DingoUIDrawList.h).

#type vertex
#version 450

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec4 a_BorderColor;
layout(location = 4) in vec4 a_Local;
layout(location = 5) in vec4 a_Radii;
layout(location = 6) in vec4 a_Clip;
layout(location = 7) in vec4 a_Params;

layout(std140, binding = 0) uniform DingoUIFrame
{
	vec2 InvTargetSize;
	vec2 Padding;
};

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec2 v_Local;
layout(location = 2) out flat vec4 v_Color;
layout(location = 3) out flat vec4 v_BorderColor;
layout(location = 4) out flat vec2 v_HalfSize;
layout(location = 5) out flat vec4 v_Radii;
layout(location = 6) out flat vec4 v_Clip;
layout(location = 7) out flat vec4 v_Params;

void main()
{
	v_TexCoord = a_TexCoord;
	v_Local = a_Local.xy;
	v_Color = a_Color;
	v_BorderColor = a_BorderColor;
	v_HalfSize = a_Local.zw;
	v_Radii = a_Radii;
	v_Clip = a_Clip;
	v_Params = a_Params;

	gl_Position = vec4(a_Position.x * InvTargetSize.x * 2.0 - 1.0, 1.0 - a_Position.y * InvTargetSize.y * 2.0, 0.0, 1.0);
}

#type fragment
#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec2 v_Local;
layout(location = 2) in flat vec4 v_Color;
layout(location = 3) in flat vec4 v_BorderColor;
layout(location = 4) in flat vec2 v_HalfSize;
layout(location = 5) in flat vec4 v_Radii;
layout(location = 6) in flat vec4 v_Clip;
layout(location = 7) in flat vec4 v_Params;

layout(location = 0) out vec4 o_Color;

layout(set = 0, binding = 1) uniform texture2D u_Textures[32];
layout(set = 0, binding = 2) uniform sampler u_Sampler;
layout(set = 0, binding = 3) uniform sampler u_PointSampler;

float Median(float r, float g, float b)
{
	return max(min(r, g), min(max(r, g), b));
}

float RoundedRectDistance(vec2 p, vec2 halfSize, vec4 radii)
{
	float radius = p.x < 0.0 ? (p.y < 0.0 ? radii.x : radii.w) : (p.y < 0.0 ? radii.y : radii.z);
	vec2 q = abs(p) - halfSize + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
	// Both filters are sampled before any discard, in uniform control flow, so the implicit derivatives hold.
	int slot = int(v_Params.y);
	vec4 linearTexel = texture(sampler2D(u_Textures[nonuniformEXT(slot)], u_Sampler), v_TexCoord);
	vec4 pointTexel = texture(sampler2D(u_Textures[nonuniformEXT(slot)], u_PointSampler), v_TexCoord);
	vec4 texel = v_Params.z > 1.5 ? pointTexel : linearTexel;

	vec4 color;
	if (v_Params.z > 0.5 && v_Params.z < 1.5)
	{
		float distance = v_Params.w * (Median(texel.r, texel.g, texel.b) - 0.5);
		color = vec4(v_Color.rgb, v_Color.a * clamp(distance + 0.5, 0.0, 1.0));
	}
	else
	{
		float distance = RoundedRectDistance(v_Local, v_HalfSize, v_Radii);
		vec4 fill = texel * v_Color;
		if (v_Params.x > 0.0)
			fill = mix(v_BorderColor, fill, clamp(0.5 - (distance + v_Params.x), 0.0, 1.0));
		color = vec4(fill.rgb, fill.a * clamp(0.5 - distance, 0.0, 1.0));
	}

	vec2 pixel = gl_FragCoord.xy;
	if (color.a <= 0.0 || pixel.x < v_Clip.x || pixel.y < v_Clip.y || pixel.x >= v_Clip.z || pixel.y >= v_Clip.w)
		discard;

	o_Color = color;
}
