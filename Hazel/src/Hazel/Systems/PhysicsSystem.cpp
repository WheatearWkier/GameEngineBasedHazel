#include "hzpch.h"
#include "PhysicsSystem.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Physics/ContactListener.h"

#include <box2d/b2_world.h>
#include <box2d/b2_body.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_circle_shape.h>

namespace Hazel {

    // ── 内部辅助 ──────────────────────────────────────────────────────────────

    namespace {

        b2BodyType RigidbodyTypeToBox2D(Rigidbody2DComponent::BodyType type)
        {
            switch (type)
            {
            case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
            case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
            case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
            }
            HZ_CORE_ASSERT(false, "Unknown Rigidbody2D BodyType");
            return b2_staticBody;
        }

    } // anonymous namespace

    // ── PhysicsSystem ─────────────────────────────────────────────────────────

    void PhysicsSystem::OnRuntimeStart(Scene* scene)
    {
        m_PhysicsWorld = new b2World({ 0.0f, -9.8f });
        m_ContactListener = new ContactListener(scene);
        m_PhysicsWorld->SetContactListener(m_ContactListener);

        // 为场景中所有已有刚体创建 Box2D body
        for (auto e : scene->GetRegistry().view<Rigidbody2DComponent>())
            InitEntityPhysics(scene, { e, scene });
    }

    void PhysicsSystem::OnRuntimeStop(Scene* scene)
    {
        delete m_PhysicsWorld;    m_PhysicsWorld = nullptr;
        delete m_ContactListener; m_ContactListener = nullptr;
    }

    void PhysicsSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        constexpr int32_t velocityIterations = 6;
        constexpr int32_t positionIterations = 2;
        m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

        // 物理结果回写到 TransformComponent
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<Rigidbody2DComponent>())
        {
            auto& tc = registry.get<TransformComponent>(e);
            auto& rb2d = registry.get<Rigidbody2DComponent>(e);

            b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
            if (!body) continue;

            const auto& pos = body->GetPosition();
            tc.Translation.x = pos.x;
            tc.Translation.y = pos.y;
            tc.Rotation.z = body->GetAngle();
        }
    }

    void PhysicsSystem::InitEntityPhysics(Scene* scene, Entity entity)
    {
        HZ_CORE_ASSERT(m_PhysicsWorld, "PhysicsSystem: world not initialized");

        auto& tc = entity.GetComponent<TransformComponent>();
        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

        b2BodyDef bodyDef;
        bodyDef.type = RigidbodyTypeToBox2D(rb2d.Type);
        bodyDef.position = { tc.Translation.x, tc.Translation.y };
        bodyDef.angle = tc.Rotation.z;

        b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
        body->SetFixedRotation(rb2d.FixedRotation);
        body->SetGravityScale(rb2d.GravityScale);
        body->GetUserData().pointer =
            static_cast<uintptr_t>(static_cast<uint64_t>(entity.GetUUID()));
        rb2d.RuntimeBody = body;

        // 工厂 lambda，避免重复填写 FixtureDef 字段
        auto makeFixture = [&](b2Shape& shape, float density, float friction,
            float restitution, float restitutionThreshold)
            {
                b2FixtureDef fd;
                fd.shape = &shape;
                fd.density = density;
                fd.friction = friction;
                fd.restitution = restitution;
                fd.restitutionThreshold = restitutionThreshold;
                body->CreateFixture(&fd);
            };

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            auto& bc = entity.GetComponent<BoxCollider2DComponent>();
            b2PolygonShape shape;
            shape.SetAsBox(bc.Size.x * tc.Scale.x, bc.Size.y * tc.Scale.y,
                { bc.Offset.x, bc.Offset.y }, 0.0f);
            makeFixture(shape, bc.Density, bc.Friction,
                bc.Restitution, bc.RestitutionThreshold);
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            auto& cc = entity.GetComponent<CircleCollider2DComponent>();
            b2CircleShape shape;
            shape.m_p = { cc.Offset.x, cc.Offset.y };
            shape.m_radius = cc.Radius * tc.Scale.x;
            makeFixture(shape, cc.Density, cc.Friction,
                cc.Restitution, cc.RestitutionThreshold);
        }
    }

    void PhysicsSystem::OnEntityCreated(Scene* scene, Entity& entity)
    {
        // 只在运行时且实体有刚体时才初始化
        if (!m_PhysicsWorld) return;
        if (!entity.HasComponent<Rigidbody2DComponent>()) return;
        InitEntityPhysics(scene, entity);
    }

    void PhysicsSystem::OnEntityDestroy(Scene* scene, Entity& entity)
    {
        if (!entity.HasComponent<Rigidbody2DComponent>()) return;
        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        if (rb2d.RuntimeBody && m_PhysicsWorld)
        {
            m_PhysicsWorld->DestroyBody(static_cast<b2Body*>(rb2d.RuntimeBody));
            rb2d.RuntimeBody = nullptr;
        }
    }

} // namespace Hazel