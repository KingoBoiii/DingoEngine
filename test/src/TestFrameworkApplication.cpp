#include "TestFrameworkApplication.h"

#include "TestLayer.h"

namespace Dingo
{

	void TestFrameworkApplication::OnInitialize()
	{
		PushLayer(new TestLayer());
	}

}
