#include "hzpch.h"
#include "Material.h"
#include "Hazel/Core/Log.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>

namespace Hazel {

    Ref<Material> Material::Create()
    {
        return CreateRef<Material>();
    }

    Ref<Material> Material::Load(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            HZ_CORE_ERROR("Material::Load: 无法打开文件 {0}", path);
            return CreateRef<Material>();
        }

        YAML::Node root = YAML::Load(file);
        YAML::Node node = root["Material"];
        if (!node)
        {
            HZ_CORE_ERROR("Material::Load: 文件格式错误 {0}", path);
            return CreateRef<Material>();
        }

        auto mat = CreateRef<Material>();
        mat->m_Path = path;

        if (node["Albedo"])
        {
            auto a = node["Albedo"];
            mat->Albedo = { a[0].as<float>(), a[1].as<float>(),
                            a[2].as<float>(), a[3].as<float>() };
        }
        mat->Metallic = node["Metallic"].as<float>(0.0f);
        mat->Roughness = node["Roughness"].as<float>(0.5f);
        mat->FlipNormals = node["FlipNormals"].as<bool>(false);

        auto loadTex = [](const YAML::Node& n, const char* key) -> Ref<Texture2D>
            {
                if (!n[key]) return nullptr;
                std::string p = n[key].as<std::string>("");
                if (p.empty()) return nullptr;
                return Texture2D::Create(p);
            };

        mat->AlbedoMap = loadTex(node, "AlbedoMap");
        mat->NormalMap = loadTex(node, "NormalMap");
        mat->RoughnessMap = loadTex(node, "RoughnessMap");
        mat->MetallicMap = loadTex(node, "MetallicMap");

        return mat;
    }

    void Material::Save(const std::string& path)
    {
        m_Path = path;
        Save();
    }

    void Material::Save()
    {
        if (m_Path.empty())
        {
            HZ_CORE_ERROR("Material::Save: 路径为空 ");
            return;
        }

        // 自动创建目录
        std::filesystem::create_directories(
            std::filesystem::path(m_Path).parent_path()
        );

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Material" << YAML::BeginMap;

        out << YAML::Key << "Albedo" << YAML::Value
            << YAML::Flow << YAML::BeginSeq
            << Albedo.r << Albedo.g << Albedo.b << Albedo.a
            << YAML::EndSeq;
        out << YAML::Key << "Metallic" << YAML::Value << Metallic;
        out << YAML::Key << "Roughness" << YAML::Value << Roughness;
        out << YAML::Key << "FlipNormals" << YAML::Value << FlipNormals;

        auto texPath = [](const Ref<Texture2D>& t) -> std::string {
            return t ? t->GetPath() : "";
            };
        out << YAML::Key << "AlbedoMap" << YAML::Value << texPath(AlbedoMap);
        out << YAML::Key << "NormalMap" << YAML::Value << texPath(NormalMap);
        out << YAML::Key << "RoughnessMap" << YAML::Value << texPath(RoughnessMap);
        out << YAML::Key << "MetallicMap" << YAML::Value << texPath(MetallicMap);

        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream file(m_Path);
        file << out.c_str();
        m_Dirty = false;
    }

} // namespace Hazel