#include "Life/VillagerLines.h"
#include "Sim/Rng.h"

#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr int SP = static_cast<int>(Season::Spring);
		constexpr int SU = static_cast<int>(Season::Summer);
		constexpr int FA = static_cast<int>(Season::Fall);
		constexpr int WI = static_cast<int>(Season::Winter);
		constexpr int CLEAR = static_cast<int>(Weather::Clear);
		constexpr int RAIN = static_cast<int>(Weather::Rain);
		constexpr int SNOW = static_cast<int>(Weather::Snow);
		constexpr int A = LINE_ANY;

		constexpr VillagerLine k_Mira[] = {
			{ "Fair prices, fresh seeds, and a smile on the house.", A, A, 0, 10 },
			{ "If you cannot find something, ask. If I do not have it, I will pretend I meant to.", A, A, 0, 10 },
			{ "Oh, hello. Mind the step, the shop floor creaks.", A, A, 0, 3 },
			{ "Looking to buy, or just stretching your legs? Either is fine by me.", A, A, 0, 3 },
			{ "There you are! I saved the good twine for you.", A, A, 4, 7 },
			{ "You look like someone with a plan. What are you growing this week?", A, A, 4, 7 },
			{ "My favourite customer. Come in, sit, I will not let you leave without tea.", A, A, 8, 10 },
			{ "Every morning I hope it is you at the door. Is that silly? Do not answer that.", A, A, 8, 10 },
			{ "Everything smells of wet soil and new leaves. Parsnips are on the shelf, by the way.", SP, A, 0, 10 },
			{ "It is warm enough to nap standing up. I have been trying all afternoon.", SU, A, 0, 10 },
			{ "Pumpkins by the door and cider in the pot. Autumn is the shop's best season.", FA, A, 0, 10 },
			{ "Frost on the windows again. I draw little suns on them to feel better.", WI, A, 0, 10 },
			{ "Listen to that rain on the roof. Customers are scarce, but oh, the quiet.", A, RAIN, 0, 10 },
			{ "Rain days are for sorting buttons and drinking too much tea.", A, RAIN, 0, 10 },
			{ "Snow on the sign again. I keep a shovel by the counter and a blanket on my knees.", A, SNOW, 0, 10 },
			{ "Not a cloud today. I propped the door open so the whole square can wander in.", A, CLEAR, 0, 10 },
		};

		constexpr VillagerLine k_Bram[] = {
			{ "Good timber has a smell like nothing else. Breathe that in.", A, A, 0, 10 },
			{ "Measure twice, cut once, drink tea in between.", A, A, 0, 10 },
			{ "Hm. Planning to build? Bring wood and stone and we will talk.", A, A, 0, 3 },
			{ "Do not lean on the fresh paint. Thank you.", A, A, 0, 3 },
			{ "Ah, the farmer. Your fences are holding up, I hear. Good, good.", A, A, 4, 7 },
			{ "Have a seat on the bench, I made it myself. It only wobbles a little.", A, A, 4, 7 },
			{ "I carved you a little something, but it came out looking like a duck. Take the thought instead.", A, A, 8, 10 },
			{ "You are the only one who notices the dovetails. That means a lot to an old carpenter.", A, A, 8, 10 },
			{ "Spring thaw means loose boards everywhere. Busy season for me, and I do not mind a bit.", SP, A, 0, 10 },
			{ "Shade is the finest building material in summer. I keep a stack of it under the eaves.", SU, A, 0, 10 },
			{ "The wind is picking up. Time to check every roof in the valley before the cold sets in.", FA, A, 0, 10 },
			{ "Sawdust and a warm stove. Winter is when I do my best thinking.", WI, A, 0, 10 },
			{ "Rain on a good roof is music. Rain on a bad roof is a to-do list.", A, RAIN, 0, 10 },
			{ "Too wet to saw outside, so I am sanding every spare plank I own.", A, RAIN, 0, 10 },
			{ "Snow piles on the beams, and every beam is holding. Proud of that.", A, SNOW, 0, 10 },
			{ "Clear skies, dry planks, and nowhere I would rather be.", A, CLEAR, 0, 10 },
		};

		constexpr VillagerLine k_Odette[] = {
			{ "The valley runs on small kindnesses. I only keep the list.", A, A, 0, 10 },
			{ "Always a pleasure. Do mind the board, there are new requests.", A, A, 0, 10 },
			{ "Ah, our newest neighbour. I hope the valley is treating you kindly.", A, A, 0, 3 },
			{ "If you need anything at all, my door is the one with the geraniums.", A, A, 0, 3 },
			{ "You have been a real help around here. People notice, you know.", A, A, 4, 7 },
			{ "Come by for biscuits sometime. I bake far too many for one mayor.", A, A, 4, 7 },
			{ "I would trust you with the keys to the whole valley. Not that there are many keys.", A, A, 8, 10 },
			{ "When I think of the valley's good luck, I think of you.", A, A, 8, 10 },
			{ "Spring planning is my favourite meeting, mostly because I get to hold it outside.", SP, A, 0, 10 },
			{ "We are planning the summer lanterns. Do say you will come.", SU, A, 0, 10 },
			{ "Harvest time! Every table in the valley will be heaped by the end of the month.", FA, A, 0, 10 },
			{ "Mind the ice by the well, and keep your scarf on. I have seen the valley in worse.", WI, A, 0, 10 },
			{ "A soggy day, but the crops will be grateful. Every cloud, as they say.", A, RAIN, 0, 10 },
			{ "I have an umbrella somewhere. I also have no idea where. Typical.", A, RAIN, 0, 10 },
			{ "Look at the square under all that snow. Quiet enough to hear your own thoughts.", A, SNOW, 0, 10 },
			{ "A perfect day for a stroll. I might even finish a sentence before someone interrupts.", A, CLEAR, 0, 10 },
		};

		constexpr VillagerLine k_Pip[] = {
			{ "Hi hi hi! Did you know a chicken can run faster than you think?", A, A, 0, 10 },
			{ "I am not supposed to say this, but I think puddles are the best part of the valley.", A, A, 0, 10 },
			{ "Oh! You are the farmer. Do you have any sweets? No? That is okay.", A, A, 0, 3 },
			{ "Mum says not to bother the grown-ups, but you are interesting.", A, A, 0, 3 },
			{ "I drew a picture of your farm! It has a very big sun and about forty cows.", A, A, 4, 7 },
			{ "Come on, I will show you where the frogs sit. Be quiet though.", A, A, 4, 7 },
			{ "You are my best friend in the whole valley. Do not tell Bram, he thinks it is him.", A, A, 8, 10 },
			{ "When I grow up I am going to be a farmer just like you. Or a pirate. Probably both.", A, A, 8, 10 },
			{ "Flowers everywhere! I have been counting them. I got to a hundred and then lost track.", SP, A, 0, 10 },
			{ "It is so hot the pond is practically calling my name. Splash!", SU, A, 0, 10 },
			{ "I found the best leaf pile! It is taller than me. You should jump in it.", FA, A, 0, 10 },
			{ "Snowball fight? No? Okay, I will build a snow-farmer then. You can watch.", WI, A, 0, 10 },
			{ "Rain! Rain! I have my boots on and everything. Want to splash?", A, RAIN, 0, 10 },
			{ "The worms come out when it rains. I say hello to every one.", A, RAIN, 0, 10 },
			{ "It is snowing it is snowing it is snowing!", A, SNOW, 0, 10 },
			{ "The sky is so blue today it looks like someone washed it.", A, CLEAR, 0, 10 },
		};

		struct Reactions
		{
			const char* Loved;
			const char* Fine;
			const char* Limit;
		};

		constexpr Reactions k_Reactions[VILLAGER_COUNT] = {
			{ "Oh, this is lovely! You really do know my tastes. Thank you!",
			  "How thoughtful. I will find a place for it, I promise.",
			  "You have already spoiled me today. Save something for tomorrow!" },
			{ "Well now. That is just what I wanted. Good choice, farmer.",
			  "Hm. Kind of you. I will put it with the nails, where it will be safe.",
			  "One gift a day is plenty for an old carpenter. Come back tomorrow." },
			{ "For me? How perfect. The whole house will smell of it tonight.",
			  "How kind of you. Every gift is a small celebration.",
			  "Another one? You are too generous. Tomorrow, dear, tomorrow." },
			{ "WOW! It is my favourite! Thank you thank you thank you!",
			  "Ooh, a present! I will keep it in my pocket forever. Or until lunch.",
			  "You already gave me something today! Can I have another tomorrow?" },
		};
	}

	std::span<const VillagerLine> GetVillagerLines(VillagerId villager)
	{
		switch (villager)
		{
		case VillagerId::Shopkeeper: return k_Mira;
		case VillagerId::Carpenter: return k_Bram;
		case VillagerId::Mayor: return k_Odette;
		default: return k_Pip;
		}
	}

	bool LineMatches(const VillagerLine& line, const LineContext& context)
	{
		return (line.Season == LINE_ANY || line.Season == static_cast<int>(context.Season))
			&& (line.Weather == LINE_ANY || line.Weather == static_cast<int>(context.Weather))
			&& context.Hearts >= line.MinHearts && context.Hearts <= line.MaxHearts;
	}

	int CountMatchingLines(VillagerId villager, const LineContext& context)
	{
		int count = 0;
		for (const VillagerLine& line : GetVillagerLines(villager))
			count += LineMatches(line, context) ? 1 : 0;
		return count;
	}

	const char* PickVillagerLine(VillagerId villager, const LineContext& context)
	{
		std::vector<const VillagerLine*> matches;
		for (const VillagerLine& line : GetVillagerLines(villager))
		{
			if (LineMatches(line, context))
				matches.push_back(&line);
		}
		if (matches.empty())
			return GetVillagerLines(villager)[0].Text;

		const uint64_t key = HashMix(HashMix(static_cast<uint64_t>(context.Day), static_cast<uint64_t>(villager) + 1), static_cast<uint64_t>(context.Talk));
		return matches[static_cast<size_t>(key % matches.size())]->Text;
	}

	const char* GiftReaction(VillagerId villager, GiftOutcome outcome)
	{
		const Reactions& reactions = k_Reactions[static_cast<size_t>(villager)];
		switch (outcome)
		{
		case GiftOutcome::Loved: return reactions.Loved;
		case GiftOutcome::Fine: return reactions.Fine;
		case GiftOutcome::DailyLimit: return reactions.Limit;
		default: return "That is your tool, farmer. You will want to keep hold of it.";
		}
	}

}
