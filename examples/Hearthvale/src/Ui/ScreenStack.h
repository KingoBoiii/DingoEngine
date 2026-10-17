#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Dingo
{

	enum class ScreenId : uint8_t
	{
		None, Inventory, BuildPalette,
		Title, Slots, Pause, Options, Credits, Shop, ShippingBin, Board, Dialogue
	};

	const char* ToString(ScreenId id);

	// The open screens, topmost last. Each entry remembers the label of the widget that opened it, so closing it hands
	// the focus back to that widget in the screen underneath. Plain data.
	class ScreenStack
	{
	public:
		static constexpr int MAX_DEPTH = 4;

		// False when the id is None, already open, or the stack is full.
		bool Open(ScreenId id, std::string openerLabel = {});
		// Pops the top screen and remembers its opener for TakeReturnFocus; false when nothing is open.
		bool Close();
		// Swaps the top screen for `id`, keeping what it was opened from; false when nothing is open or `id` is open.
		bool Replace(ScreenId id);
		void CloseAll();

		bool IsEmpty() const { return m_Entries.empty(); }
		int Depth() const { return static_cast<int>(m_Entries.size()); }
		ScreenId Top() const { return m_Entries.empty() ? ScreenId::None : m_Entries.back().Id; }
		bool Contains(ScreenId id) const;

		// The opener of the screen closed last, once: the widget to focus in the screen that is now on top.
		std::string TakeReturnFocus();

	private:
		struct Entry
		{
			ScreenId Id = ScreenId::None;
			std::string Opener;
		};

		std::vector<Entry> m_Entries;
		std::string m_Returned;
	};

}
