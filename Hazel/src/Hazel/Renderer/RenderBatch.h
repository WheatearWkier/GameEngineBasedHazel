#pragma once

#include "VertexArray.h"
#include "Shader.h"
#include "RenderCommand.h"
#include "Hazel/Renderer/Renderer2D.h"

#include <vector>
#include <functional>

namespace Hazel {

    /// 图元绘制模式
    enum class BatchDrawMode
    {
        Triangles,  // Quad / Circle 用
        Lines       // Line 用
    };

    /// 单种图元的 CPU→GPU 批处理缓冲
    /// 
    /// 每种图元（Quad、Circle、Line、未来的 Triangle 等）
    /// 实例化一个 RenderBatch，自己管理：
    ///   - CPU 端顶点缓冲
    ///   - VAO / VBO / IBO
    ///   - Shader 绑定
    ///   - Flush / Reset
    /// 
    /// Renderer2D 只持有一组 RenderBatch，Flush 时统一遍历，
    /// 新增图元只需新增一个 RenderBatch 实例，其他代码不用动。
    /// 
    /// @tparam TVertex  顶点结构体类型
    template<typename TVertex>
    class RenderBatch
    {
    public:
        /// @param maxVertices   CPU 缓冲可容纳的最大顶点数
        /// @param shader        使用的着色器
        /// @param layout        顶点布局
        /// @param drawMode      绘制模式（三角形 / 线段）
        /// @param indexBuffer   共用的索引缓冲（线段模式传 nullptr）
        RenderBatch(uint32_t maxVertices,
            Ref<Shader> shader,
            const BufferLayout& layout,
            BatchDrawMode drawMode,
            Ref<IndexBuffer> indexBuffer = nullptr)
            : m_Shader(std::move(shader))
            , m_DrawMode(drawMode)
            , m_MaxVertices(maxVertices)
        {
            // CPU 端缓冲
            m_VertexBufferBase = new TVertex[maxVertices];
            m_VertexBufferPtr = m_VertexBufferBase;

            // VAO + VBO
            m_VertexArray = VertexArray::Create();
            m_VertexBuffer = VertexBuffer::Create(maxVertices * sizeof(TVertex));
            m_VertexBuffer->SetLayout(layout);
            m_VertexArray->AddVertexBuffer(m_VertexBuffer);

            if (indexBuffer)
                m_VertexArray->SetIndexBuffer(indexBuffer);
        }

        ~RenderBatch()
        {
            delete[] m_VertexBufferBase;
        }

        // 禁止拷贝，允许移动
        RenderBatch(const RenderBatch&) = delete;
        RenderBatch& operator=(const RenderBatch&) = delete;
        RenderBatch(RenderBatch&&) = default;

        /// 向批次追加顶点，返回可写入的顶点指针
        /// 调用方写完顶点后调用 AdvanceVertex() 推进指针
        TVertex* AllocVertex()
        {
            return m_VertexBufferPtr++;
        }

        /// 通知批次本次提交涉及的索引数（Triangles 模式用）
        void AddIndexCount(uint32_t count) { m_IndexCount += count; }

        /// 通知批次本次提交涉及的顶点数（Lines 模式用）
        void AddVertexCount(uint32_t count) { m_VertexCount += count; }

        /// 当前批次是否为空
        bool IsEmpty() const
        {
            return m_DrawMode == BatchDrawMode::Lines
                ? m_VertexCount == 0
                : m_IndexCount == 0;
        }

        /// 判断是否已满，满时外部应先 Flush 再继续写
        bool IsFull(uint32_t extraVertices = 4) const
        {
            return (m_VertexBufferPtr - m_VertexBufferBase) + extraVertices > m_MaxVertices;
        }

        /// 上传数据并提交 Draw Call
        /// @return 实际产生的 DrawCall 次数（0 或 1）
        uint32_t Flush(Renderer2D::Statistics& stats)
        {
            if (IsEmpty()) return 0;

            const size_t dataSize =
                (m_VertexBufferPtr - m_VertexBufferBase) * sizeof(TVertex);
            m_VertexBuffer->SetData(m_VertexBufferBase, (uint32_t)dataSize);

            // 纹理绑定由外部（Renderer2D）在 Flush 前统一做
            OnPreDraw();

            m_Shader->Bind();

            if (m_DrawMode == BatchDrawMode::Lines)
            {
                RenderCommand::SetLineWidth(m_LineWidth);
                RenderCommand::DrawLines(m_VertexArray, m_VertexCount);
            }
            else
            {
                RenderCommand::DrawIndexed(m_VertexArray, m_IndexCount);
            }

            stats.DrawCalls++;
            return 1;
        }

        /// 重置指针，准备下一帧
        void Reset()
        {
            m_VertexBufferPtr = m_VertexBufferBase;
            m_IndexCount = 0;
            m_VertexCount = 0;
        }

        /// 设置线宽（仅 Lines 模式有效）
        void SetLineWidth(float w) { m_LineWidth = w; }
        float GetLineWidth()  const { return m_LineWidth; }

        Ref<Shader>& GetShader() { return m_Shader; }

        /// 注册预绘制回调（用于纹理绑定等）
        void SetPreDrawCallback(std::function<void()> cb) { m_PreDrawCallback = std::move(cb); }

    private:
        void OnPreDraw()
        {
            if (m_PreDrawCallback) m_PreDrawCallback();
        }

    private:
        Ref<VertexArray>  m_VertexArray;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<Shader>       m_Shader;
        BatchDrawMode     m_DrawMode;

        uint32_t  m_MaxVertices = 0;
        uint32_t  m_IndexCount = 0;  // Triangles 模式
        uint32_t  m_VertexCount = 0;  // Lines 模式
        float     m_LineWidth = 2.0f;

        TVertex* m_VertexBufferBase = nullptr;
        TVertex* m_VertexBufferPtr = nullptr;

        std::function<void()> m_PreDrawCallback;
    };

} // namespace Hazel
