#include "SandboxApplication.h"

#include "Layers/StaticTriangleLayer.h"
#include "Layers/VertexBufferTriangleLayer.h"
#include "Layers/IndexBufferQuadLayer.h"
#include "Layers/CameraTransformationQuadLayer.h"
#include "Layers/TexturedQuadLayer.h"

void SandboxApplication::OnInitialize()
{
	//PushLayer(new StaticTriangleLayer());
	//PushLayer(new VertexBufferTriangleLayer());
	//PushLayer(new IndexBufferQuadLayer());
	//PushLayer(new CameraTransformationQuadLayer());
	PushLayer(new TexturedQuadLayer());
}

void SandboxApplication::OnDestroy()
{}
