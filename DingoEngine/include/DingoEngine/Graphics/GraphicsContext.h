#pragma once
#include "GraphicsParams.h"
#include "DingoEngine/Common.h"

#include <string>

struct GLFWwindow;

namespace Dingo
{

	class GraphicsContext
	{
	public:
		static GraphicsContext* Create(const GraphicsParams& params);

	public:
		GraphicsContext(const GraphicsParams& params);
		virtual ~GraphicsContext();

	public:
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;

		virtual void RunGarbageCollection() const = 0;

		const GraphicsParams& GetParams() { return m_Params; }
		const GraphicsParams& GetParams() const { return m_Params; }
		static GraphicsContext& Get() { return *s_Instance; }

		template<typename T>
		T& As() { return static_cast<T&>(*this); }

	protected:
		GraphicsParams m_Params;

	private:
		inline static GraphicsContext* s_Instance = nullptr;
	};

}
