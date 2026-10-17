#include "Ui/UiLayout.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr float k_EstimateAdvance = 0.62f;
		constexpr float k_EstimateLineStep = 1.25f;
		constexpr float k_Epsilon = 0.01f;

		glm::vec3 Hue(float hue, float saturation, float value)
		{
			const float h = (hue - std::floor(hue)) * 6.0f;
			const float c = value * saturation;
			const float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
			const float m = value - c;
			glm::vec3 rgb(0.0f);
			if (h < 1.0f) rgb = { c, x, 0.0f };
			else if (h < 2.0f) rgb = { x, c, 0.0f };
			else if (h < 3.0f) rgb = { 0.0f, c, x };
			else if (h < 4.0f) rgb = { 0.0f, x, c };
			else if (h < 5.0f) rgb = { x, 0.0f, c };
			else rgb = { c, 0.0f, x };
			return rgb + glm::vec3(m);
		}
	}

	bool UiRect::Inside(const UiRect& outer) const
	{
		return X >= outer.X - k_Epsilon && Y >= outer.Y - k_Epsilon && Right() <= outer.Right() + k_Epsilon && Bottom() <= outer.Bottom() + k_Epsilon;
	}

	bool UiRect::Overlaps(const UiRect& other) const
	{
		return X < other.Right() - k_Epsilon && other.X < Right() - k_Epsilon && Y < other.Bottom() - k_Epsilon && other.Y < Bottom() - k_Epsilon;
	}

	float UiScaleFactor(float screenW, float screenH)
	{
		const float widthLog = std::log2(screenW / 1920.0f);
		const float heightLog = std::log2(screenH / 1080.0f);
		return std::exp2((widthLog + heightLog) * 0.5f);
	}

	glm::vec2 UiDesignSize(float screenW, float screenH)
	{
		const float scale = UiScaleFactor(screenW, screenH);
		return { screenW / scale, screenH / scale };
	}

	UiText EstimateText(const std::string& text, float size, float maxWidth)
	{
		if (text.empty())
			return {};
		const float advance = size * k_EstimateAdvance;
		UiText result;
		result.Lines = 1;
		float line = 0.0f;
		float word = 0.0f;
		float widest = 0.0f;
		bool lineHasWord = false;

		const auto flush = [&]
		{
			if (word > 0.0f)
			{
				const float joined = lineHasWord ? line + advance + word : word;
				if (maxWidth > 0.0f && lineHasWord && joined > maxWidth)
				{
					widest = (std::max)(widest, line);
					++result.Lines;
					line = word;
				}
				else
					line = joined;
				lineHasWord = true;
				word = 0.0f;
			}
		};

		for (const char ch : text)
		{
			if (ch == '\n')
			{
				flush();
				widest = (std::max)(widest, line);
				++result.Lines;
				line = 0.0f;
				lineHasWord = false;
			}
			else if (ch == ' ')
				flush();
			else
				word += advance;
		}
		flush();
		widest = (std::max)(widest, line);
		result.Width = widest;
		result.Height = size + (static_cast<float>(result.Lines) - 1.0f) * size * k_EstimateLineStep;
		return result;
	}

	bool TextFitsBox(const std::string& text, float size, float boxW, float boxH, bool wrap, const UiMeasure& measure)
	{
		const UiText metrics = measure(text, size, wrap ? boxW : 0.0f);
		return metrics.Width <= boxW + k_Epsilon && metrics.Height <= boxH + k_Epsilon;
	}

	std::vector<std::string> WrapLines(const std::string& text, float size, float maxWidth, const UiMeasure& measure)
	{
		std::vector<std::string> lines;
		const auto fits = [&](const std::string& candidate) { return maxWidth <= 0.0f || measure(candidate, size, 0.0f).Width <= maxWidth + k_Epsilon; };

		size_t paragraphStart = 0;
		while (paragraphStart <= text.size())
		{
			size_t paragraphEnd = text.find('\n', paragraphStart);
			if (paragraphEnd == std::string::npos)
				paragraphEnd = text.size();
			const std::string paragraph = text.substr(paragraphStart, paragraphEnd - paragraphStart);
			paragraphStart = paragraphEnd + 1;

			std::string line;
			size_t cursor = 0;
			bool any = false;
			while (cursor < paragraph.size())
			{
				while (cursor < paragraph.size() && paragraph[cursor] == ' ')
					++cursor;
				if (cursor >= paragraph.size())
					break;
				size_t wordEnd = paragraph.find(' ', cursor);
				if (wordEnd == std::string::npos)
					wordEnd = paragraph.size();
				std::string word = paragraph.substr(cursor, wordEnd - cursor);
				cursor = wordEnd;
				any = true;

				const std::string joined = line.empty() ? word : line + " " + word;
				if (fits(joined))
				{
					line = joined;
					continue;
				}
				if (!line.empty())
				{
					lines.push_back(line);
					line.clear();
				}
				while (!fits(word) && word.size() > 1)
				{
					size_t cut = word.size() - 1;
					while (cut > 1 && (static_cast<unsigned char>(word[cut]) & 0xC0) == 0x80)
						--cut;
					while (cut > 1 && !fits(word.substr(0, cut)))
					{
						--cut;
						while (cut > 1 && (static_cast<unsigned char>(word[cut]) & 0xC0) == 0x80)
							--cut;
					}
					lines.push_back(word.substr(0, cut));
					word = word.substr(cut);
				}
				line = word;
			}
			if (!line.empty() || !any)
				lines.push_back(line);
			if (paragraphEnd == text.size())
				break;
		}
		return lines;
	}

	std::vector<std::string> PaginateText(const std::string& text, float size, float maxWidth, int maxLines, const UiMeasure& measure)
	{
		const std::vector<std::string> lines = WrapLines(text, size, maxWidth, measure);
		const size_t perPage = static_cast<size_t>((std::max)(1, maxLines));
		std::vector<std::string> pages;
		for (size_t first = 0; first < lines.size(); first += perPage)
		{
			std::string page;
			for (size_t i = first; i < (std::min)(lines.size(), first + perPage); ++i)
				page += (i == first ? "" : "\n") + lines[i];
			pages.push_back(page);
		}
		if (pages.empty())
			pages.emplace_back();
		return pages;
	}

	std::string SplitName(const std::string& name, int maxChars)
	{
		if (static_cast<int>(name.size()) <= maxChars)
			return name;
		const int middle = static_cast<int>(name.size()) / 2;
		int best = -1;
		for (int i = 0; i < static_cast<int>(name.size()); ++i)
		{
			if (name[static_cast<size_t>(i)] == ' ' && (best < 0 || std::abs(i - middle) < std::abs(best - middle)))
				best = i;
		}
		if (best < 0)
			return name;
		std::string split = name;
		split[static_cast<size_t>(best)] = '\n';
		return split;
	}

	std::string CostInline(const BuildCost& cost)
	{
		std::string text;
		const auto add = [&text](int count, const char* name)
		{
			if (count > 0)
				text += std::format("{}{} {}", text.empty() ? "" : ", ", count, name);
		};
		add(cost.Coins, "coins");
		add(cost.Wood, "wood");
		add(cost.Stone, "stone");
		add(cost.Fibre, "fibre");
		return text.empty() ? "free" : text;
	}

	std::string CostLines(const BuildCost& cost)
	{
		std::string materials;
		const auto add = [&materials](int count, const char* name)
		{
			if (count > 0)
				materials += std::format("{}{} {}", materials.empty() ? "" : ", ", count, name);
		};
		add(cost.Wood, "wood");
		add(cost.Stone, "stone");
		add(cost.Fibre, "fibre");
		if (cost.Coins <= 0)
			return materials.empty() ? "free" : materials;
		const std::string coins = std::format("{} coins", cost.Coins);
		return materials.empty() ? coins : coins + "\n" + materials;
	}

	glm::vec4 ItemSwatch(ItemId item)
	{
		if (item >= ItemId::Count)
			return glm::vec4(0.2f, 0.18f, 0.16f, 1.0f);
		const ItemDef& def = GetItemDef(item);
		switch (def.Kind)
		{
		case ItemKind::Tool:
			return glm::vec4(0.55f, 0.6f, 0.66f, 1.0f);
		case ItemKind::Resource:
			if (item == ItemId::Wood) return glm::vec4(0.6f, 0.42f, 0.24f, 1.0f);
			if (item == ItemId::Stone) return glm::vec4(0.52f, 0.52f, 0.55f, 1.0f);
			return glm::vec4(0.74f, 0.68f, 0.42f, 1.0f);
		case ItemKind::Product:
			if (item == ItemId::Egg) return glm::vec4(0.93f, 0.88f, 0.74f, 1.0f);
			if (item == ItemId::Milk) return glm::vec4(0.82f, 0.88f, 0.94f, 1.0f);
			return glm::vec4(0.9f, 0.62f, 0.12f, 1.0f);
		case ItemKind::Seed:
			return glm::vec4(Hue(static_cast<float>(static_cast<int>(def.Crop)) / static_cast<float>(CROP_COUNT), 0.45f, 0.6f), 1.0f);
		case ItemKind::Harvest:
			return glm::vec4(Hue(static_cast<float>(static_cast<int>(def.Crop)) / static_cast<float>(CROP_COUNT), 0.75f, 0.95f), 1.0f);
		}
		return glm::vec4(1.0f);
	}

	bool AllInside(const std::vector<UiRect>& rects, const UiRect& outer)
	{
		return std::all_of(rects.begin(), rects.end(), [&outer](const UiRect& rect) { return rect.Inside(outer); });
	}

}
