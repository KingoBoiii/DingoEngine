#pragma once
#include "FighterIntent.h"

namespace Dingo
{

	class PlayerBrain : public Brain
	{
	public:
		FighterIntent Think(float deltaTime, const Fighter& self, const Fighter& opponent) override;
	};

}
