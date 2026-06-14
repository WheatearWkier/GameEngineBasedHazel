#include "hzpch.h"
#include "ScriptSystem.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Scene/ScriptableEntity.h"
#include "Hazel/Scripting/ScriptEngine.h"

namespace Hazel {

    void ScriptSystem::OnRuntimeStart(Scene* scene)
    {
        ScriptEngine::OnRuntimeStart(scene);
        for (auto e : scene->GetRegistry().view<ScriptComponent>())
            ScriptEngine::OnCreateEntity({ e, scene });
    }

    void ScriptSystem::OnRuntimeStop(Scene* scene)
    {
        ScriptEngine::OnRuntimeStop();
    }

    void ScriptSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        auto& registry = scene->GetRegistry();

        //// NativeScript£¨C++ ÄÚÇ¶½Å±¾£©
        //registry.view<NativeScriptComponent>().each([=](auto e, auto& nsc)
        //    {
        //        if (!nsc.Instance)
        //        {
        //            nsc.Instance = nsc.InstantiateScript();
        //            nsc.Instance->m_Entity = { e, scene };
        //            nsc.Instance->OnCreate();
        //        }
        //        nsc.Instance->OnUpdate(ts);
        //    });

        // C# ½Å±¾
        for (auto e : registry.view<ScriptComponent>())
            ScriptEngine::OnUpdateEntity({ e, scene }, ts);
    }

    void ScriptSystem::OnEntityCreated(Scene* scene, Entity& entity)
    {
        if (!entity.HasComponent<ScriptComponent>()) return;
        ScriptEngine::OnCreateEntity(entity);
    }

    // ScriptSystem.cpp
    void ScriptSystem::OnEntityDestroy(Scene* scene, Entity& entity)
    {
        ScriptEngine::OnDestroyEntity(entity);
    }

} // namespace Hazel