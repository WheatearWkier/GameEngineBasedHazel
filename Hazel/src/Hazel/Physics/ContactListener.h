#pragma once

#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>

namespace Hazel {

    class Entity;
    class Scene;

    class ContactListener : public b2ContactListener
    {
    public:
        ContactListener(class Scene* scene);

        virtual void BeginContact(b2Contact* contact) override;
        virtual void EndContact(b2Contact* contact) override;

    private:
        Entity GetEntityFromFixture(b2Fixture* fixture);

    private:
        Scene* m_Scene = nullptr;
    };

}
