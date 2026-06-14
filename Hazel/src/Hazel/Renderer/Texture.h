#pragma once

#include <string>
#include <cstdint>

namespace Hazel {

    class Texture
    {
    public:
        virtual ~Texture() = default;

        virtual uint32_t GetWidth()      const = 0;
        virtual uint32_t GetHeight()     const = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual void SetData(void* data, uint32_t size) = 0;
        virtual void Bind(uint32_t slot = 0) const = 0;

        virtual bool IsLoaded() const = 0; // 预留：判断纹理是否加载成功

        virtual bool operator==(const Texture& other) const = 0;
    };

    class Texture2D : public Texture
    {
    public:
        // 获得纹理路径
        virtual const std::string& GetPath() const = 0;

        // 创建空白纹理（用于 SetData 填充）
        static Ref<Texture2D> Create(uint32_t width, uint32_t height);

        // 从文件路径加载
        static Ref<Texture2D> Create(const std::string& path);
    };

} // namespace Hazel