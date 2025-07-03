#include "SandboxApplication.h"

#include "Layers/StaticTriangleLayer.h"
#include "Layers/VertexBufferTriangleLayer.h"
#include "Layers/IndexBufferQuadLayer.h"
#include "Layers/CameraTransformationQuadLayer.h"

#define RENDER_STATIC_TRIANGLE 0
#define RENDER_VERTEX_BUFFER_TRIANGLE 0
#define RENDER_INDEX_BUFFER_QUAD 0
#define RENDER_CAMERA_TRANSFORMATION_QUAD 1

void SandboxApplication::OnInitialize()
{
#if RENDER_STATIC_TRIANGLE
	PushLayer(new StaticTriangleLayer());
#endif // RENDER_STATIC_TRIANGLE

#if RENDER_VERTEX_BUFFER_TRIANGLE
	PushLayer(new VertexBufferTriangleLayer());
#endif // RENDER_VERTEX_BUFFER_TRIANGLE

#if RENDER_INDEX_BUFFER_QUAD
	PushLayer(new IndexBufferQuadLayer());
#endif // RENDER_INDEX_BUFFER_QUAD

#if RENDER_CAMERA_TRANSFORMATION_QUAD
	PushLayer(new CameraTransformationQuadLayer());
#endif // RENDER_CAMERA_TRANSFORMATION_QUAD
}

void SandboxApplication::OnDestroy()
{}
