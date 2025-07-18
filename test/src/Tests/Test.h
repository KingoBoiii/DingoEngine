#pragma once

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

		virtual void ImGuiRender() {}
	};

}
