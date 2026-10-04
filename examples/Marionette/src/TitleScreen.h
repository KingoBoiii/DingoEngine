#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class TitleControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		Font* m_Font = nullptr;
	};

	class EndControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		Font* m_Font = nullptr;
	};

}
