#include "hzpch.h"
#include "UIRenderer.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Hazel {

    uint32_t  UIRenderer::s_ViewportWidth = 1920;
    uint32_t  UIRenderer::s_ViewportHeight = 1080;
    glm::mat4 UIRenderer::s_SavedViewProjection = glm::mat4(1.0f);

    // ═══════════════════════════════════════════════════════════════════════
    //  坐标转换工具（file-local）
    // ═══════════════════════════════════════════════════════════════════════

    // 归一化坐标(0~1) -> NDC(-1~1)，Y 轴翻转（ImGui/屏幕 Y 向下，NDC Y 向上）
    static glm::vec2 NormToNDC(const glm::vec2& pos)
    {
        return { pos.x * 2.0f - 1.0f, 1.0f - pos.y * 2.0f };
    }

    static glm::vec2 NormSizeToNDC(const glm::vec2& size)
    {
        return { size.x * 2.0f, size.y * 2.0f };
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  WidgetToTransform
    //  将 UIWidgetComponent 的归一化布局信息转换为 Renderer2D 用的 mat4 变换。
    //  NDC 空间：X/Y 均在 [-1, 1]，Quad 本地坐标 [-0.5, 0.5]。
    // ═══════════════════════════════════════════════════════════════════════

    glm::mat4 UIRenderer::WidgetToTransform(
        const UIWidgetComponent& widget,
        uint32_t /*viewportWidth*/,
        uint32_t /*viewportHeight*/)
    {
        // Position 为中心点（归一化 0~1），转换为 NDC
        float ndcX = widget.Position.x * 2.0f - 1.0f;
        float ndcY = -(widget.Position.y * 2.0f - 1.0f); // Y 轴翻转

        // Renderer2D Quad 本地坐标 [-0.5, 0.5]
        // Scale 直接等于 NDC 尺寸，quad 会被缩放到正确大小
        float ndcW = widget.Size.x * 2.0f;
        float ndcH = widget.Size.y * 2.0f;

        // Anchor 偏移（在 NDC 空间中把中心点移到锚点对应位置）
        switch (widget.Anchor)
        {
        case UIAnchor::TopLeft:
            ndcX += ndcW * 0.5f;
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::TopCenter:
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::TopRight:
            ndcX -= ndcW * 0.5f;
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::MiddleLeft:
            ndcX += ndcW * 0.5f;
            break;
        case UIAnchor::MiddleCenter:
            break;
        case UIAnchor::MiddleRight:
            ndcX -= ndcW * 0.5f;
            break;
        case UIAnchor::BottomLeft:
            ndcX += ndcW * 0.5f;
            ndcY += ndcH * 0.5f;
            break;
        case UIAnchor::BottomCenter:
            ndcY += ndcH * 0.5f;
            break;
        case UIAnchor::BottomRight:
            ndcX -= ndcW * 0.5f;
            ndcY += ndcH * 0.5f;
            break;
        }

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(ndcX, ndcY, 0.0f));
        transform = glm::rotate(transform,
            glm::radians(widget.Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(ndcW, ndcH, 1.0f));

        return transform;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  UI 正交相机（NDC 直通投影）
    // ═══════════════════════════════════════════════════════════════════════

    class UIOrthographicCamera : public Camera
    {
    public:
        UIOrthographicCamera()
        {
            // NDC 直通：投影矩阵将 [-1,1]^3 映射到裁剪空间，相当于 identity
            m_Projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        }
    };

    // ═══════════════════════════════════════════════════════════════════════
    //  Pass 管理
    // ═══════════════════════════════════════════════════════════════════════

    void UIRenderer::BeginUIPass(uint32_t viewportWidth, uint32_t viewportHeight)
    {
        s_ViewportWidth = viewportWidth;
        s_ViewportHeight = viewportHeight;

        // UI 不做深度测试，始终绘制在场景上方
        RenderCommand::DisableDepthTest();

        // 使用单例 UI 相机（ortho NDC 直通），view = identity
        static UIOrthographicCamera uiCamera;
        Renderer2D::BeginScene(uiCamera, glm::mat4(1.0f));
    }

    void UIRenderer::EndUIPass(const glm::mat4& restoreViewProjection)
    {
        // 提交 UI 批次
        Renderer2D::EndScene();

        // BUG FIX: EndUIPass 之后调用 SetViewProjection 恢复游戏相机 VP 到 UBO，
        // 确保后续 OnOverlayRender 的 BeginScene 能在正确状态下开始。
        // 注意：SetViewProjection 只更新 UBO，不重置批次指针（批次已在 EndScene 中 flush）。
        Renderer2D::SetViewProjection(restoreViewProjection);

        // 恢复深度测试
        RenderCommand::EnableDepthTest();
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  绘制接口
    // ═══════════════════════════════════════════════════════════════════════

    void UIRenderer::DrawUIImage(
        const UIWidgetComponent& widget,
        const UIImageComponent& image,
        int entityID)
    {
        if (!widget.Visible) return;

        glm::mat4 transform = WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight);

        if (image.Texture)
            Renderer2D::DrawQuad(transform, image.Texture, 1.0f, image.Color, entityID);
        else
            Renderer2D::DrawQuad(transform, image.Color, entityID);
    }

    void UIRenderer::DrawUIButton(
        const UIWidgetComponent& widget,
        const UIButtonComponent& button,
        int entityID)
    {
        if (!widget.Visible) return;

        glm::mat4 transform = WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight);

        // 根据状态选色
        glm::vec4 color = button.NormalColor;
        if (button.IsPressed) color = button.PressedColor;
        else if (button.IsHovered) color = button.HoverColor;

        Renderer2D::DrawQuad(transform, color, entityID);
    }

    void UIRenderer::DrawUIProgressBar(
        const UIWidgetComponent& widget,
        const UIProgressBarComponent& bar,
        int entityID)
    {
        if (!widget.Visible) return;

        // 1. 背景（整个 widget 区域）
        glm::mat4 bgTransform = WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight);
        Renderer2D::DrawQuad(bgTransform, bar.BackgroundColor, entityID);

        float ratio = bar.GetNormalized();
        if (ratio <= 0.0f) return;

        // 2. 前景（从左边对齐，宽度 = 背景宽 * ratio）
        // 在 NDC 空间中直接构建前景 transform，不走 WidgetToTransform
        // 以保证前景从背景左边缘开始生长。
        float ndcCenterX = widget.Position.x * 2.0f - 1.0f;
        float ndcCenterY = -(widget.Position.y * 2.0f - 1.0f);
        float ndcFullW = widget.Size.x * 2.0f;
        float ndcH = widget.Size.y * 2.0f;
        float ndcFgW = ndcFullW * ratio;

        // 背景左边缘 + 前景半宽 = 前景中心 X
        float bgLeft = ndcCenterX - ndcFullW * 0.5f;
        float fgCenterX = bgLeft + ndcFgW * 0.5f;

        glm::mat4 fgTransform = glm::mat4(1.0f);
        fgTransform = glm::translate(fgTransform, glm::vec3(fgCenterX, ndcCenterY, 0.0f));
        fgTransform = glm::scale(fgTransform, glm::vec3(ndcFgW, ndcH, 1.0f));

        Renderer2D::DrawQuad(fgTransform, bar.ForegroundColor, entityID);
    }

} // namespace Hazel