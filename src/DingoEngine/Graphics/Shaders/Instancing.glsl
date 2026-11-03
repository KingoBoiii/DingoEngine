// Renderer3D's instanced draws, for a vertex stage: one record per copy of a mesh, the mesh's own
// vertices as they are in the file (a_Position, a_Normal, a_TexCoord at locations 0 to 2). A draw's
// records start at Base.x and are found by gl_InstanceIndex from there, never by the draw's first
// instance, which D3D's SV_InstanceID leaves out. Mirrors Renderer3D::InstanceRecord and
// Renderer3D::InstanceDraw. Both blocks are found by name, so a custom shader may move them to
// bindings its own uniforms and textures leave free: define DE_INSTANCE_DATA_BINDING and
// DE_INSTANCE_DRAW_BINDING before including this.
//
// With DE_INSTANCE_DRAW_PUSH (the engine's shaders) DE_InstanceDraw is a push-constant block
// instead, which a draw sets without writing a buffer, so on Vulkan the draws of a pass share one
// render pass. The renderer then owns the shader's push constants: it declares no other push block.

#ifndef DE_INSTANCE_DATA_BINDING
#define DE_INSTANCE_DATA_BINDING 9
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

#ifdef DE_INSTANCE_DRAW_PUSH
layout(push_constant) uniform DE_InstanceDraw
#else
#ifndef DE_INSTANCE_DRAW_BINDING
#define DE_INSTANCE_DRAW_BINDING 10
#endif
layout(std140, binding = DE_INSTANCE_DRAW_BINDING) uniform DE_InstanceDraw
#endif
{
	uvec4 InstanceBase; // x = the draw's first record, y = the views each record is drawn into
};

InstanceRecord GetInstance(uint copy)
{
	return Instances[InstanceBase.x + copy];
}

// Both sum in glm's order and never fuse, so a copy lands on exactly the bits the CPU batch gives
// the same vertex: otherwise an edge pixel can flip to whatever lies behind it.
vec3 InstanceWorldPosition(InstanceRecord instance, vec3 position)
{
	mat4x3 model = transpose(mat3x4(instance.ModelRows[0], instance.ModelRows[1], instance.ModelRows[2]));
	precise vec3 world = (model[0] * position.x + model[1] * position.y) + (model[2] * position.z + model[3]);
	return world;
}

vec3 InstanceWorldNormal(InstanceRecord instance, vec3 normal)
{
	precise vec3 world = instance.NormalColumns[0].xyz * normal.x + instance.NormalColumns[1].xyz * normal.y + instance.NormalColumns[2].xyz * normal.z;
	return world;
}
