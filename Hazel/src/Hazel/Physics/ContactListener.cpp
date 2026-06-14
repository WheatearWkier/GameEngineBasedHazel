#include "hzpch.h"
#include "ContactListener.h"

#include <box2d/b2_fixture.h>
#include <box2d/b2_body.h>

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scripting/ScriptEngine.h"

namespace Hazel {

    ContactListener::ContactListener(Scene* scene)
        : m_Scene(scene)
    {
    }

    Entity ContactListener::GetEntityFromFixture(b2Fixture* fixture)
    {
        if (!fixture)
            return {};

        b2Body* body = fixture->GetBody();
        if (!body)
            return {};

        uintptr_t data = body->GetUserData().pointer;
        if (data == 0)
            return {};

        UUID uuid = (UUID)data;
        return m_Scene->GetEntityByUUID(uuid);
    }

    void ContactListener::BeginContact(b2Contact* contact)
    {
        Entity entityA = GetEntityFromFixture(contact->GetFixtureA());
        Entity entityB = GetEntityFromFixture(contact->GetFixtureB());

        if (!entityA || !entityB)
        {
            HZ_CORE_WARN("Collision but entity invalid!");
            return;
        }

        // 只负责通知脚本（Unity）
        ScriptEngine::OnCollisionBegin(entityA, entityB);
        ScriptEngine::OnCollisionBegin(entityB, entityA);
    }

    void ContactListener::EndContact(b2Contact* contact)
    {
        Entity entityA = GetEntityFromFixture(contact->GetFixtureA());
        Entity entityB = GetEntityFromFixture(contact->GetFixtureB());

        if (!entityA || !entityB)
            return;

        ScriptEngine::OnCollisionEnd(entityA, entityB);
        ScriptEngine::OnCollisionEnd(entityB, entityA);
    }

}
