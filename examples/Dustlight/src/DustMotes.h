#pragma once

#include <DingoEngine.h>

namespace Dingo
{

	class DustMotes
	{
	public:
		~DustMotes();

		void Build(Scene& scene);
		void Release();

		Entity GetEmitter() const { return m_Emitter; }

	private:
		ParticleEffect* m_Effect = nullptr;
		Entity m_Emitter;
	};

}
