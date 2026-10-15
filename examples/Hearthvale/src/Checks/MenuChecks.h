#pragma once
#include "Ui/PageLayout.h"

#include <functional>
#include <string>

namespace Dingo
{

	// Every page variant the menu suite lays out, so the windowed run can measure each with the real font.
	void ForEachMenuPage(const std::function<void(const std::string&, const PageSpec&)>& visit);

}
