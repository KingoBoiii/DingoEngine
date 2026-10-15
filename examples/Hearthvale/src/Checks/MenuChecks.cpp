#include "Checks/BoardFill.h"
#include "Checks/Checks.h"
#include "Checks/LayoutAsserts.h"
#include "Checks/MenuChecks.h"
#include "Defs/CreditDefs.h"
#include "GameTuning.h"
#include "Placing/BuildController.h"
#include "Placing/Interactions.h"
#include "Ui/MenuPages.h"
#include "Ui/OptionsModel.h"
#include "Ui/ScreenHost.h"
#include "ValleyDirector.h"
#include "World/ValleyLayout.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <span>

namespace Dingo
{

	namespace
	{
		using namespace LayoutAsserts;
		using namespace MenuMetrics;

		class TestSlots final : public SlotProvider
		{
		public:
			bool Used[SLOT_COUNT] = {};
			bool Refuse = false;
			int Started = -1;
			int Loaded = -1;
			int Saved = -1;
			int Calls = 0;

			SlotInfo Describe(int slot) const override { return { Used[slot], Used[slot] ? "Day 3, 120 coins" : "" }; }

			bool StartNew(int slot, std::string& message) override
			{
				++Calls;
				message = Refuse ? "Refused" : "New game";
				if (Refuse)
					return false;
				Started = slot;
				Used[slot] = true;
				return true;
			}

			bool Load(int slot, std::string& message) override
			{
				++Calls;
				message = Refuse ? "Refused" : "Loaded";
				if (Refuse)
					return false;
				Loaded = slot;
				return true;
			}

			bool Save(int slot, std::string& message) override
			{
				++Calls;
				message = Refuse ? "Refused" : "Saved";
				if (Refuse)
					return false;
				Saved = slot;
				Used[slot] = true;
				return true;
			}
		};

		struct MenuRig
		{
			ValleyDirector Director;
			GameState State;
			BuildController Controller;
			ScreenHost Host;
			TestSlots Slots;
			int Quits = 0;
			std::vector<OptionsState> Changes;
			std::vector<OptionsState> Closed;

			explicit MenuRig(int startDay = 0)
				: Director(MakeConfig(startDay)), Controller(Director.GetGrid(), State.Pack, [](const TileRect&) {}), Host(Director, State.Pack, Controller)
			{
				State.Pack.GiveStarterLoadout();
				Controller.SetCalendar(&Director.GetCalendar());
				MenuServices services;
				services.State = &State;
				services.Slots = &Slots;
				services.OnQuit = [this] { ++Quits; };
				services.OnOptionsChanged = [this](const OptionsState& state) { Changes.push_back(state); };
				services.OnOptionsClosed = [this](const OptionsState& state) { Closed.push_back(state); };
				Host.SetServices(std::move(services));
			}

			static DirectorConfig MakeConfig(int startDay)
			{
				DirectorConfig config;
				config.StartDay = startDay;
				return config;
			}

			int Day() const { return Director.GetCalendar().GetAbsoluteDay(); }
			void Idle() { PlayerIntent intent; Host.Update(intent, false); }
			void Back() { Idle(); Host.Back(); }
			void Slide(int arg, float value) { Idle(); Host.SetSlider(arg, value); }

			// A player picking the widget `label`: nothing happens when the page has none or it is disabled.
			bool Press(const std::string& label)
			{
				Idle();
				const PageSpec page = Host.BuildPage();
				int row = -1;
				const int column = FindWidgetCell(page, label, &row);
				if (column < 0)
					return false;
				const PageCell& cell = page.Rows[static_cast<size_t>(row)].Cells[static_cast<size_t>(column)];
				if (!cell.Enabled || cell.Kind != CellKind::Button)
					return false;
				Host.Act(cell.Action, cell.Arg, cell.Label);
				return true;
			}

			bool HasWidget(const std::string& label)
			{
				return FindWidgetCell(Host.BuildPage(), label) >= 0;
			}

			bool WidgetEnabled(const std::string& label)
			{
				const PageSpec page = Host.BuildPage();
				int row = -1;
				const int column = FindWidgetCell(page, label, &row);
				return column >= 0 && page.Rows[static_cast<size_t>(row)].Cells[static_cast<size_t>(column)].Enabled;
			}
		};

		Inventory FullPack()
		{
			Inventory pack;
			for (int item = 0; item < ITEM_COUNT; ++item)
				pack.Add(static_cast<ItemId>(item), 99);
			pack.AddCoins(999999);
			return pack;
		}

		struct Audit
		{
			int Pages = 0;
			std::function<void(const std::string&, const PageSpec&)> Visit;
			std::vector<std::string> Inside, Overlap, Unique, WidgetText, PlainText, Resolutions, Reach;

			void Add(const std::string& name, const PageSpec& page)
			{
				++Pages;
				if (Visit)
					Visit(name, page);
				const ScreenLayout layout = LayoutPage(page);
				if (!EverythingInside(layout) || layout.Panel.W <= 0.0f || layout.Panel.H <= 0.0f)
					Inside.push_back(name);
				if (!NoWidgetsOverlap(layout))
					Overlap.push_back(name);
				if (!LabelsUnique(layout))
					Unique.push_back(name);
				if (!WidgetsFit(layout))
					WidgetText.push_back(name);
				if (!TextsFit(layout))
					PlainText.push_back(name);
				if (!FitsEveryResolution(layout))
					Resolutions.push_back(name);
				if (layout.FocusableCount() <= 0 || layout.FocusIndexOf(page.DefaultFocus) < 0 || layout.ReachableFrom(page.DefaultFocus) != layout.FocusableCount())
					Reach.push_back(name);
			}

			static std::string Names(const std::vector<std::string>& list)
			{
				std::string text;
				for (size_t i = 0; i < list.size() && i < 3; ++i)
					text += (text.empty() ? "" : "; ") + list[i];
				return text;
			}

			static void Report(CheckReport& report, const std::vector<std::string>& failed, const char* name)
			{
				report.Check(failed.empty(), failed.empty() ? std::string(name) : std::format("{} (first: {})", name, Names(failed)));
			}
		};

		std::vector<std::string> LongStatuses()
		{
			std::vector<std::string> statuses;
			for (int result = 0; result <= static_cast<int>(TradeResult::AlreadyKnown); ++result)
				statuses.push_back(ToString(static_cast<TradeResult>(result)));
			for (int item = 0; item < ITEM_COUNT; ++item)
			{
				const char* name = GetItemDef(static_cast<ItemId>(item)).Name;
				statuses.push_back(std::format("Sold 25 {} for 99999 coins", name));
				statuses.push_back(std::format("Bought 25 {}", name));
				statuses.push_back(std::format("Put 25 {} in the bin", name));
				statuses.push_back(std::format("Took 25 {} back", name));
				statuses.push_back(std::format("Gave Shopkeeper one {}", name));
			}
			for (const BuildDef& def : GetBuildDefs())
				statuses.push_back(std::format("Bought the {} plan", def.Name));
			statuses.push_back("Slot 3 is in use. Press it again to overwrite.");
			statuses.push_back("Loading arrives with the save system");
			statuses.push_back("Saving arrives with the save system");
			statuses.push_back("Saving is not available yet");
			return statuses;
		}

		std::string LongestStatus(const std::vector<std::string>& statuses)
		{
			std::string longest;
			for (const std::string& status : statuses)
			{
				if (status.size() > longest.size())
					longest = status;
			}
			return longest;
		}

		void AuditShop(Audit& audit, const std::string& label, Merchant seller, const Inventory& pack, const std::string& status)
		{
			const ShippingBin bin;
			const CommunityBoard board;
			for (int tab = 0; tab < 2; ++tab)
			{
				for (int quantity = 0; quantity < QUANTITY_COUNT; ++quantity)
				{
					MenuView view;
					view.Pack = &pack;
					view.Bin = &bin;
					view.Board = &board;
					view.Seller = seller;
					view.Tab = tab;
					view.Quantity = quantity;
					view.Status = status;
					const PageSpec first = BuildMenuPage(ScreenId::Shop, view);
					for (int page = 0; page < first.PageCount; ++page)
					{
						view.Page = page;
						audit.Add(std::format("shop {} tab {} x{} page {}", label, tab, QUANTITIES[quantity], page), BuildMenuPage(ScreenId::Shop, view));
					}
				}
			}
		}

		void AuditBin(Audit& audit, const std::string& label, const Inventory& pack, const ShippingBin& bin, const std::string& status)
		{
			const CommunityBoard board;
			for (int tab = 0; tab < 2; ++tab)
			{
				MenuView view;
				view.Pack = &pack;
				view.Bin = &bin;
				view.Board = &board;
				view.Tab = tab;
				view.Status = status;
				const PageSpec first = BuildMenuPage(ScreenId::ShippingBin, view);
				for (int page = 0; page < first.PageCount; ++page)
				{
					view.Page = page;
					audit.Add(std::format("bin {} tab {} page {}", label, tab, page), BuildMenuPage(ScreenId::ShippingBin, view));
				}
			}
		}

		void AuditBoard(Audit& audit, const std::string& label, const CommunityBoard& board, const Inventory& pack, int day, const std::string& status)
		{
			const ShippingBin bin;
			for (int index = 0; index < CommunityBoard::BoardCount; ++index)
			{
				MenuView view;
				view.Pack = &pack;
				view.Bin = &bin;
				view.Board = &board;
				view.Day = day;
				view.BoardIndex = index;
				view.Status = status;
				audit.Add(std::format("board {} index {} day {}", label, index, day), BuildMenuPage(ScreenId::Board, view));
			}
		}

		Audit AuditEveryPage(const std::function<void(const std::string&, const PageSpec&)>& visit = {})
		{
			Audit audit;
			audit.Visit = visit;
			const std::vector<std::string> statuses = LongStatuses();
			const std::string longest = LongestStatus(statuses);

			Inventory empty;
			Inventory starter;
			starter.GiveStarterLoadout();
			Inventory full = FullPack();
			const Inventory* packs[] = { &empty, &starter, &full };
			const char* packNames[] = { "empty", "starter", "full" };

			{
				const CommunityBoard board;
				const ShippingBin bin;
				MenuView view;
				view.Pack = &starter;
				view.Bin = &bin;
				view.Board = &board;
				for (int occupied = 0; occupied < 2; ++occupied)
				{
					view.Slots.assign(SlotProvider::SLOT_COUNT, SlotInfo{ occupied == 1, occupied == 1 ? "Day 118 of the third year, 123456 coins, the long summary of a game" : "" });
					audit.Add(std::format("title occupied {}", occupied), BuildMenuPage(ScreenId::Title, view));
					for (const SlotMode mode : { SlotMode::NewGame, SlotMode::Load, SlotMode::Save })
					{
						view.SlotsMode = mode;
						for (int confirm = -1; confirm < SlotProvider::SLOT_COUNT; ++confirm)
						{
							view.SlotConfirm = confirm;
							for (const std::string& status : { std::string(), longest })
							{
								view.Status = status;
								audit.Add(std::format("slots mode {} occupied {} confirm {}", static_cast<int>(mode), occupied, confirm), BuildMenuPage(ScreenId::Slots, view));
							}
						}
					}
					view.SlotConfirm = -1;
					view.Status.clear();
				}
				audit.Add("pause", BuildMenuPage(ScreenId::Pause, view));

				for (int variant = 0; variant < 4; ++variant)
				{
					OptionsState& options = view.Options;
					options.Master = variant == 1 ? 0.0f : 1.0f;
					options.Music = variant == 1 ? 0.0f : 0.7f;
					options.CameraSpeed = variant == 2 ? OPTION_CAMERA_MAX : (variant == 1 ? OPTION_CAMERA_MIN : 1.0f);
					options.VSync = variant != 3;
					options.Fullscreen = variant >= 2;
					options.Exclusive = variant == 3;
					options.HoldToPlace = variant % 2 == 1;
					audit.Add(std::format("options variant {}", variant), BuildMenuPage(ScreenId::Options, view));
				}

				const PageSpec firstCredits = BuildMenuPage(ScreenId::Credits, view);
				for (int page = 0; page < firstCredits.PageCount; ++page)
				{
					view.Page = page;
					audit.Add(std::format("credits page {}", page), BuildMenuPage(ScreenId::Credits, view));
				}
			}

			for (size_t p = 0; p < 3; ++p)
			{
				for (const Merchant seller : { Merchant::Shopkeeper, Merchant::Carpenter })
					AuditShop(audit, std::format("{} {}", p, static_cast<int>(seller)), seller, *packs[p], p == 1 ? longest : std::string());
				ShippingBin bin;
				Inventory shipper = FullPack();
				for (int item = 0; item < ITEM_COUNT; ++item)
					bin.Ship(shipper, static_cast<ItemId>(item), item % 3 == 0 ? 0 : 40);
				AuditBin(audit, std::format("{} empty bin", packNames[p]), *packs[p], ShippingBin(), longest);
				AuditBin(audit, std::format("{} loaded bin", packNames[p]), *packs[p], bin, std::string());
			}
			for (const std::string& status : statuses)
			{
				AuditShop(audit, "status", Merchant::Shopkeeper, starter, status);
				const ShippingBin bin;
				AuditBin(audit, "status", starter, bin, status);
			}

			TileGrid grid(VALLEY_SIZE, VALLEY_SIZE);
			CommunityBoard board;
			const int lastDay = DAYS_PER_YEAR - 1;
			for (const int day : { 0, 100, lastDay })
			{
				for (size_t p = 0; p < 3; ++p)
					AuditBoard(audit, std::format("fresh {}", packNames[p]), board, *packs[p], day, std::string());
				AuditBoard(audit, "status", board, full, day, longest);
			}
			for (int b = 0; b < CommunityBoard::BoardCount; ++b)
			{
				BoardFill::Fulfil(board, grid, b, lastDay);
				AuditBoard(audit, std::format("filled {}", b), board, full, lastDay, std::string());
			}
			board.Settle(grid, lastDay);
			AuditBoard(audit, "pending", board, full, lastDay, std::string());
			board.AcknowledgeFestival(grid);
			AuditBoard(audit, "tier 1", board, full, lastDay, longest);
			for (int b = 0; b < CommunityBoard::BoardCount; ++b)
			{
				BoardFill::Fulfil(board, grid, b, lastDay);
				board.Settle(grid, lastDay);
			}
			board.AcknowledgeFestival(grid);
			AuditBoard(audit, "tier 2", board, full, lastDay, std::string());

			std::vector<DialogueScript> scripts;
			for (const VillagerDef& villager : GetVillagerDefs())
				scripts.push_back(PlaceholderVillagerLine(villager.Id));
			scripts.push_back(FestivalScript());
			DialogueScript huge;
			huge.Speaker = "Odette";
			for (int i = 0; i < 40; ++i)
				huge.Text += "A long and winding sentence that keeps going on and on. ";
			scripts.push_back(huge);
			DialogueScript word;
			word.Speaker = "Bram";
			for (int i = 0; i < 30; ++i)
				word.Text += "abcdefghij";
			scripts.push_back(word);
			DialogueScript blank;
			blank.Speaker = "Mira";
			scripts.push_back(blank);
			DialogueScript lines;
			lines.Speaker = "Pip";
			lines.Text = "one\ntwo\nthree\nfour\nfive\nsix\nseven\n\n\nten";
			scripts.push_back(lines);
			const CommunityBoard board0;
			const ShippingBin bin0;
			for (size_t s = 0; s < scripts.size(); ++s)
			{
				MenuView view;
				view.Pack = &starter;
				view.Bin = &bin0;
				view.Board = &board0;
				view.Dialogue = scripts[s];
				const PageSpec first = BuildMenuPage(ScreenId::Dialogue, view);
				for (int page = 0; page < first.PageCount; ++page)
				{
					view.DialoguePage = page;
					audit.Add(std::format("dialogue {} page {}", s, page), BuildMenuPage(ScreenId::Dialogue, view));
				}
			}
			return audit;
		}

		void CheckLayouts(CheckReport& report)
		{
			const Audit audit = AuditEveryPage();
			report.Check(audit.Pages > 300, std::format("Menus: {} page variants were laid out", audit.Pages));
			Audit::Report(report, audit.Inside, "Menus: every widget and text of every screen sits inside its panel");
			Audit::Report(report, audit.Overlap, "Menus: no two widgets of any screen overlap");
			Audit::Report(report, audit.Unique, "Menus: every widget on a screen has its own label");
			Audit::Report(report, audit.WidgetText, "Menus: every button's label fits inside the button");
			Audit::Report(report, audit.PlainText, "Menus: every text of every screen fits its box, with the longest statuses and the longest lines");
			Audit::Report(report, audit.Resolutions, "Menus: every screen's panel fits the design size at 1280 x 720, 3840 x 2160 and the other test resolutions");
			Audit::Report(report, audit.Reach, "Menus: every screen's widgets are all reachable by focus moves from where the focus starts");
		}

		void CheckWrap(CheckReport& report)
		{
			const UiMeasure measure = k_Estimate;
			const float size = 10.0f;
			const float width = 62.0f;

			const std::vector<std::string> two = WrapLines("aaaa bbbb cccc", size, width, measure);
			report.Check(two.size() == 2 && two[0] == "aaaa bbbb" && two[1] == "cccc", "Wrap: words fill a line and the next word starts the next");

			const std::string paragraph = "The valley is coming back to life, one board at a time, and every neighbour has something to ask of you before the festival.";
			bool fits = true;
			std::string joined;
			for (const std::string& line : WrapLines(paragraph, size, width, measure))
			{
				fits = fits && measure(line, size, 0.0f).Width <= width + 0.01f;
				joined += (joined.empty() ? "" : " ") + line;
			}
			report.Check(fits && joined == paragraph, "Wrap: no line is wider than the box and no word is lost");

			const std::string word = "abcdefghijklmnopqrstuvwxyz";
			const std::vector<std::string> broken = WrapLines(word, size, width, measure);
			std::string rebuilt;
			bool brokenFits = broken.size() > 1;
			for (const std::string& line : broken)
			{
				brokenFits = brokenFits && measure(line, size, 0.0f).Width <= width + 0.01f;
				rebuilt += line;
			}
			report.Check(brokenFits && rebuilt == word, "Wrap: a word wider than the box is broken between characters");

			const std::vector<std::string> blank = WrapLines("a\n\nb", size, width, measure);
			report.Check(blank.size() == 3 && blank[0] == "a" && blank[1].empty() && blank[2] == "b", "Wrap: a newline starts a line and a blank line stays");

			const std::vector<std::string> none = PaginateText("", size, width, 4, measure);
			report.Check(none.size() == 1 && none[0].empty(), "Wrap: an empty text is one empty page");

			const std::vector<std::string> pages = PaginateText("1\n2\n3\n4\n5\n6\n7\n8\n9\n10", size, width, 4, measure);
			bool pageSizes = pages.size() == 3;
			for (const std::string& page : pages)
				pageSizes = pageSizes && WrapLines(page, size, width, measure).size() <= 4;
			report.Check(pageSizes && pages[0] == "1\n2\n3\n4" && pages[2] == "9\n10", "Wrap: pages hold at most the line limit and keep the order");

			const std::vector<std::string> festival = DialoguePages(FestivalScript());
			bool festivalFits = festival.size() >= 2;
			for (const std::string& page : festival)
				festivalFits = festivalFits && TextFitsBox(page, DIALOGUE_SIZE, DIALOGUE_W, DIALOGUE_H, true, k_Estimate);
			report.Check(festivalFits, "Wrap: the festival speech is cut into pages that each fit the dialogue box");
		}

		void CheckOptionsModel(CheckReport& report)
		{
			OptionsState wild;
			wild.Master = 1.7f;
			wild.Music = -1.0f;
			wild.Sfx = 0.52f;
			wild.CameraSpeed = 9.0f;
			const OptionsState clean = ClampOptions(wild);
			report.Check(clean.Master == 1.0f && clean.Music == 0.0f && std::abs(clean.Sfx - 0.5f) < 0.001f && clean.CameraSpeed == OPTION_CAMERA_MAX,
				"Options: volumes clamp to 0..1 on the step and the camera speed to its range");
			report.Check(VolumeText(0.8f) == "80%" && VolumeText(0.0f) == "0%" && CameraSpeedText(1.5f) == "1.5x", "Options: a volume reads as a percentage and the camera speed as a multiple");

			bool roundTrips = true;
			const Settings base = OptionsToSettings(OptionsState{}, Settings{});
			for (int variant = 0; variant < 16; ++variant)
			{
				OptionsState state;
				state.Master = static_cast<float>(variant % 5) * 0.25f;
				state.Music = static_cast<float>((variant + 1) % 5) * 0.25f;
				state.Ambience = static_cast<float>((variant + 2) % 5) * 0.25f;
				state.Sfx = static_cast<float>((variant + 3) % 5) * 0.25f;
				state.Ui = static_cast<float>((variant + 4) % 5) * 0.25f;
				state.VSync = (variant & 1) != 0;
				state.Fullscreen = (variant & 2) != 0;
				state.Exclusive = (variant & 4) != 0;
				state.HoldToPlace = (variant & 8) != 0;
				state.CameraSpeed = OPTION_CAMERA_MIN + static_cast<float>(variant % 6) * 0.3f;
				const OptionsState expected = ClampOptions(state);
				const Settings stored = Settings::FromSaveData(OptionsToSettings(state, base).ToSaveData());
				roundTrips = roundTrips && OptionsFromSettings(stored) == expected;
			}
			report.Check(roundTrips, "Options: every option survives the engine Settings save data round trip, including the display kind kept while windowed");

			OptionsState fullscreen;
			fullscreen.Fullscreen = true;
			fullscreen.Exclusive = true;
			const Settings exclusive = OptionsToSettings(fullscreen, base);
			fullscreen.Exclusive = false;
			const Settings borderless = OptionsToSettings(fullscreen, base);
			OptionsState windowed;
			windowed.Fullscreen = false;
			const Settings plain = OptionsToSettings(windowed, base);
			report.Check(exclusive.Display.Mode == WindowMode::ExclusiveFullscreen && borderless.Display.Mode == WindowMode::BorderlessFullscreen && plain.Display.Mode == WindowMode::Windowed,
				"Options: fullscreen and its kind map to the three window modes");

			Settings other = base;
			other.Display.Width = 1111;
			other.Game["unrelated"] = 7;
			const Settings merged = OptionsToSettings(OptionsState{}, other);
			report.Check(merged.Display.Width == 1111 && merged.Game.Get("unrelated", 0) == 7, "Options: what the screen does not edit is kept from the base settings");
			report.Check(merged.Audio.BusVolumes.size() >= 4 && merged.Audio.BusVolumes.contains(k_BusMusic) && merged.Audio.BusVolumes.contains(k_BusAmbience)
				&& merged.Audio.BusVolumes.contains(k_BusSfx) && merged.Audio.BusVolumes.contains(k_BusUi), "Options: the four bus volumes are named for the engine to capture and apply");
		}

		bool FolderExists(const std::filesystem::path& path)
		{
			std::error_code error;
			return std::filesystem::is_directory(path, error);
		}

		void CheckCredits(CheckReport& report)
		{
			const std::span<const CreditEntry> credits = GetCredits();
			bool complete = !credits.empty();
			bool kenney = false;
			bool kaykit = false;
			bool inter = false;
			bool engine = false;
			for (const CreditEntry& entry : credits)
			{
				complete = complete && std::string(entry.Pack).size() > 0 && std::string(entry.Author).size() > 0 && std::string(entry.Licence).size() > 0 && std::string(entry.Url).size() > 0;
				kenney = kenney || std::string(entry.Author) == "Kenney";
				kaykit = kaykit || std::string(entry.Author) == "Kay Lousberg";
				inter = inter || std::string(entry.Pack).find("Inter") != std::string::npos;
				engine = engine || std::string(entry.Pack) == "DingoEngine";
			}
			report.Check(complete && kenney && kaykit && inter && engine, "Credits: the table names Kenney, KayKit, Inter and the engine and every row is filled in");

			bool unique = true;
			std::set<std::string> packs;
			for (const CreditEntry& entry : credits)
				unique = packs.insert(entry.Pack).second && unique;
			report.Check(unique, "Credits: no pack is listed twice");

			for (const char* root : { "assets", "examples/Hearthvale/assets" })
			{
				const std::filesystem::path kenneyDir = std::filesystem::path(root) / "kenney";
				if (!FolderExists(kenneyDir))
					continue;
				std::set<std::string> folders;
				for (const CreditEntry& entry : credits)
					folders.insert(entry.Folder);
				bool covered = true;
				for (const std::filesystem::directory_entry& pack : std::filesystem::directory_iterator(kenneyDir))
				{
					if (pack.is_directory())
						covered = folders.contains(FileSystem::PathToUtf8(pack.path().filename())) && covered;
				}
				bool listed = true;
				for (const CreditEntry& entry : credits)
				{
					if (std::string(entry.Author) == "Kenney")
						listed = FolderExists(kenneyDir / entry.Folder) && listed;
				}
				report.Check(covered && listed && folders.contains("kaykit") == FolderExists(std::filesystem::path(root) / "kaykit"),
					"Credits: every pack folder in the assets has a row and every Kenney row has its folder");
				break;
			}
		}

		void CheckTitleAndSlots(CheckReport& report)
		{
			{
				MenuRig rig;
				rig.Host.OpenTitle();
				report.Check(rig.Host.Top() == ScreenId::Title && rig.Director.IsTimePaused(), "Title: the title opens over a stopped clock");

				rig.Back();
				PlayerIntent pause;
				pause.Pause = true;
				rig.Host.Update(pause, false);
				report.Check(rig.Host.Top() == ScreenId::Title && rig.Host.GetStack().Depth() == 1, "Title: back and the pause input leave the title open");

				report.Check(!rig.WidgetEnabled("Continue##title") && !rig.Press("Continue##title") && rig.Host.Top() == ScreenId::Title && rig.Host.BuildPage().DefaultFocus == "New game##title",
					"Title: Continue is disabled and the focus starts on New game while no slot is used");

				report.Check(rig.Press("New game##title") && rig.Host.Top() == ScreenId::Slots && rig.Host.GetStack().Depth() == 2 && rig.Host.BuildPage().Title == "New game",
					"Title: New game opens the slots screen over the title");
				rig.Back();
				report.Check(rig.Host.Top() == ScreenId::Title && rig.Host.TakeFocusRequest() == "New game##title", "Title: closing the slots returns the focus to New game");

				rig.Press("New game##title");
				const bool picked = rig.Press("Slot 1##slot0");
				report.Check(picked && rig.Slots.Started == 0 && !rig.Host.IsOpen() && !rig.Director.IsTimePaused(), "Slots: picking an empty slot starts a game there, closes every menu and runs the clock");
			}
			{
				MenuRig rig;
				rig.Slots.Used[1] = true;
				rig.Host.OpenTitle();
				report.Check(rig.WidgetEnabled("Continue##title") && rig.Host.BuildPage().DefaultFocus == "Continue##title", "Title: Continue is enabled and focused once a slot is used");
				rig.Press("Continue##title");
				report.Check(rig.Host.Top() == ScreenId::Slots && rig.Host.BuildPage().Title == "Load game" && !rig.WidgetEnabled("Slot 1##slot0") && rig.WidgetEnabled("Slot 2##slot1"),
					"Slots: loading lists the used slots only");
				report.Check(!rig.Press("Slot 1##slot0") && rig.Slots.Calls == 0, "Slots: an empty slot cannot be loaded");
				report.Check(rig.Press("Slot 2##slot1") && rig.Slots.Loaded == 1 && !rig.Host.IsOpen(), "Slots: loading a used slot needs no confirmation");
			}
			{
				MenuRig rig;
				rig.Slots.Used[1] = true;
				rig.Host.OpenSlots(SlotMode::NewGame);
				rig.Press("Slot 2##slot1");
				const bool asked = rig.Slots.Calls == 0 && rig.Host.IsOpen() && rig.HasWidget("Overwrite?##slot1") && rig.Host.GetStatus().find("again") != std::string::npos;
				rig.Press("Slot 1##slot0");
				const bool cleared = rig.Slots.Started == 0 && !rig.Host.IsOpen();
				report.Check(asked && cleared, "Slots: a used slot asks before it is overwritten and another pick drops the question");
			}
			{
				MenuRig rig;
				rig.Slots.Used[2] = true;
				rig.Host.OpenSlots(SlotMode::Save);
				rig.Press("Slot 3##slot2");
				const bool asked = rig.Slots.Saved < 0 && rig.Host.IsOpen();
				rig.Press("Overwrite?##slot2");
				report.Check(asked && rig.Slots.Saved == 2 && !rig.Host.IsOpen(), "Slots: pressing the overwrite question again saves");
			}
			{
				MenuRig rig;
				rig.Slots.Refuse = true;
				rig.Host.OpenSlots(SlotMode::Save);
				rig.Press("Slot 1##slot0");
				report.Check(rig.Host.IsOpen() && rig.Host.GetStatus() == "Refused" && rig.Slots.Saved < 0, "Slots: a refused save keeps the screen open and says why");
			}
			{
				MenuRig rig;
				MenuServices none;
				rig.Host.SetServices(none);
				rig.Host.OpenSlots(SlotMode::NewGame);
				rig.Press("Slot 1##slot0");
				report.Check(rig.Host.IsOpen() && !rig.Host.GetStatus().empty() && !rig.Host.OpenBin() && !rig.Host.OpenBoard() && rig.Host.OpenShop(Merchant::Carpenter),
					"Slots: without a save provider the screen says so, and the bin and board need the game state");
			}
			{
				NullSlotProvider null;
				std::string message;
				report.Check(!null.Describe(0).Occupied && null.StartNew(0, message) && !null.Load(0, message) && !message.empty() && !null.Save(0, message) && !message.empty(),
					"Slots: the placeholder provider starts a new game and refuses to load or save");
			}
		}

		void CheckPause(CheckReport& report)
		{
			MenuRig rig;
			PlayerIntent pause;
			pause.Pause = true;
			rig.Host.Update(pause, false);
			report.Check(rig.Host.Top() == ScreenId::Pause && rig.Director.IsTimePaused() && !pause.Pause, "Pause: the pause input opens the menu over a stopped clock");

			PlayerIntent again;
			again.Pause = true;
			rig.Host.Update(again, false);
			report.Check(!rig.Host.IsOpen() && !rig.Director.IsTimePaused(), "Pause: the same input closes it and the clock runs again");

			PlayerIntent blocked;
			blocked.Pause = true;
			rig.Host.Update(blocked, true);
			report.Check(!rig.Host.IsOpen(), "Pause: it does not open during the sleep fade");

			rig.Idle();
			PlayerIntent open;
			open.Pause = true;
			rig.Host.Update(open, false);
			report.Check(rig.Press("Save game##pause") && rig.Host.Top() == ScreenId::Slots && rig.Host.BuildPage().Title == "Save game", "Pause: Save game opens the slots in save mode");
			rig.Back();
			report.Check(rig.Host.Top() == ScreenId::Pause && rig.Host.TakeFocusRequest() == "Save game##pause", "Pause: closing the slots returns the focus to Save game");
			rig.Press("Options##pause");
			rig.Back();
			report.Check(rig.Host.Top() == ScreenId::Pause && rig.Host.TakeFocusRequest() == "Options##pause", "Pause: closing the options returns the focus to Options");

			rig.Press("Quit game##pause");
			report.Check(rig.Quits == 1, "Pause: Quit game asks the layer to close the application");
			rig.Press("Resume##pause");
			report.Check(!rig.Host.IsOpen() && !rig.Director.IsTimePaused(), "Pause: Resume closes the menu");

			MenuRig frozen;
			frozen.Host.OpenTitle();
			frozen.Press("Quit##title");
			report.Check(frozen.Quits == 1, "Title: Quit asks the layer to close the application");
		}

		void CheckOptionsScreen(CheckReport& report)
		{
			MenuRig rig;
			rig.Host.OpenTitle();
			rig.Press("Options##title");
			report.Check(rig.Host.Top() == ScreenId::Options && rig.Host.BuildPage().DefaultFocus == "##opt_master", "Options: the screen opens with the master volume focused");

			rig.Slide(1, 0.42f);
			report.Check(rig.Changes.size() == 1 && std::abs(rig.Changes.back().Music - 0.4f) < 0.001f && std::abs(rig.Host.GetOptions().Music - 0.4f) < 0.001f,
				"Options: a slider reports its value snapped to the step");
			rig.Slide(1, 0.4f);
			report.Check(rig.Changes.size() == 1, "Options: moving a slider to the value it has changes nothing");

			rig.Slide(0, 0.0f);
			rig.Slide(2, 0.25f);
			rig.Slide(3, 0.5f);
			rig.Slide(4, 0.9f);
			rig.Slide(5, 1.55f);
			const OptionsState sliders = rig.Host.GetOptions();
			report.Check(sliders.Master == 0.0f && std::abs(sliders.Ambience - 0.25f) < 0.001f && std::abs(sliders.Sfx - 0.5f) < 0.001f && std::abs(sliders.Ui - 0.9f) < 0.001f
				&& std::abs(sliders.CameraSpeed - 1.5f) < 0.001f, "Options: each of the five volume sliders and the camera slider edits its own value");
			report.Check(rig.HasWidget("##opt_master") && rig.HasWidget("##opt_music") && rig.HasWidget("##opt_ambience") && rig.HasWidget("##opt_sfx") && rig.HasWidget("##opt_ui") && rig.HasWidget("##opt_camera"),
				"Options: the page has the five bus sliders and the camera slider");

			report.Check(rig.Press("On##opt_vsync") && !rig.Host.GetOptions().VSync && rig.HasWidget("Off##opt_vsync"), "Options: the VSync button toggles and relabels");
			report.Check(!rig.WidgetEnabled("Borderless##opt_kind") && !rig.Press("Borderless##opt_kind"), "Options: the display kind is disabled while the game is windowed");
			rig.Press("Off##opt_fullscreen");
			report.Check(rig.Host.GetOptions().Fullscreen && rig.WidgetEnabled("Borderless##opt_kind"), "Options: Fullscreen turns on and enables the display kind");
			rig.Press("Borderless##opt_kind");
			report.Check(rig.Host.GetOptions().Exclusive && rig.HasWidget("Exclusive##opt_kind"), "Options: the display kind switches to exclusive");
			rig.Press("Off##opt_hold");
			report.Check(rig.Host.GetOptions().HoldToPlace && rig.Changes.back().HoldToPlace, "Options: hold-to-place toggles and the change reaches the layer");

			report.Check(rig.Closed.empty(), "Options: nothing is saved while the screen is open");
			rig.Back();
			report.Check(rig.Closed.size() == 1 && rig.Closed.back() == rig.Host.GetOptions() && rig.Host.Top() == ScreenId::Title && rig.Host.TakeFocusRequest() == "Options##title",
				"Options: closing saves the final state once and returns the focus to Options");

			rig.Host.SetSlider(0, 0.1f);
			rig.Press("Credits##title");
			rig.Back();
			report.Check(rig.Closed.size() == 1 && rig.Host.GetOptions().Master == 0.0f, "Options: a slider event outside the options screen is ignored and other screens do not save");

			MenuRig closeAll;
			closeAll.Host.OpenTitle();
			closeAll.Press("Options##title");
			closeAll.Slide(0, 0.5f);
			closeAll.Host.CloseAll();
			report.Check(closeAll.Closed.size() == 1, "Options: closing every screen from the options still saves");

			OptionsState custom;
			custom.Master = 0.35f;
			custom.HoldToPlace = true;
			MenuRig loaded;
			loaded.Host.SetOptions(custom);
			report.Check(loaded.Host.GetOptions().Master == 0.35f && loaded.Host.GetOptions().HoldToPlace, "Options: the host starts from the options the layer loaded");
		}

		void CheckCreditsScreen(CheckReport& report)
		{
			MenuRig rig;
			rig.Host.OpenTitle();
			rig.Press("Credits##title");
			const PageSpec first = rig.Host.BuildPage();
			report.Check(rig.Host.Top() == ScreenId::Credits && first.PageCount >= 2 && first.PageIndex == 0 && first.DefaultFocus == "Next##credits", "Credits: the screen opens on page 1 with Next focused");
			rig.Press("Prev##credits");
			report.Check(rig.Host.BuildPage().PageIndex == first.PageCount - 1, "Credits: Prev from the first page wraps to the last");
			rig.Press("Next##credits");
			report.Check(rig.Host.BuildPage().PageIndex == 0, "Credits: Next from the last page wraps to the first");

			std::set<std::string> shown;
			for (int page = 0; page < first.PageCount; ++page)
			{
				for (const PageRow& row : rig.Host.BuildPage().Rows)
				{
					if (row.Cells.size() == 4 && row.Cells[0].Kind == CellKind::Text && row.Cells[0].Label != "Pack")
						shown.insert(row.Cells[0].Label);
				}
				rig.Press("Next##credits");
			}
			report.Check(shown.size() == GetCredits().size(), "Credits: paging shows every row of the table");
			rig.Back();
			report.Check(rig.Host.Top() == ScreenId::Title && rig.Host.TakeFocusRequest() == "Credits##title", "Credits: closing returns the focus to Credits");
		}

		void CheckShopScreen(CheckReport& report)
		{
			{
				MenuRig rig;
				Inventory& pack = rig.State.Pack;
				const std::vector<ShopOffer> stock = ShopStock(Merchant::Shopkeeper);
				const ShopOffer& offer = stock.front();
				const std::string label = std::format("{}##buy0", GetItemDef(offer.Item).Name);
				rig.Host.OpenShop(Merchant::Shopkeeper);
				report.Check(rig.Host.Top() == ScreenId::Shop && rig.Director.IsTimePaused() && rig.HasWidget("Buy##shoptab") && rig.HasWidget("Sell##shoptab"), "Shop: the shopkeeper's screen opens with Buy and Sell tabs over a stopped clock");

				const int coins = pack.GetCoins();
				const int held = pack.Count(offer.Item);
				report.Check(rig.Press(label) && pack.GetCoins() == coins - offer.Price && pack.Count(offer.Item) == held + 1 && rig.Host.GetStatus().find("Bought 1") == 0,
					"Shop: buying an item pays its price and says what was bought");

				rig.Press("Amount x1##shopqty");
				report.Check(rig.Host.GetQuantity() == 5 && rig.HasWidget("Amount x5##shopqty"), "Shop: the amount button cycles to 5");
				rig.Press(label);
				report.Check(pack.GetCoins() == coins - offer.Price * 6 && pack.Count(offer.Item) == held + 6, "Shop: buying at x5 takes five at once");
				rig.Press("Amount x5##shopqty");
				report.Check(rig.Host.GetQuantity() == 25 && rig.HasWidget("Amount x25##shopqty"), "Shop: the amount button cycles to 25");
				rig.Press("Amount x25##shopqty");
				report.Check(rig.Host.GetQuantity() == 1, "Shop: the amount button wraps back to 1");

				pack.SpendCoins(pack.GetCoins());
				const int broke = pack.Count(offer.Item);
				rig.Press(label);
				report.Check(pack.Count(offer.Item) == broke && rig.Host.GetStatus() == ToString(TradeResult::CannotAfford), "Shop: a purchase without the coins changes nothing and says so");

				pack.Add(ItemId::Wood, 10 - pack.Count(ItemId::Wood));
				const int sellPrice = ShopSellBackPrice(ItemId::Wood);
				rig.Press("Sell##shoptab");
				const std::string sellLabel = std::format("{}##sell{}", GetItemDef(ItemId::Wood).Name, static_cast<int>(ItemId::Wood));
				report.Check(rig.HasWidget(sellLabel) && rig.Host.BuildPage().Title.find("shop") != std::string::npos, "Shop: the Sell tab lists what the pack holds that the shop buys");
				rig.Press(sellLabel);
				report.Check(pack.Count(ItemId::Wood) == 9 && pack.GetCoins() == sellPrice, "Shop: selling pays the sell-back price at once");
				rig.Press("Amount x1##shopqty");
				rig.Press("Amount x5##shopqty");
				rig.Press(sellLabel);
				report.Check(pack.Count(ItemId::Wood) == 0 && pack.GetCoins() == sellPrice * 10, "Shop: selling more than held sells what there is");
				const std::string request = rig.Host.TakeFocusRequest();
				const PageSpec after = rig.Host.BuildPage();
				const ScreenLayout layout = LayoutPage(after);
				report.Check(!request.empty() && layout.FocusIndexOf(request) >= 0 && !rig.HasWidget(sellLabel), "Shop: when a sold row disappears the focus moves to a row that is still there");
				rig.Back();
				report.Check(!rig.Host.IsOpen(), "Shop: back closes it and the clock runs");
			}
			{
				MenuRig rig;
				rig.State.Pack.AddCoins(100000);
				const ShopOffer offer = ShopStock(Merchant::Carpenter).front();
				const std::string label = std::format("Plan: {}##buy0", GetBuildDef(offer.Build).Name);
				rig.Host.OpenShop(Merchant::Carpenter);
				const bool known = rig.State.Pack.KnowsPlan(offer.Build);
				const int coins = rig.State.Pack.GetCoins();
				report.Check(!rig.HasWidget("Sell##shoptab") && !known && rig.Press(label) && rig.State.Pack.KnowsPlan(offer.Build) && rig.State.Pack.GetCoins() == coins - offer.Price,
					"Shop: the carpenter sells plans, has no Sell tab, and a bought plan is learnt");
				report.Check(!rig.Press(label) && rig.State.Pack.GetCoins() == coins - offer.Price, "Shop: a plan that is owned cannot be bought again");
			}
			{
				MenuRig rig;
				rig.Host.OpenShop(Merchant::Shopkeeper);
				const PageSpec page = rig.Host.BuildPage();
				if (page.PageCount > 1)
				{
					rig.Press("Next##shop");
					const int second = rig.Host.BuildPage().PageIndex;
					rig.Press("Prev##shop");
					report.Check(second == 1 && rig.Host.BuildPage().PageIndex == 0, "Shop: a long list pages with Next and Prev");
				}
				else
					report.Check(!rig.WidgetEnabled("Next##shop"), "Shop: a short list has its page buttons disabled");
			}
		}

		void CheckBinScreen(CheckReport& report)
		{
			MenuRig rig;
			Inventory& pack = rig.State.Pack;
			ShippingBin& bin = rig.State.Bin;
			const ItemId crop = HarvestItem(CropId::Parsnip);
			pack.Add(crop, 30);
			const int coins = pack.GetCoins();

			report.Check(rig.Host.OpenBin() && rig.Host.Top() == ScreenId::ShippingBin && rig.Director.IsTimePaused(), "Bin: the shipping bin opens over a stopped clock");
			const std::string ship = std::format("{}##ship{}", GetItemDef(crop).Name, static_cast<int>(crop));
			const std::string take = std::format("{}##take{}", GetItemDef(crop).Name, static_cast<int>(crop));
			rig.Press(ship);
			report.Check(pack.Count(crop) == 29 && bin.Held(crop) == 1 && pack.GetCoins() == coins, "Bin: shipping moves the goods into the bin and pays nothing yet");
			rig.Press("Amount x1##binqty");
			rig.Press(ship);
			report.Check(pack.Count(crop) == 24 && bin.Held(crop) == 6, "Bin: shipping at x5 moves five");

			rig.Press("Take back##bintab");
			report.Check(rig.HasWidget(take) && !rig.HasWidget(ship), "Bin: the Take back tab lists what the bin holds");
			rig.Press(take);
			report.Check(pack.Count(crop) == 29 && bin.Held(crop) == 1, "Bin: taking back at x5 returns five");
			rig.Press("Amount x5##binqty");
			rig.Press("Amount x25##binqty");
			rig.Press(take);
			report.Check(pack.Count(crop) == 30 && bin.IsEmpty() && pack.GetCoins() == coins, "Bin: taking back more than the bin holds returns what is there");
			report.Check(rig.Host.BuildPage().DefaultFocus == "Take back##bintab" && rig.HasWidget("Take back##bintab"), "Bin: an empty bin's list focuses the tab");

			rig.Press("Ship##bintab");
			rig.Press("Amount x1##binqty");
			rig.Press("Amount x5##binqty");
			rig.Press(ship);
			rig.Back();
			report.Check(!rig.Host.IsOpen() && bin.Held(crop) == 25 && pack.Count(crop) == 5, "Bin: goods stay in the bin when the screen closes");
		}

		void CheckBoardScreen(CheckReport& report)
		{
			{
				MenuRig rig(DAYS_PER_YEAR - 1);
				CommunityBoard& board = rig.State.Board;
				int boardIndex = -1;
				int requestIndex = -1;
				for (int b = 0; b < CommunityBoard::BoardCount && boardIndex < 0; ++b)
				{
					for (int i = 0; i < BOARD_REQUESTS; ++i)
					{
						if (GetBoardDef(static_cast<Season>(b)).Requests[static_cast<size_t>(i)].Kind == RequestKind::Gift)
						{
							boardIndex = b;
							requestIndex = i;
							break;
						}
					}
				}
				const BoardRequest& request = GetBoardDef(static_cast<Season>(boardIndex)).Requests[static_cast<size_t>(requestIndex)];
				const std::string give = std::format("Give##board{}", requestIndex);

				report.Check(rig.Host.OpenBoard() && rig.Host.Top() == ScreenId::Board && rig.Director.IsTimePaused(), "Board: the community board opens over a stopped clock");
				for (int n = 0; n < CommunityBoard::BoardCount && rig.Host.BuildPage().PageIndex != boardIndex; ++n)
					rig.Press("Next board##board");
				report.Check(rig.Host.BuildPage().PageIndex == boardIndex && rig.HasWidget(give), "Board: Next board steps through the open boards to the one with a gift request");
				report.Check(!rig.WidgetEnabled(give) && !rig.Press(give), "Board: Give is disabled while the item is not held");

				rig.State.Pack.Add(request.Item, 2);
				const int coins = rig.State.Pack.GetCoins();
				report.Check(rig.WidgetEnabled(give), "Board: Give is enabled once the item is held");
				rig.Press(give);
				report.Check(board.GetRequest(boardIndex, requestIndex).Progress == 1 && rig.State.Pack.Count(request.Item) == 1 && rig.State.Pack.GetCoins() == coins && !board.GetRequest(boardIndex, requestIndex).Done,
					"Board: giving takes one item, counts it, and the reward waits for the day roll");

				const int target = board.TargetCount(boardIndex, requestIndex);
				rig.State.Pack.Add(request.Item, target);
				for (int n = 0; n < target + 2; ++n)
					rig.Press(give);
				report.Check(board.GetRequest(boardIndex, requestIndex).Progress == target && !rig.WidgetEnabled(give) && rig.State.Pack.Count(request.Item) == 2 + target - board.TargetCount(boardIndex, requestIndex),
					"Board: a full request takes no more gifts");

				int steps = 0;
				const int start = rig.Host.BuildPage().PageIndex;
				do
				{
					rig.Press("Next board##board");
					++steps;
				} while (rig.Host.BuildPage().PageIndex != start && steps < 10);
				report.Check(steps == 4, "Board: Next board visits all four open boards and wraps");
				rig.Press("Prev board##board");
				report.Check(rig.Host.BuildPage().PageIndex == (start + 3) % 4, "Board: Prev board goes the other way");
				rig.Back();
				report.Check(!rig.Host.IsOpen(), "Board: back closes it");
			}
			{
				MenuRig rig;
				rig.Host.OpenBoard();
				report.Check(rig.Host.BuildPage().PageIndex == 0 && !rig.WidgetEnabled("Next board##board") && !rig.WidgetEnabled("Prev board##board"), "Board: with one board open the board buttons are disabled");
			}
		}

		void CheckFestivalFlow(CheckReport& report)
		{
			MenuRig rig(DAYS_PER_YEAR - 1);
			CommunityBoard& board = rig.State.Board;
			TileGrid& grid = rig.Director.GetGrid();
			bool placed = true;
			for (int b = 0; b < CommunityBoard::BoardCount; ++b)
				placed = BoardFill::Fulfil(board, grid, b, rig.Day()) && placed;
			const BoardReport settled = board.Settle(grid, rig.Day());
			report.Check(placed && settled.FestivalTriggered && board.GetFestival() == FestivalPhase::Pending, "Festival: a finished set of boards leaves the festival pending");

			rig.Host.OpenBoard();
			report.Check(rig.HasWidget("Hear the mayor##board") && rig.Host.BuildPage().DefaultFocus == "Hear the mayor##board", "Festival: the board offers the mayor's speech and focuses it");
			rig.Press("Hear the mayor##board");
			const PageSpec speech = rig.Host.BuildPage();
			report.Check(rig.Host.Top() == ScreenId::Dialogue && rig.Host.GetStack().Depth() == 2 && speech.Title == GetVillagerDef(VillagerId::Mayor).Name && speech.PageCount >= 2,
				"Festival: the speech opens as a dialogue of several pages over the board");

			rig.Back();
			report.Check(rig.Host.Top() == ScreenId::Board && board.GetFestival() == FestivalPhase::Pending && rig.Host.TakeFocusRequest() == "Hear the mayor##board",
				"Festival: leaving the speech early does not acknowledge the festival");

			rig.Press("Hear the mayor##board");
			int guard = 0;
			while (rig.HasWidget("Next##dialogue") && guard++ < 20)
			{
				rig.Press("Next##dialogue");
				report.Check(board.GetFestival() == FestivalPhase::Pending, "Festival: turning a page does not acknowledge it");
				break;
			}
			guard = 0;
			while (rig.HasWidget("Next##dialogue") && guard++ < 20)
				rig.Press("Next##dialogue");
			report.Check(rig.HasWidget("Close##dialogue") && rig.Host.BuildPage().DefaultFocus == "Close##dialogue" && rig.Host.BuildPage().PageIndex == speech.PageCount - 1,
				"Festival: the last page offers Close and focuses it");
			rig.Press("Close##dialogue");
			report.Check(rig.Host.Top() == ScreenId::Board && board.GetFestival() == FestivalPhase::Held && board.GetTier() == 1 && board.DoneCount() == 0,
				"Festival: closing the last page acknowledges the festival and re-arms the boards");
			report.Check(!rig.HasWidget("Hear the mayor##board") && !rig.Host.GetToast().empty(), "Festival: the speech is gone from the board afterwards and a toast says the festival began");

			rig.Host.OpenDialogue(FestivalScript());
			for (int n = 0; n < 10 && rig.Host.Top() == ScreenId::Dialogue; ++n)
				rig.Press(rig.HasWidget("Next##dialogue") ? "Next##dialogue" : "Close##dialogue");
			report.Check(board.GetTier() == 1 && board.GetFestival() == FestivalPhase::Held, "Festival: the speech played again acknowledges nothing twice");
		}

		void CheckDialogue(CheckReport& report)
		{
			MenuRig rig;
			rig.Host.OpenVillager(VillagerId::Child);
			const PageSpec page = rig.Host.BuildPage();
			report.Check(rig.Host.Top() == ScreenId::Dialogue && page.Title == GetVillagerDef(VillagerId::Child).Name && rig.Director.IsTimePaused(), "Dialogue: a villager's placeholder line opens over a stopped clock");
			rig.Press(rig.HasWidget("Next##dialogue") ? "Next##dialogue" : "Close##dialogue");
			report.Check(!rig.Host.IsOpen() || rig.Host.BuildPage().PageIndex == 1, "Dialogue: the button turns the page or closes");
			rig.Host.CloseAll();

			rig.Host.SetVillagerLines([](VillagerId villager)
			{
				DialogueScript script;
				script.Speaker = "Custom";
				script.Text = villager == VillagerId::Mayor ? "Mayor line" : "Other line";
				return script;
			});
			rig.Idle();
			rig.Host.OpenVillager(VillagerId::Mayor);
			report.Check(rig.Host.BuildPage().Title == "Custom" && rig.Host.BuildPage().PageCount == 1 && rig.HasWidget("Close##dialogue"), "Dialogue: the villager hook supplies the line");
			rig.Press("Close##dialogue");
			report.Check(!rig.Host.IsOpen(), "Dialogue: Close ends it");

			DialogueScript longer;
			longer.Speaker = "Odette";
			for (int i = 0; i < 12; ++i)
				longer.Text += std::format("Sentence number {} of a much longer speech. ", i);
			rig.Idle();
			rig.Host.OpenDialogue(longer);
			const int pages = rig.Host.BuildPage().PageCount;
			int turned = 1;
			while (rig.HasWidget("Next##dialogue") && turned < 30)
			{
				rig.Press("Next##dialogue");
				++turned;
			}
			report.Check(pages >= 3 && turned == pages && rig.HasWidget("Close##dialogue"), "Dialogue: a long text is turned page by page to the last");
			PlayerIntent escape;
			escape.Pause = true;
			rig.Host.Update(escape, false);
			report.Check(!rig.Host.IsOpen(), "Dialogue: the pause input closes it from any page");
		}

		void CheckFocusReturns(CheckReport& report)
		{
			struct Case
			{
				const char* Name;
				std::function<void(MenuRig&)> Open;
				std::vector<std::string> Openers;
			};
			const Case cases[] = {
				{ "title", [](MenuRig& rig) { rig.Slots.Used[0] = true; rig.Host.OpenTitle(); }, { "Continue##title", "New game##title", "Options##title", "Credits##title" } },
				{ "pause", [](MenuRig& rig) { rig.Host.Open(ScreenId::Pause); }, { "Save game##pause", "Options##pause" } },
			};
			for (const Case& c : cases)
			{
				bool returns = true;
				for (const std::string& opener : c.Openers)
				{
					MenuRig rig;
					c.Open(rig);
					const ScreenLayout below = LayoutPage(rig.Host.BuildPage());
					const ScreenId parent = rig.Host.Top();
					const bool opened = rig.Press(opener);
					rig.Back();
					returns = returns && opened && below.FocusIndexOf(opener) >= 0 && rig.Host.Top() == parent && rig.Host.TakeFocusRequest() == opener;
				}
				report.Check(returns, std::format("Menus ({}): every screen it opens hands the focus back to the button that opened it", c.Name));
			}
		}

		void CheckInteractions(CheckReport& report)
		{
			TileGrid grid(VALLEY_SIZE, VALLEY_SIZE);
			bool doors = true;
			bool footprints = true;
			for (const LandmarkSpec& spec : k_Landmarks)
			{
				const std::optional<Landmark> door = LandmarkAt(grid, spec.DoorX, spec.DoorY);
				const std::optional<Landmark> body = LandmarkAt(grid, spec.Footprint.X, spec.Footprint.Y);
				doors = doors && door && *door == spec.Id;
				footprints = footprints && body && *body == spec.Id;
			}
			report.Check(doors && footprints && !LandmarkAt(grid, 0, 0) && !LandmarkAt(grid, -1, -1) && !LandmarkAt(grid, VALLEY_SIZE, VALLEY_SIZE),
				"Interact: every landmark is found from its door and its footprint and nothing else is a landmark");

			struct Case
			{
				Landmark Place;
				ScreenId Opens;
			};
			const Case cases[] = {
				{ Landmark::ShippingBin, ScreenId::ShippingBin }, { Landmark::Shop, ScreenId::Shop }, { Landmark::Carpenter, ScreenId::Shop },
				{ Landmark::NoticeBoard, ScreenId::Board }, { Landmark::MayorHouse, ScreenId::Dialogue }, { Landmark::Cottage, ScreenId::Dialogue } };
			bool all = true;
			for (const Case& c : cases)
			{
				MenuRig rig;
				rig.Controller.SetInteractHandler([&rig](Landmark landmark) { return rig.Host.OpenLandmark(landmark); });
				const LandmarkSpec& spec = GetLandmarkSpec(c.Place);
				AimInput aim;
				aim.Feet = glm::vec2(static_cast<float>(spec.DoorX) + 0.5f, static_cast<float>(spec.DoorY) + 2.5f);
				aim.HasMouse = true;
				aim.MouseGround = glm::vec2(static_cast<float>(spec.DoorX) + 0.5f, static_cast<float>(spec.DoorY) + 0.5f);
				PlayerIntent intent;
				intent.Interact = true;
				rig.Controller.Step(intent, aim);
				all = all && rig.Host.Top() == c.Opens && rig.Director.IsTimePaused();
			}
			report.Check(all, "Interact: pressing interact at the bin, shop, carpenter, board and the two houses opens their screens");

			{
				MenuRig rig;
				rig.Controller.SetInteractHandler([&rig](Landmark landmark) { return rig.Host.OpenLandmark(landmark); });
				rig.Controller.SetHoldToPlace(false);
				const LandmarkSpec& spec = GetLandmarkSpec(Landmark::Carpenter);
				AimInput aim;
				aim.Feet = glm::vec2(static_cast<float>(spec.DoorX) + 0.5f, static_cast<float>(spec.DoorY) + 2.5f);
				aim.HasMouse = true;
				aim.MouseGround = glm::vec2(static_cast<float>(spec.DoorX) + 0.5f, static_cast<float>(spec.DoorY) + 0.5f);
				PlayerIntent intent;
				intent.Interact = true;
				rig.Controller.Step(intent, aim);
				report.Check(rig.Host.Top() == ScreenId::Shop && rig.Host.BuildPage().Title.find("workshop") != std::string::npos, "Interact: the carpenter's door opens the workshop, not the shop");
			}
			{
				MenuRig rig;
				int slept = 0;
				rig.Controller.SetSleepHandler([&slept] { ++slept; });
				rig.Controller.SetInteractHandler([&rig](Landmark landmark) { return rig.Host.OpenLandmark(landmark); });
				const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
				AimInput aim;
				aim.Feet = glm::vec2(static_cast<float>(home.DoorX) + 0.5f, static_cast<float>(home.DoorY) + 2.5f);
				aim.HasMouse = true;
				aim.MouseGround = glm::vec2(static_cast<float>(home.DoorX) + 0.5f, static_cast<float>(home.DoorY) + 0.5f);
				PlayerIntent intent;
				intent.Interact = true;
				rig.Controller.Step(intent, aim);
				report.Check(slept == 1 && !rig.Host.IsOpen(), "Interact: the farmhouse door still goes to bed and opens no screen");
			}
			{
				MenuRig rig;
				const LandmarkSpec& spec = GetLandmarkSpec(Landmark::Shop);
				AimInput aim;
				aim.Feet = glm::vec2(static_cast<float>(spec.DoorX) + 0.5f, static_cast<float>(spec.DoorY) + 2.5f);
				aim.HasMouse = true;
				aim.MouseGround = glm::vec2(static_cast<float>(spec.DoorX) + 0.5f, static_cast<float>(spec.DoorY) + 0.5f);
				PlayerIntent intent;
				intent.Interact = true;
				rig.Controller.Step(intent, aim);
				report.Check(!rig.Host.IsOpen(), "Interact: without a handler the shop door does nothing");

				MenuRig plot;
				plot.Controller.SetInteractHandler([&plot](Landmark landmark) { return plot.Host.OpenLandmark(landmark); });
				AimInput field;
				field.Feet = glm::vec2(30.5f, 46.5f);
				field.HasMouse = true;
				field.MouseGround = glm::vec2(30.5f, 44.5f);
				plot.Controller.Step(intent, field);
				report.Check(!plot.Host.IsOpen(), "Interact: an ordinary tile opens nothing");
			}
			{
				MenuRig rig;
				rig.Host.OpenBin();
				PlayerIntent pulses;
				pulses.Use = true;
				pulses.Interact = true;
				pulses.ToggleBuild = true;
				rig.Host.Update(pulses, false);
				report.Check(!pulses.Use && !pulses.Interact && rig.Host.Top() == ScreenId::ShippingBin, "Menus: a menu swallows the use and interact pulses so nothing reaches the farmer");
			}
			{
				MenuRig rig;
				rig.Host.OpenShop(Merchant::Shopkeeper);
				PlayerIntent inventory;
				inventory.ToggleInventory = true;
				rig.Host.Update(inventory, false);
				report.Check(rig.Host.Top() == ScreenId::Shop && !inventory.ToggleInventory, "Menus: the inventory key does not close a shop");
			}
		}

		void CheckHoldToPlace(CheckReport& report)
		{
			auto fenceAt = [](MenuRig& rig, int x) { return rig.Director.GetGrid().At(x, 44).Occupant != k_NoOccupant; };
			auto step = [](MenuRig& rig, int x, bool use, bool held)
			{
				AimInput aim;
				aim.Feet = glm::vec2(30.5f, 46.5f);
				aim.HasMouse = true;
				aim.MouseGround = glm::vec2(static_cast<float>(x) + 0.5f, 44.5f);
				PlayerIntent intent;
				intent.Use = use;
				intent.UseHeld = held;
				rig.Controller.Step(intent, aim);
			};
			auto ready = [](MenuRig& rig, bool hold)
			{
				rig.State.Pack.Add(ItemId::Wood, 99);
				rig.State.Pack.AddCoins(5000);
				rig.Controller.SetMode(BuildMode::Build);
				rig.Controller.SelectBuildable(BuildId::WoodFence);
				rig.Controller.SetHoldToPlace(hold);
			};

			{
				MenuRig rig;
				ready(rig, true);
				step(rig, 28, true, true);
				step(rig, 29, false, true);
				step(rig, 29, false, true);
				step(rig, 30, false, true);
				report.Check(fenceAt(rig, 28) && fenceAt(rig, 29) && fenceAt(rig, 30), "Hold to place: holding use after a press places on each new tile the aim reaches");
				step(rig, 31, false, false);
				step(rig, 32, false, true);
				report.Check(!fenceAt(rig, 31) && !fenceAt(rig, 32), "Hold to place: releasing the button stops it, and a hold with no press does not start it");
			}
			{
				MenuRig rig;
				ready(rig, false);
				step(rig, 28, true, true);
				step(rig, 29, false, true);
				report.Check(fenceAt(rig, 28) && !fenceAt(rig, 29), "Hold to place: with the option off one press places one");
			}
			{
				MenuRig rig;
				ready(rig, true);
				rig.Controller.SetMode(BuildMode::Tools);
				step(rig, 28, true, true);
				rig.Controller.SetMode(BuildMode::Build);
				step(rig, 29, false, true);
				report.Check(!fenceAt(rig, 29) && rig.Controller.GetHoldToPlace(), "Hold to place: a tool swing never arms it");
			}
		}

		void CheckMenus(CheckReport& report)
		{
			CheckLayouts(report);
			CheckWrap(report);
			CheckOptionsModel(report);
			CheckCredits(report);
			CheckTitleAndSlots(report);
			CheckPause(report);
			CheckOptionsScreen(report);
			CheckCreditsScreen(report);
			CheckShopScreen(report);
			CheckBinScreen(report);
			CheckBoardScreen(report);
			CheckFestivalFlow(report);
			CheckDialogue(report);
			CheckFocusReturns(report);
			CheckInteractions(report);
			CheckHoldToPlace(report);
		}
	}

	void ForEachMenuPage(const std::function<void(const std::string&, const PageSpec&)>& visit)
	{
		(void)AuditEveryPage(visit);
	}

	void RunMenuChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Menus");
		CheckMenus(report);
	}

}
