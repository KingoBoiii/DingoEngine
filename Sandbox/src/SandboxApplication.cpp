#include "SandboxApplication.h"

#include "Layers/StaticTriangleLayer.h"
#include "Layers/VertexBufferTriangleLayer.h"
#include "Layers/IndexBufferQuadLayer.h"
#include "Layers/CameraTransformationQuadLayer.h"

void SandboxApplication::OnInitialize()
{
	//PushLayer(new StaticTriangleLayer());
	//PushLayer(new VertexBufferTriangleLayer());
	//PushLayer(new IndexBufferQuadLayer());
	PushLayer(new CameraTransformationQuadLayer());
}

void SandboxApplication::OnDestroy()
{}
