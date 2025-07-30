#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class Test
	{
	public:
		virtual ~Test() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Update(float deltaTime) = 0;
		virtual void Cleanup() = 0;
		virtual void Resize(uint32_t width, uint32_t height) {}

		virtual void ImGuiRender() {}

		virtual Texture* GetResult() { return nullptr; }
	};

}
