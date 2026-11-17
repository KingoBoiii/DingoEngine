#include <doctest/doctest.h>

#include <DingoEngine/Scene/Entity.h>
#include <DingoEngine/Scene/Scene.h>
#include <DingoEngine/Scene/ScriptableEntity.h>

#include <memory>
#include <string>
#include <vector>

using namespace Dingo;

namespace
{
	struct Inventory;
}

namespace Dingo
{
	template<>
	struct ComponentTraits<Inventory>
	{
		static constexpr bool Copyable = false;
	};
}

namespace
{

	struct Listener { int Rate = 0; };
	struct Queue { std::vector<int> Items; };
	struct Owned { std::unique_ptr<int> Value; };
	struct Inventory { std::vector<std::unique_ptr<int>> Items; };
	struct OwnedBy { UUID Owner; };

	class Spawner : public ScriptableEntity
	{
	public:
		void OnDestroy() override
		{
			const UUID self = GetEntity().GetUUID();
			GetScene().Each<OwnedBy>([self](Entity minion, OwnedBy& owned)
			{
				if (owned.Owner == self)
					minion.Destroy();
			});
		}
	};

	std::vector<std::string> VisitNames(Scene& scene)
	{
		std::vector<std::string> names;
		scene.Each<Listener, Queue>([&](Entity entity, Listener&, Queue&) { names.push_back(entity.GetName()); });
		return names;
	}

}

TEST_CASE("Scene: game components are added, read, removed and die with their entity")
{
	Scene scene("Custom");
	Entity a = scene.CreateEntity("A");

	CHECK_FALSE(a.HasComponent<Listener>());
	Listener& listener = a.AddComponent<Listener>(Listener{ 3 });
	CHECK(listener.Rate == 3);
	CHECK(a.HasComponent<Listener>());
	CHECK(a.GetComponent<Listener>().Rate == 3);

	a.AddComponent(Listener{ 7 });
	CHECK(a.GetComponent<Listener>().Rate == 7);
	CHECK(scene.GetComponentCount<Listener>() == 1);

	a.RemoveComponent<Listener>();
	CHECK_FALSE(a.HasComponent<Listener>());
	a.RemoveComponent<Listener>();

	a.AddComponent<Queue>().Items.push_back(1);
	a.AddComponent<Owned>(Owned{ std::make_unique<int>(5) });
	CHECK(*a.GetComponent<Owned>().Value == 5);
	CHECK(a.HasComponent<TransformComponent>());

	a.Destroy();
	CHECK(scene.GetComponentCount<Queue>() == 0);
	CHECK(scene.GetComponentCount<Owned>() == 0);

	Entity b = scene.CreateEntity("B");
	CHECK_FALSE(b.HasComponent<Queue>());
	CHECK_FALSE(Entity().HasComponent<Queue>());
}

TEST_CASE("Scene: a view visits only the entities holding every type, in add order")
{
	Scene scene("View");
	for (int i = 0; i < 6; ++i)
	{
		Entity e = scene.CreateEntity("E" + std::to_string(i));
		e.AddComponent<Listener>(Listener{ i });
		if (i % 2 == 0)
			e.AddComponent<Queue>();
	}

	CHECK(VisitNames(scene) == std::vector<std::string>{ "E0", "E2", "E4" });
	CHECK(scene.View<Queue, Listener>().SizeHint() == 3);

	int sum = 0;
	scene.Each<Listener>([&](Listener& listener) { sum += listener.Rate; });
	CHECK(sum == 15);

	Entity first;
	scene.Each<Queue>([&](Entity entity, Queue&) { if (!first) first = entity; });
	first.RemoveComponent<Queue>();
	CHECK(VisitNames(scene) == std::vector<std::string>{ "E2", "E4" });
	std::vector<std::string> byQueue;
	scene.Each<Queue, Listener>([&](Entity entity, Queue&, Listener&) { byQueue.push_back(entity.GetName()); });
	CHECK(byQueue == std::vector<std::string>{ "E4", "E2" });

	scene.SortComponents<Listener>([](const Listener& a, const Listener& b) { return a.Rate > b.Rate; });
	CHECK(VisitNames(scene) == std::vector<std::string>{ "E4", "E2" });
	std::vector<int> rates;
	scene.Each<Listener>([&](Listener& listener) { rates.push_back(listener.Rate); });
	CHECK(rates == std::vector<int>{ 5, 4, 3, 2, 1, 0 });
}

TEST_CASE("Scene: the same calls give the same order in two scenes")
{
	auto build = [](Scene& scene)
	{
		std::vector<Entity> entities;
		for (int i = 0; i < 50; ++i)
		{
			Entity e = scene.CreateEntity("N" + std::to_string(i));
			e.AddComponent<Listener>(Listener{ (i * 37) % 11 });
			e.AddComponent<Queue>();
			entities.push_back(e);
		}
		for (int i = 0; i < 50; i += 7)
			entities[i].Destroy();
		for (int i = 3; i < 50; i += 9)
			entities[i].RemoveComponent<Queue>();
		scene.CreateEntity("Late").AddComponent<Listener>().Rate = 4;
		scene.SortComponents<Listener>([](const Listener& a, const Listener& b) { return a.Rate < b.Rate; });
	};

	Scene first("First");
	Scene second("Second");
	build(first);
	build(second);
	const std::vector<std::string> order = VisitNames(first);
	CHECK(order.size() == 50 - 8 - 5);
	CHECK(order == VisitNames(second));
}

TEST_CASE("Scene: changes inside Each land safely")
{
	Scene scene("Mutate");
	std::vector<Entity> entities;
	for (int i = 0; i < 5; ++i)
	{
		Entity e = scene.CreateEntity("M" + std::to_string(i));
		e.AddComponent<Listener>(Listener{ i });
		entities.push_back(e);
	}

	std::vector<int> visited;
	scene.Each<Listener>([&](Entity entity, Listener& listener)
	{
		visited.push_back(listener.Rate);
		if (listener.Rate == 0)
		{
			entities[1].RemoveComponent<Listener>();
			CHECK_FALSE(entities[1].HasComponent<Listener>());
			entities[2].Destroy();
			CHECK(entities[2].IsValid());
			scene.CreateEntity("Spawned").AddComponent<Listener>(Listener{ 99 });
			entity.RemoveComponent<Listener>();
			CHECK(listener.Rate == 0);
		}
	});

	CHECK(visited == std::vector<int>{ 0, 2, 3, 4 });
	CHECK_FALSE(entities[2].IsValid());
	CHECK(scene.GetComponentCount<Listener>() == 3);

	visited.clear();
	scene.Each<Listener>([&](Listener& listener) { visited.push_back(listener.Rate); });
	CHECK(visited == std::vector<int>{ 4, 99, 3 });
}

TEST_CASE("Scene: duplicate copies game components, Clear drops them")
{
	Scene scene("Copy");
	Entity source = scene.CreateEntity("Source");
	source.AddComponent<Listener>(Listener{ 8 });
	source.AddComponent<Owned>(Owned{ std::make_unique<int>(1) });

	Entity clone = scene.DuplicateEntity(source);
	REQUIRE(clone.HasComponent<Listener>());
	CHECK(clone.GetComponent<Listener>().Rate == 8);
	CHECK_FALSE(clone.HasComponent<Owned>());

	scene.Clear();
	CHECK(scene.GetComponentCount<Listener>() == 0);
	CHECK(VisitNames(scene).empty());
}

TEST_CASE("Scene: move-only game types, with ComponentTraits for a container of them")
{
	Scene scene("MoveOnly");
	Entity e = scene.CreateEntity("Bag");
	e.AddComponent<Inventory>().Items.push_back(std::make_unique<int>(3));
	e.AddComponent<Owned>();
	CHECK(*e.GetComponent<Inventory>().Items[0] == 3);

	Entity clone = scene.DuplicateEntity(e);
	CHECK_FALSE(clone.HasComponent<Inventory>());
	CHECK_FALSE(clone.HasComponent<Owned>());
}

TEST_CASE("Scene: a script's OnDestroy may destroy entities from an Each while a flush runs")
{
	Scene scene("Flush");
	std::vector<Entity> spawners;
	std::vector<Entity> minions;
	for (int s = 0; s < 3; ++s)
	{
		Entity spawner = scene.CreateEntity("Spawner");
		spawner.AddComponent<Listener>();
		spawner.AddScript<Spawner>();
		spawners.push_back(spawner);
		for (int m = 0; m < 40; ++m)
		{
			Entity minion = scene.CreateEntity("Minion");
			minion.AddComponent<OwnedBy>(OwnedBy{ spawner.GetUUID() });
			minions.push_back(minion);
		}
	}
	Entity bystander = scene.CreateEntity("Bystander");
	bystander.AddComponent<OwnedBy>(OwnedBy{ UUID() });

	scene.Each<Listener>([](Entity spawner, Listener&) { spawner.Destroy(); });

	for (Entity spawner : spawners)
		CHECK_FALSE(spawner.IsValid());
	bool allGone = true;
	for (Entity minion : minions)
		allGone = allGone && !minion.IsValid();
	CHECK(allGone);
	CHECK(bystander.IsValid());
	CHECK(scene.GetComponentCount<OwnedBy>() == 1);
}
