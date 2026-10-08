// The vertex stage of a fullscreen pass: Renderer::Draw(material, 3) draws one triangle over the
// whole target, with no vertex buffer. Include it as the whole vertex stage, after its version line.
// v_TexCoord is (0, 0) at the target's top-left corner, which is its first row on every backend.

layout(location = 0) out vec2 v_TexCoord;

void main()
{
	vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	v_TexCoord = corner;
	gl_Position = vec4(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0, 0.0, 1.0);
}
