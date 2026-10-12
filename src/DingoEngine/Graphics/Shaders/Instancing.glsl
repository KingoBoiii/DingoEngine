// Renderer3D's instanced draws, for a vertex stage: one record per copy of a mesh, the mesh's own
// vertices as they are in the file (a_Position, a_Normal, a_TexCoord at locations 0 to 2). A draw's
// records start at Base.x and are found by gl_InstanceIndex from there, never by the draw's first
// instance, which D3D's SV_InstanceID leaves out. Mirrors Renderer3D::InstanceRecord and
// Renderer3D::InstanceDraw. Both blocks are found by name, so a custom shader may move them to
// bindings its own uniforms and textures leave free: define DE_INSTANCE_DATA_BINDING and
// DE_INSTANCE_DRAW_BINDING before including this.

#ifndef DE_INSTANCE_DATA_BINDING
#define DE_INSTANCE_DATA_BINDING 9
#endif
#ifndef DE_INSTANCE_DRAW_BINDING
#define DE_INSTANCE_DRAW_BINDING 10
#endif

struct InstanceRecord
{
	vec4 ModelRows[3];     // the transform's first three rows
	vec4 NormalColumns[3]; // xyz = the inverse transpose of its upper 3 x 3, by column
	vec4 Color;
	uvec4 Misc;            // x = the DE_ObjectData record of a lit mesh, 0 for none
};

layout(std430, binding = DE_INSTANCE_DATA_BINDING) readonly buffer DE_InstanceData
{
	InstanceRecord Instances[];
};

layout(std140, binding = DE_INSTANCE_DRAW_BINDING) uniform DE_InstanceDraw
{
	uvec4 InstanceBase; // x = the draw's first record, y = the views each record is drawn into
};

InstanceRecord GetInstance(uint copy)
{
	return Instances[InstanceBase.x + copy];
}

vec3 InstanceWorldPosition(InstanceRecord instance, vec3 position)
{
	vec4 p = vec4(position, 1.0);
	return vec3(dot(instance.ModelRows[0], p), dot(instance.ModelRows[1], p), dot(instance.ModelRows[2], p));
}

vec3 InstanceWorldNormal(InstanceRecord instance, vec3 normal)
{
	return mat3(instance.NormalColumns[0].xyz, instance.NormalColumns[1].xyz, instance.NormalColumns[2].xyz) * normal;
}
