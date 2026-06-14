#include "hzpch.h"
#include "RenderSystem.h"
#include "UISystem.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Renderer/Renderer3D.h"
#include "Hazel/UI/UIRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Hazel {

    // =========================================================================
    // Helpers
    // =========================================================================

    glm::mat4 RenderSystem::ComputeLightSpaceMatrix(Scene* scene)
    {
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TransformComponent, DirectionalLightComponent>())
        {
            auto [tc, dl] = registry.get<TransformComponent, DirectionalLightComponent>(e);

            glm::mat4 rot = glm::toMat4(glm::quat(tc.Rotation));
            glm::vec3 dir = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));

            // Orthographic projection that covers the scene; adjust extents as needed.
            const float size = 20.0f;
            glm::mat4 proj = glm::ortho(-size, size, -size, size, 0.1f, 100.0f);
            glm::vec3 lightPos = -dir * 30.0f;
            glm::mat4 view = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            return proj * view;
        }
        return glm::mat4(1.0f); // No directional light -- identity matrix
    }

    void RenderSystem::CollectLights(Scene* scene)
    {
        auto& registry = scene->GetRegistry();

        // Only the first directional light is used.
        bool hasDirLight = false;
        for (auto e : registry.view<TransformComponent, DirectionalLightComponent>())
        {
            if (hasDirLight) break;
            auto [tc, dl] = registry.get<TransformComponent, DirectionalLightComponent>(e);

            glm::mat4 rot = glm::toMat4(glm::quat(tc.Rotation));
            glm::vec3 dir = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));

            Renderer3D::SetDirectionalLight(dir, dl.Color, dl.Intensity);
            hasDirLight = true;
        }

        for (auto e : registry.view<TransformComponent, PointLightComponent>())
        {
            auto [tc, pl] = registry.get<TransformComponent, PointLightComponent>(e);
            Renderer3D::AddPointLight(tc.Translation, pl.Color, pl.Intensity,
                pl.Constant, pl.Linear, pl.Quadratic);
        }
    }

    void RenderSystem::RenderSceneShadow(Scene* scene)
    {
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TransformComponent, MeshRendererComponent>())
        {
            auto [tc, mr] = registry.get<TransformComponent, MeshRendererComponent>(e);
            if (mr.Mesh)
                Renderer3D::DrawMeshShadow(tc.GetTransform(), mr.Mesh);
        }
    }

    void RenderSystem::RenderScene2D(Scene* scene)
    {
        RenderCommand::DisableDepthTest();
        auto& registry = scene->GetRegistry();

        // Sort sprites back-to-front by Z translation before drawing.
        auto group = registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
        std::vector<entt::entity> sprites(group.begin(), group.end());
        std::sort(sprites.begin(), sprites.end(), [&](entt::entity a, entt::entity b) {
            return group.get<TransformComponent>(a).Translation.z
                < group.get<TransformComponent>(b).Translation.z;
            });

        for (auto e : sprites)
        {
            auto [tc, sr] = group.get<TransformComponent, SpriteRendererComponent>(e);
            Renderer2D::DrawSprite(tc.GetTransform(), sr, static_cast<int>(e));
        }

        for (auto e : registry.view<TransformComponent, CircleRendererComponent>())
        {
            auto [tc, cr] = registry.get<TransformComponent, CircleRendererComponent>(e);
            Renderer2D::DrawCircle(tc.GetTransform(), cr.Color,
                cr.Thickness, cr.Fade, static_cast<int>(e));
        }

        RenderCommand::EnableDepthTest();
    }

    void RenderSystem::RenderScene3D(Scene* scene)
    {
        RenderCommand::EnableDepthTest();
        auto& registry = scene->GetRegistry();

        for (auto e : registry.view<TransformComponent, MeshRendererComponent>())
        {
            auto [tc, mr] = registry.get<TransformComponent, MeshRendererComponent>(e);
            if (mr.Mesh)
                Renderer3D::DrawMesh(tc.GetTransform(), mr.Mesh,
                    mr.Material, static_cast<int>(e));
        }
    }

    // =========================================================================
    // Runtime update (game camera)
    // =========================================================================

    void RenderSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        // Find the primary camera.
        Camera* mainCamera = nullptr;
        glm::mat4 cameraTransform = glm::mat4(1.0f);
        for (auto e : scene->GetRegistry().view<TransformComponent, CameraComponent>())
        {
            auto [tc, cc] = scene->GetRegistry().get<TransformComponent, CameraComponent>(e);
            if (cc.Primary)
            {
                mainCamera = &cc.Camera;
                cameraTransform = tc.GetTransform();
                break;
            }
        }
        if (!mainCamera) return;

        glm::mat4 gameVP = mainCamera->GetProjection() * glm::inverse(cameraTransform);

        uint32_t previousFBO = RenderCommand::GetBoundFramebuffer(); // ¼ÓÕâÐÐ
        // Shadow pass
        Renderer3D::GetShadowMapFB()->Bind();
        RenderCommand::Clear();
        Renderer3D::BeginShadowPass(ComputeLightSpaceMatrix(scene));
        RenderSceneShadow(scene);
        Renderer3D::EndShadowPass();
        //Renderer3D::GetShadowMapFB()->Unbind();
        RenderCommand::BindFramebuffer(previousFBO);
        RenderCommand::SetViewport(0, 0, scene->GetViewportWidth(), scene->GetViewportHeight());

        Renderer2D::BeginScene(*mainCamera, cameraTransform);
        RenderScene2D(scene);
        Renderer2D::EndScene();

        Renderer3D::BeginScene(*mainCamera, cameraTransform);
        CollectLights(scene);
        RenderScene3D(scene);
        Renderer3D::EndScene();

        UIRenderer::BeginUIPass(scene->GetViewportWidth(), scene->GetViewportHeight());
        if (auto* ui = scene->GetSystem<UISystem>())
            ui->RenderUI(scene);
        UIRenderer::EndUIPass(gameVP);
    }

    // =========================================================================
    // Editor update (editor camera)
    // =========================================================================

    void RenderSystem::RenderWithEditorCamera(Scene* scene, EditorCamera& camera)
    {
        // Save the currently bound framebuffer so we can restore it after the
        // shadow pass (which binds its own FBO and changes the viewport).
        uint32_t previousFBO = RenderCommand::GetBoundFramebuffer();

        // Shadow pass
        Renderer3D::GetShadowMapFB()->Bind();
        RenderCommand::Clear();
        Renderer3D::BeginShadowPass(ComputeLightSpaceMatrix(scene));
        RenderSceneShadow(scene);
        Renderer3D::EndShadowPass();

        // Restore the editor's framebuffer and viewport.
        RenderCommand::BindFramebuffer(previousFBO);
        RenderCommand::SetViewport(0, 0, scene->GetViewportWidth(), scene->GetViewportHeight());

        Renderer2D::BeginScene(camera);
        RenderScene2D(scene);
        Renderer2D::EndScene();

        Renderer3D::BeginScene(camera);
        CollectLights(scene);
        RenderScene3D(scene);
        Renderer3D::EndScene();

        UIRenderer::BeginUIPass(scene->GetViewportWidth(), scene->GetViewportHeight());
        if (auto* ui = scene->GetSystem<UISystem>())
            ui->RenderUI(scene);
        UIRenderer::EndUIPass(camera.GetViewProjection());
    }

} // namespace Hazel