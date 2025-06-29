#include "SandboxApplication.h"

#include "Layers/StaticTriangleLayer.h"

#define RENDER_STATIC_TRIANGLE 0

void SandboxApplication::OnInitialize()
{
#if RENDER_STATIC_TRIANGLE
	PushLayer(new StaticTriangleLayer());
#endif // RENDER_STATIC_TRIANGLE
}

void SandboxApplication::OnDestroy()
{}
