#include "Ui/ScreenStack.h"

#include <algorithm>

namespace Dingo
{

	const char* ToString(ScreenId id)
	{
		switch (id)
		{
		case ScreenId::None: return "None";
		case ScreenId::Inventory: return "Inventory";
		case ScreenId::BuildPalette: return "BuildPalette";
		}
		return "?";
	}

	bool ScreenStack::Open(ScreenId id, std::string openerLabel)
	{
		if (id == ScreenId::None || Contains(id) || Depth() >= MAX_DEPTH)
			return false;
		m_Entries.push_back({ id, std::move(openerLabel) });
		m_Returned.clear();
		return true;
	}

	bool ScreenStack::Close()
	{
		if (m_Entries.empty())
			return false;
		m_Returned = std::move(m_Entries.back().Opener);
		m_Entries.pop_back();
		return true;
	}

	bool ScreenStack::Replace(ScreenId id)
	{
		if (m_Entries.empty() || id == ScreenId::None || Contains(id))
			return false;
		m_Entries.back().Id = id;
		return true;
	}

	void ScreenStack::CloseAll()
	{
		m_Entries.clear();
		m_Returned.clear();
	}

	bool ScreenStack::Contains(ScreenId id) const
	{
		return std::any_of(m_Entries.begin(), m_Entries.end(), [id](const Entry& entry) { return entry.Id == id; });
	}

	std::string ScreenStack::TakeReturnFocus()
	{
		std::string label = std::move(m_Returned);
		m_Returned.clear();
		return label;
	}

}
