#pragma once
#include "ISystem.h"
#include "Hazel/Renderer/EditorCamera.h"

namespace Hazel {

    class RenderSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void RenderWithEditorCamera(Scene* scene, EditorCamera& camera);

    private:
        glm::mat4 ComputeLightSpaceMatrix(Scene* scene);
        void RenderSceneShadow(Scene* scene);
        void CollectLights(Scene* scene);
        void RenderScene2D(Scene* scene);
        void RenderScene3D(Scene* scene);
    };

} // namespace Hazel