// Tests for the entity-component system: entity handles, component pools and
// the registry's queries. No GPU needed: components are plain data.

#include "scene/Registry.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

using namespace Engine;

namespace {

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};
struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};
struct Tag {
    std::string label;
};

} // namespace

TEST_SUITE("ecs") {
    TEST_CASE("new entities are alive and distinct") {
        Registry r;
        const Entity a = r.create();
        const Entity b = r.create();
        CHECK(a != b);
        CHECK(r.alive(a));
        CHECK(r.alive(b));
        CHECK(r.aliveCount() == 2);
        CHECK_FALSE(r.alive(NullEntity));
        CHECK(NullEntity.isNull());
        CHECK_FALSE(a.isNull());
    }

    TEST_CASE("a destroyed entity's handle stays dead even after its slot is reused") {
        Registry r;
        const Entity old = r.create();
        r.add<Position>(old, 1.0f, 2.0f);
        r.destroy(old);
        CHECK_FALSE(r.alive(old));

        const Entity reused = r.create();
        CHECK(reused.index == old.index);           // same slot...
        CHECK(reused.generation != old.generation); // ...new generation
        CHECK(r.alive(reused));
        CHECK_FALSE(r.alive(old));
        // The old handle must not see the new entity's data.
        r.add<Position>(reused, 5.0f, 6.0f);
        CHECK_FALSE(r.has<Position>(old));
        CHECK(r.tryGet<Position>(old) == nullptr);
    }

    TEST_CASE("destroying twice, or destroying the null entity, is harmless") {
        Registry r;
        const Entity e = r.create();
        r.destroy(e);
        r.destroy(e);
        r.destroy(NullEntity);
        CHECK(r.aliveCount() == 0);
    }

    TEST_CASE("add, get, has and remove a component") {
        Registry r;
        const Entity e = r.create();
        CHECK_FALSE(r.has<Position>(e));

        Position& p = r.add<Position>(e, 3.0f, 4.0f);
        CHECK(p.x == 3.0f);
        CHECK(r.has<Position>(e));
        r.get<Position>(e).y = 9.0f;
        CHECK(r.get<Position>(e).y == 9.0f);

        r.remove<Position>(e);
        CHECK_FALSE(r.has<Position>(e));
        CHECK(r.alive(e)); // removing a component doesn't destroy the entity
        r.remove<Position>(e); // removing what isn't there does nothing
    }

    TEST_CASE("components of different types are independent") {
        Registry r;
        const Entity e = r.create();
        r.add<Position>(e);
        r.add<Tag>(e, "player");
        CHECK(r.has<Position>(e));
        CHECK_FALSE(r.has<Velocity>(e));
        CHECK(r.get<Tag>(e).label == "player");
        CHECK(componentTypeId<Position>() != componentTypeId<Tag>());
        CHECK(componentTypeId<Position>() == componentTypeId<Position>());
    }

    TEST_CASE("destroying an entity removes all its components") {
        Registry r;
        const Entity a = r.create();
        const Entity b = r.create();
        r.add<Position>(a);
        r.add<Tag>(a, "a");
        r.add<Position>(b);
        r.destroy(a);
        CHECK(r.count<Position>() == 1);
        CHECK(r.count<Tag>() == 0);
        CHECK(r.has<Position>(b));
    }

    TEST_CASE("removing from the middle keeps the other components intact (swap and pop)") {
        Registry r;
        std::vector<Entity> entities;
        for (int i = 0; i < 5; ++i) {
            entities.push_back(r.create());
            r.add<Position>(entities.back(), static_cast<float>(i), 0.0f);
        }
        r.remove<Position>(entities[1]);
        r.remove<Position>(entities[3]);
        CHECK(r.count<Position>() == 3);
        CHECK(r.get<Position>(entities[0]).x == 0.0f);
        CHECK(r.get<Position>(entities[2]).x == 2.0f);
        CHECK(r.get<Position>(entities[4]).x == 4.0f); // moved into a hole, still correct
    }

    TEST_CASE("each<A, B> visits exactly the entities that have both") {
        Registry r;
        const Entity moving = r.create();
        r.add<Position>(moving);
        r.add<Velocity>(moving, 1.0f, 2.0f);
        const Entity still = r.create();
        r.add<Position>(still);
        const Entity ghost = r.create();
        r.add<Velocity>(ghost);

        std::vector<Entity> seen;
        r.each<Position, Velocity>([&](Entity e, Position& p, Velocity& v) {
            seen.push_back(e);
            p.x += v.dx; // a tiny "movement system"
            p.y += v.dy;
        });
        REQUIRE(seen.size() == 1);
        CHECK(seen[0] == moving);
        CHECK(r.get<Position>(moving).x == 1.0f);
        CHECK(r.get<Position>(moving).y == 2.0f);
        CHECK(r.get<Position>(still).x == 0.0f);
    }

    TEST_CASE("each over a type nobody has visits nothing") {
        Registry r;
        r.create();
        int calls = 0;
        r.each<Velocity>([&](Entity, Velocity&) { ++calls; });
        CHECK(calls == 0);
    }

    TEST_CASE("each tolerates destroying and creating entities inside the loop") {
        Registry r;
        std::vector<Entity> entities;
        for (int i = 0; i < 6; ++i) {
            entities.push_back(r.create());
            r.add<Position>(entities.back(), static_cast<float>(i), 0.0f);
        }
        int visits = 0;
        r.each<Position>([&](Entity e, Position& p) {
            ++visits;
            if (p.x == 0.0f) {
                r.destroy(entities[5]); // not visited yet: must be skipped
                const Entity spawned = r.create();
                r.add<Position>(spawned, 100.0f, 0.0f); // created during the loop: not visited
            }
            CHECK(e != entities[5]);
        });
        CHECK(visits == 5);
        CHECK(r.count<Position>() == 6);
    }

    TEST_CASE("const each reads without changing anything") {
        Registry r;
        for (int i = 0; i < 3; ++i) {
            r.add<Position>(r.create(), static_cast<float>(i), 0.0f);
        }
        const Registry& view = r;
        float sum = 0.0f;
        view.each<Position>([&](Entity, const Position& p) { sum += p.x; });
        CHECK(sum == 3.0f);
    }

    TEST_CASE("many entities: handles stay unique and lookups stay correct") {
        Registry r;
        std::vector<Entity> entities;
        for (int i = 0; i < 1000; ++i) {
            entities.push_back(r.create());
            r.add<Position>(entities.back(), static_cast<float>(i), 0.0f);
        }
        // Destroy every third, then make new ones that reuse those slots.
        for (size_t i = 0; i < entities.size(); i += 3) {
            r.destroy(entities[i]);
        }
        for (int i = 0; i < 100; ++i) {
            r.add<Position>(r.create(), -1.0f, 0.0f);
        }
        std::unordered_set<Entity> unique;
        r.each<Position>([&](Entity e, Position&) { unique.insert(e); });
        CHECK(unique.size() == r.count<Position>());
        for (size_t i = 1; i < entities.size(); i += 3) {
            CHECK(r.get<Position>(entities[i]).x == static_cast<float>(i));
        }
    }
}
