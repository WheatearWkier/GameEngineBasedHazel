#pragma once
// =============================================================
//  Renderer3D.h  (updated for IBL)
//
//  Changes vs original:
//    - Added SetIBL() / ClearIBL() / SetIBLIntensity()
//    - SetSkyboxColors() still works as a gradient fallback
//    - IBLData UBO replaces old Skybox UBO at binding = 10
//    - SkyboxGradient UBO is now at binding = 14
// =============================================================

#include "Camera.h"
#include "EditorCamera.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "Framebuffer.h"
#include "IBLPrecompute.h"
#include <glm/glm.hpp>

namespace Hazel {

    static constexpr uint32_t k_MaxPointLights = 8;

    class Renderer3D
    {
    public:
        static void Init();
        static void Shutdown();

        static void BeginScene(const Camera& camera, const glm::mat4& cameraTransform);
        static void BeginScene(const EditorCamera& camera);
        static void EndScene();

        // ── Lights ──────────────────────────────────────────
        static void SetDirectionalLight(const glm::vec3& direction,
                                        const glm::vec3& color,
                                        float intensity);
        static void AddPointLight(const glm::vec3& position,
                                  const glm::vec3& color,
                                  float intensity,
                                  float constant,
                                  float linear,
                                  float quadratic);
        static void FlushLights();

        // ── Shadow ──────────────────────────────────────────
        static void BeginShadowPass(const glm::mat4& lightSpaceMatrix);
        static void EndShadowPass();
        static void DrawMeshShadow(const glm::mat4& transform, const Ref<Mesh>& mesh);

        static uint32_t          GetShadowMapID();
        static const glm::mat4&  GetLightSpaceMatrix();
        static Ref<Framebuffer>& GetShadowMapFB();

        // ── Mesh rendering ───────────────────────────────────
        static void DrawMesh(const glm::mat4& transform,
                             const Ref<Mesh>& mesh,
                             const Ref<Material>& material,
                             int entityID = -1);

        // ── IBL ─────────────────────────────────────────────
        // Load a precomputed IBL set (call IBLPrecompute::Compute() first).
        // Binds textures to slots 11, 12, 13 and flips u_HasIBL = 1.
        static void SetIBL(const Ref<IBLResult>& ibl);

        // Remove IBL — skybox falls back to gradient, mesh uses hemisphere ambient.
        static void ClearIBL();

        // Scale the entire indirect (ambient) contribution. Default = 1.0.
        static void SetIBLIntensity(float intensity);

        // SSAO
        static void InitSSAO(uint32_t width, uint32_t height);
        static void ResizeSSAO(uint32_t width, uint32_t height);
        static void ComputeSSAO(uint32_t normalTex, uint32_t depthTex, const glm::mat4& projection);
        static void BindSSAOResult();

        static bool& SSAOEnabled();
        static float& SSAORadius();
        static float& SSAOBias();
        static float& SSAOPower();

        // ── Editor visuals ───────────────────────────────────
        static void DrawEditorSkybox();
        static void DrawEditorGrid();

        // Gradient colours used when no IBL is loaded.
        static void SetSkyboxColors(const glm::vec3& top,
                                    const glm::vec3& horizon,
                                    const glm::vec3& bottom);

        // ── Stats ────────────────────────────────────────────
        struct Statistics { uint32_t DrawCalls = 0; uint32_t MeshCount = 0; };
        static Statistics GetStats();
        static void       ResetStats();
    };

} // namespace Hazel
