#include "hzpch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"
#include "Hazel/Animation/AnimationClip.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

// ═══════════════════════════════════════════════════════════════════
//  YAML 转换器（glm 类型，不变）
// ═══════════════════════════════════════════════════════════════════

namespace YAML {

    template<> struct convert<glm::vec2> {
        static Node encode(const glm::vec2& v) {
            Node n; n.push_back(v.x); n.push_back(v.y); return n;
        }
        static bool decode(const Node& n, glm::vec2& v) {
            if (!n.IsSequence() || n.size() != 2) return false;
            v = { n[0].as<float>(), n[1].as<float>() }; return true;
        }
    };
    template<> struct convert<glm::vec3> {
        static Node encode(const glm::vec3& v) {
            Node n; n.push_back(v.x); n.push_back(v.y); n.push_back(v.z); return n;
        }
        static bool decode(const Node& n, glm::vec3& v) {
            if (!n.IsSequence() || n.size() != 3) return false;
            v = { n[0].as<float>(), n[1].as<float>(), n[2].as<float>() }; return true;
        }
    };
    template<> struct convert<glm::vec4> {
        static Node encode(const glm::vec4& v) {
            Node n; n.push_back(v.x); n.push_back(v.y);
            n.push_back(v.z); n.push_back(v.w); return n;
        }
        static bool decode(const Node& n, glm::vec4& v) {
            if (!n.IsSequence() || n.size() != 4) return false;
            v = { n[0].as<float>(), n[1].as<float>(),
                  n[2].as<float>(), n[3].as<float>() }; return true;
        }
    };

} // namespace YAML

namespace Hazel {

    // ── Emitter 辅助（不变）──────────────────────────────────────────
    static YAML::Emitter& operator<<(YAML::Emitter& o, const glm::vec2& v)
    {
        return o << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
    }
    static YAML::Emitter& operator<<(YAML::Emitter& o, const glm::vec3& v)
    {
        return o << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
    }
    static YAML::Emitter& operator<<(YAML::Emitter& o, const glm::vec4& v)
    {
        return o << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
    }

    // ── BodyType 转换（不变）────────────────────────────────────────
    static const char* BodyTypeToString(Rigidbody2DComponent::BodyType t) {
        switch (t) {
        case Rigidbody2DComponent::BodyType::Static:    return "Static";
        case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
        case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
        }
        return "Static";
    }
    static Rigidbody2DComponent::BodyType BodyTypeFromString(const std::string& s) {
        if (s == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
        if (s == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;
        return Rigidbody2DComponent::BodyType::Static;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  ComponentSerializer 特化
    //  每个特化提供两个静态方法：
    //    Serialize(YAML::Emitter&, const T&)
    //    Deserialize(const YAML::Node&, T&)
    //  以及一个静态字符串 Key（YAML 中的键名）
    // ═══════════════════════════════════════════════════════════════════

    template<> struct ComponentSerializer<TagComponent> {
        static constexpr const char* Key = "TagComponent";
        static void Serialize(YAML::Emitter& o, const TagComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Tag" << YAML::Value << c.Tag;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TagComponent& c) {
            c.Tag = n["Tag"].as<std::string>();
        }
    };

    template<> struct ComponentSerializer<TransformComponent> {
        static constexpr const char* Key = "TransformComponent";
        static void Serialize(YAML::Emitter& o, const TransformComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Translation" << YAML::Value << c.Translation;
            o << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
            o << YAML::Key << "Scale" << YAML::Value << c.Scale;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TransformComponent& c) {
            c.Translation = n["Translation"].as<glm::vec3>();
            c.Rotation = n["Rotation"].as<glm::vec3>();
            c.Scale = n["Scale"].as<glm::vec3>();
        }
    };

    template<> struct ComponentSerializer<CameraComponent> {
        static constexpr const char* Key = "CameraComponent";
        static void Serialize(YAML::Emitter& o, const CameraComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Camera" << YAML::BeginMap;
            o << YAML::Key << "ProjectionType" << YAML::Value << (int)c.Camera.GetProjectionType();
            o << YAML::Key << "PerspectiveFOV" << YAML::Value << c.Camera.GetPerspectiveVerticalFOV();
            o << YAML::Key << "PerspectiveNear" << YAML::Value << c.Camera.GetPerspectiveNearClip();
            o << YAML::Key << "PerspectiveFar" << YAML::Value << c.Camera.GetPerspectiveFarClip();
            o << YAML::Key << "OrthographicSize" << YAML::Value << c.Camera.GetOrthographicSize();
            o << YAML::Key << "OrthographicNear" << YAML::Value << c.Camera.GetOrthographicNearClip();
            o << YAML::Key << "OrthographicFar" << YAML::Value << c.Camera.GetOrthographicFarClip();
            o << YAML::EndMap;
            o << YAML::Key << "Primary" << YAML::Value << c.Primary;
            o << YAML::Key << "FixedAspectRatio" << YAML::Value << c.FixedAspectRatio;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CameraComponent& c) {
            auto cam = n["Camera"];
            c.Camera.SetProjectionType(
                (SceneCamera::ProjectionType)cam["ProjectionType"].as<int>());
            c.Camera.SetPerspectiveVerticalFOV(cam["PerspectiveFOV"].as<float>());
            c.Camera.SetPerspectiveNearClip(cam["PerspectiveNear"].as<float>());
            c.Camera.SetPerspectiveFarClip(cam["PerspectiveFar"].as<float>());
            c.Camera.SetOrthographicSize(cam["OrthographicSize"].as<float>());
            c.Camera.SetOrthographicNearClip(cam["OrthographicNear"].as<float>());
            c.Camera.SetOrthographicFarClip(cam["OrthographicFar"].as<float>());
            c.Primary = n["Primary"].as<bool>();
            c.FixedAspectRatio = n["FixedAspectRatio"].as<bool>();
        }
    };

    template<> struct ComponentSerializer<SpriteRendererComponent> {
        static constexpr const char* Key = "SpriteRendererComponent";
        static void Serialize(YAML::Emitter& o, const SpriteRendererComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Texture" << YAML::Value << (c.Texture ? c.Texture->GetPath() : "");
            o << YAML::Key << "TilingFactor" << YAML::Value << c.TilingFactor;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SpriteRendererComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            c.TilingFactor = n["TilingFactor"].as<float>(1.0f);
            if (auto p = n["Texture"].as<std::string>(""); !p.empty())
                c.Texture = Texture2D::Create(p);
        }
    };

    template<> struct ComponentSerializer<CircleRendererComponent> {
        static constexpr const char* Key = "CircleRendererComponent";
        static void Serialize(YAML::Emitter& o, const CircleRendererComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Thickness" << YAML::Value << c.Thickness;
            o << YAML::Key << "Fade" << YAML::Value << c.Fade;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CircleRendererComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            c.Thickness = n["Thickness"].as<float>();
            c.Fade = n["Fade"].as<float>();
        }
    };

    template<> struct ComponentSerializer<Rigidbody2DComponent> {
        static constexpr const char* Key = "Rigidbody2DComponent";
        static void Serialize(YAML::Emitter& o, const Rigidbody2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "BodyType" << YAML::Value << BodyTypeToString(c.Type);
            o << YAML::Key << "FixedRotation" << YAML::Value << c.FixedRotation;
            o << YAML::Key << "GravityScale" << YAML::Value << c.GravityScale;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, Rigidbody2DComponent& c) {
            c.Type = BodyTypeFromString(n["BodyType"].as<std::string>());
            c.FixedRotation = n["FixedRotation"].as<bool>();
            c.GravityScale = n["GravityScale"].as<float>(1.0f);
        }
    };

    template<> struct ComponentSerializer<BoxCollider2DComponent> {
        static constexpr const char* Key = "BoxCollider2DComponent";
        static void Serialize(YAML::Emitter& o, const BoxCollider2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Offset" << YAML::Value << c.Offset;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Density" << YAML::Value << c.Density;
            o << YAML::Key << "Friction" << YAML::Value << c.Friction;
            o << YAML::Key << "Restitution" << YAML::Value << c.Restitution;
            o << YAML::Key << "RestitutionThreshold" << YAML::Value << c.RestitutionThreshold;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, BoxCollider2DComponent& c) {
            c.Offset = n["Offset"].as<glm::vec2>();
            c.Size = n["Size"].as<glm::vec2>();
            c.Density = n["Density"].as<float>();
            c.Friction = n["Friction"].as<float>();
            c.Restitution = n["Restitution"].as<float>();
            c.RestitutionThreshold = n["RestitutionThreshold"].as<float>();
        }
    };

    template<> struct ComponentSerializer<CircleCollider2DComponent> {
        static constexpr const char* Key = "CircleCollider2DComponent";
        static void Serialize(YAML::Emitter& o, const CircleCollider2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Offset" << YAML::Value << c.Offset;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::Key << "Density" << YAML::Value << c.Density;
            o << YAML::Key << "Friction" << YAML::Value << c.Friction;
            o << YAML::Key << "Restitution" << YAML::Value << c.Restitution;
            o << YAML::Key << "RestitutionThreshold" << YAML::Value << c.RestitutionThreshold;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CircleCollider2DComponent& c) {
            c.Offset = n["Offset"].as<glm::vec2>();
            c.Radius = n["Radius"].as<float>();
            c.Density = n["Density"].as<float>();
            c.Friction = n["Friction"].as<float>();
            c.Restitution = n["Restitution"].as<float>();
            c.RestitutionThreshold = n["RestitutionThreshold"].as<float>();
        }
    };

    //template<> struct ComponentSerializer<MeshRendererComponent>
    //{
    //    static constexpr const char* Key = "MeshRendererComponent";
    //    static void Serialize(YAML::Emitter& o, const MeshRendererComponent& c)
    //    {
    //        o << YAML::Key << Key << YAML::BeginMap;
    //        o << YAML::Key << "MeshPath" << YAML::Value << (c.Mesh ? c.Mesh->GetFilepath() : "");
    //        o << YAML::Key << "Albedo" << YAML::Value << c.Material.Albedo;
    //        o << YAML::Key << "Metallic" << YAML::Value << c.Material.Metallic;
    //        o << YAML::Key << "Roughness" << YAML::Value << c.Material.Roughness;
    //        o << YAML::EndMap;
    //    }
    //    static void Deserialize(const YAML::Node& n, MeshRendererComponent& c)
    //    {
    //        c.Material.Albedo = n["Albedo"].as<glm::vec4>(glm::vec4(1.0f));
    //        c.Material.Metallic = n["Metallic"].as<float>(0.0f);
    //        c.Material.Roughness = n["Roughness"].as<float>(0.5f);
    //        auto path = n["MeshPath"].as<std::string>("");
    //        if (!path.empty())
    //            c.Mesh = Mesh::Create(path);
    //        else
    //            c.Mesh = Mesh::CreateCube(); // 路径为空则用默认立方体
    //    }
    //};

    template<> struct ComponentSerializer<MeshRendererComponent>
    {
        static constexpr const char* Key = "MeshRendererComponent";
        static void Serialize(YAML::Emitter& o, const MeshRendererComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "MeshPath" << YAML::Value
                << (c.Mesh ? c.Mesh->GetFilepath() : "");
            o << YAML::Key << "MaterialPath" << YAML::Value
                << (c.Material ? c.Material->GetPath() : "");
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, MeshRendererComponent& c)
        {
            auto meshPath = n["MeshPath"].as<std::string>("");
            if (!meshPath.empty()) c.Mesh = Mesh::Create(meshPath);
            else                   c.Mesh = Mesh::CreateCube();

            auto matPath = n["MaterialPath"].as<std::string>("");
            if (!matPath.empty()) c.Material = Material::Load(matPath);
            else                  c.Material = Material::Create();
        }
    };

    template<> struct ComponentSerializer<DirectionalLightComponent>
    {
        static constexpr const char* Key = "DirectionalLightComponent";
        static void Serialize(YAML::Emitter& o, const DirectionalLightComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Intensity" << YAML::Value << c.Intensity;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, DirectionalLightComponent& c)
        {
            c.Color = n["Color"].as<glm::vec3>();
            c.Intensity = n["Intensity"].as<float>(1.0f);
        }
    };

    template<> struct ComponentSerializer<PointLightComponent>
    {
        static constexpr const char* Key = "PointLightComponent";
        static void Serialize(YAML::Emitter& o, const PointLightComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Intensity" << YAML::Value << c.Intensity;
            o << YAML::Key << "Constant" << YAML::Value << c.Constant;
            o << YAML::Key << "Linear" << YAML::Value << c.Linear;
            o << YAML::Key << "Quadratic" << YAML::Value << c.Quadratic;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, PointLightComponent& c)
        {
            c.Color = n["Color"].as<glm::vec3>();
            c.Intensity = n["Intensity"].as<float>(1.0f);
            c.Constant = n["Constant"].as<float>(1.0f);
            c.Linear = n["Linear"].as<float>(0.09f);
            c.Quadratic = n["Quadratic"].as<float>(0.032f);
        }
    };

    template<> struct ComponentSerializer<ScriptComponent> {
        static constexpr const char* Key = "ScriptComponent";
        static void Serialize(YAML::Emitter& o, const ScriptComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ClassName" << YAML::Value << c.ClassName;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ScriptComponent& c) {
            c.ClassName = n["ClassName"].as<std::string>();
        }
    };

    template<> struct ComponentSerializer<AudioSourceComponent> {
        static constexpr const char* Key = "AudioSourceComponent";
        static void Serialize(YAML::Emitter& o, const AudioSourceComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "AudioFilePath" << YAML::Value << c.AudioFilePath;
            o << YAML::Key << "Volume" << YAML::Value << c.Volume;
            o << YAML::Key << "Loop" << YAML::Value << c.Loop;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, AudioSourceComponent& c) {
            c.AudioFilePath = n["AudioFilePath"].as<std::string>();
            c.Volume = n["Volume"].as<float>();
            c.Loop = n["Loop"].as<bool>();
            c.PlayOnStart = n["PlayOnStart"].as<bool>();
        }
    };

    template<> struct ComponentSerializer<UICanvasComponent> {
        static constexpr const char* Key = "UICanvasComponent";
        static void Serialize(YAML::Emitter& o, const UICanvasComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Visible" << YAML::Value << c.Visible;
            o << YAML::Key << "ReferenceWidth" << YAML::Value << c.ReferenceWidth;
            o << YAML::Key << "ReferenceHeight" << YAML::Value << c.ReferenceHeight;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UICanvasComponent& c) {
            c.Visible = n["Visible"].as<bool>();
            c.ReferenceWidth = n["ReferenceWidth"].as<float>();
            c.ReferenceHeight = n["ReferenceHeight"].as<float>();
        }
    };

    template<> struct ComponentSerializer<UIWidgetComponent> {
        static constexpr const char* Key = "UIWidgetComponent";
        static void Serialize(YAML::Emitter& o, const UIWidgetComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Visible" << YAML::Value << c.Visible;
            o << YAML::Key << "Position" << YAML::Value << c.Position;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
            o << YAML::Key << "Anchor" << YAML::Value << (int)c.Anchor;
            o << YAML::Key << "SortOrder" << YAML::Value << c.SortOrder;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIWidgetComponent& c) {
            c.Visible = n["Visible"].as<bool>();
            c.Position = n["Position"].as<glm::vec2>();
            c.Size = n["Size"].as<glm::vec2>();
            c.Rotation = n["Rotation"].as<float>();
            c.Anchor = (UIAnchor)n["Anchor"].as<int>();
            c.SortOrder = n["SortOrder"].as<int>();
        }
    };

    template<> struct ComponentSerializer<UIImageComponent> {
        static constexpr const char* Key = "UIImageComponent";
        static void Serialize(YAML::Emitter& o, const UIImageComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "TexturePath" << YAML::Value << (c.Texture ? c.Texture->GetPath() : "");
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIImageComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            if (auto p = n["TexturePath"].as<std::string>(""); !p.empty())
                c.Texture = Texture2D::Create(p);
        }
    };

    template<> struct ComponentSerializer<UITextComponent> {
        static constexpr const char* Key = "UITextComponent";
        static void Serialize(YAML::Emitter& o, const UITextComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Text" << YAML::Value << c.Text;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "FontSize" << YAML::Value << c.FontSize;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UITextComponent& c) {
            c.Text = n["Text"].as<std::string>();
            c.Color = n["Color"].as<glm::vec4>();
            c.FontSize = n["FontSize"].as<float>();
        }
    };

    template<> struct ComponentSerializer<UIButtonComponent> {
        static constexpr const char* Key = "UIButtonComponent";
        static void Serialize(YAML::Emitter& o, const UIButtonComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "NormalColor" << YAML::Value << c.NormalColor;
            o << YAML::Key << "HoverColor" << YAML::Value << c.HoverColor;
            o << YAML::Key << "PressedColor" << YAML::Value << c.PressedColor;
            o << YAML::Key << "OnClickFunction" << YAML::Value << c.OnClickFunction;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIButtonComponent& c) {
            c.NormalColor = n["NormalColor"].as<glm::vec4>();
            c.HoverColor = n["HoverColor"].as<glm::vec4>();
            c.PressedColor = n["PressedColor"].as<glm::vec4>();
            c.OnClickFunction = n["OnClickFunction"].as<std::string>();
        }
    };

    template<> struct ComponentSerializer<UIProgressBarComponent> {
        static constexpr const char* Key = "UIProgressBarComponent";
        static void Serialize(YAML::Emitter& o, const UIProgressBarComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Value" << YAML::Value << c.Value;
            o << YAML::Key << "MaxValue" << YAML::Value << c.MaxValue;
            o << YAML::Key << "ForegroundColor" << YAML::Value << c.ForegroundColor;
            o << YAML::Key << "BackgroundColor" << YAML::Value << c.BackgroundColor;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIProgressBarComponent& c) {
            c.Value = n["Value"].as<float>();
            c.MaxValue = n["MaxValue"].as<float>();
            c.ForegroundColor = n["ForegroundColor"].as<glm::vec4>();
            c.BackgroundColor = n["BackgroundColor"].as<glm::vec4>();
        }
    };

    // SpriteAnimatorComponent 逻辑复杂，单独特化，但接口完全一致
    template<> struct ComponentSerializer<SpriteAnimatorComponent> {
        static constexpr const char* Key = "SpriteAnimatorComponent";
        static void Serialize(YAML::Emitter& o, const SpriteAnimatorComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "DefaultClip" << YAML::Value << c.DefaultClipName;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
            for (const auto& [name, clip] : c.Clips)
            {
                o << YAML::BeginMap;
                o << YAML::Key << "Name" << YAML::Value << clip->GetName();
                o << YAML::Key << "Looping" << YAML::Value << clip->IsLooping();

                o << YAML::Key << "Frames" << YAML::Value << YAML::BeginSeq;
                for (const auto& f : clip->GetFrames()) {
                    o << YAML::BeginMap;
                    o << YAML::Key << "Texture" << YAML::Value << (f.Texture ? f.Texture->GetPath() : "");
                    o << YAML::Key << "UVMin" << YAML::Value << f.TexCoordMin;
                    o << YAML::Key << "UVMax" << YAML::Value << f.TexCoordMax;
                    o << YAML::Key << "Duration" << YAML::Value << f.Duration;
                    o << YAML::EndMap;
                }
                o << YAML::EndSeq;

                o << YAML::Key << "PropertyTracks" << YAML::Value << YAML::BeginSeq;
                for (const auto& tb : clip->GetPropertyTracks()) {
                    o << YAML::BeginMap;
                    o << YAML::Key << "Property" << YAML::Value << (int)tb->Property;
                    o << YAML::Key << "Keyframes" << YAML::Value << YAML::BeginSeq;
                    if (tb->Property == AnimatedProperty::SpriteColor) {
                        for (auto& kf : std::static_pointer_cast<PropertyTrack<glm::vec4>>(tb)->Keyframes)
                        {
                            o << YAML::BeginMap << YAML::Key << "Time" << kf.Time
                                << YAML::Key << "Value" << kf.Value
                                << YAML::Key << "Mode" << (int)kf.Mode
                                << YAML::EndMap;
                        }
                    }
                    else {
                        for (auto& kf : std::static_pointer_cast<PropertyTrack<float>>(tb)->Keyframes)
                        {
                            o << YAML::BeginMap << YAML::Key << "Time" << kf.Time
                                << YAML::Key << "Value" << kf.Value
                                << YAML::Key << "Mode" << (int)kf.Mode
                                << YAML::EndMap;
                        }
                    }
                    o << YAML::EndSeq << YAML::EndMap;
                }
                o << YAML::EndSeq << YAML::EndMap;
            }
            o << YAML::EndSeq << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SpriteAnimatorComponent& c) {
            if (auto clipsNode = n["Clips"]) {
                for (auto cn : clipsNode) {
                    auto clip = AnimationClip::Create(
                        cn["Name"].as<std::string>(), cn["Looping"].as<bool>(true));
                    if (auto fn = cn["Frames"])
                        for (auto f : fn) {
                            AnimationFrame frame;
                            if (auto p = f["Texture"].as<std::string>(""); !p.empty())
                                frame.Texture = Texture2D::Create(p);
                            frame.TexCoordMin = f["UVMin"].as<glm::vec2>(glm::vec2(0.0f));
                            frame.TexCoordMax = f["UVMax"].as<glm::vec2>(glm::vec2(1.0f));
                            frame.Duration = f["Duration"].as<float>(0.1f);
                            clip->AddFrame(frame);
                        }
                    if (auto tn = cn["PropertyTracks"])
                        for (auto t : tn) {
                            auto prop = (AnimatedProperty)t["Property"].as<int>();
                            if (prop == AnimatedProperty::SpriteColor) {
                                auto track = clip->AddVec4Track(prop);
                                for (auto kf : t["Keyframes"])
                                    track->AddKeyframe(kf["Time"].as<float>(),
                                        kf["Value"].as<glm::vec4>(),
                                        (InterpolationMode)kf["Mode"].as<int>(0));
                            }
                            else {
                                auto track = clip->AddFloatTrack(prop);
                                for (auto kf : t["Keyframes"])
                                    track->AddKeyframe(kf["Time"].as<float>(),
                                        kf["Value"].as<float>(),
                                        (InterpolationMode)kf["Mode"].as<int>(0));
                            }
                        }
                    c.AddClip(clip);
                }
            }
            c.DefaultClipName = n["DefaultClip"].as<std::string>("");
            c.PlayOnStart = n["PlayOnStart"].as<bool>(true);
        }
    };

    // ═══════════════════════════════════════════════════════════════════
    //  注册表：唯一需要维护的地方
    //  以后新增组件：1) 写特化  2) 在这里加一行类型
    // ═══════════════════════════════════════════════════════════════════

    // 注意：TagComponent 和 TransformComponent 是每个实体必有的，
    // 单独处理；其余组件走下面的通用循环。
    using SerializableComponents = ComponentGroup
    <
        CameraComponent,
        SpriteRendererComponent,
        SpriteAnimatorComponent,
        CircleRendererComponent,
        Rigidbody2DComponent,
        BoxCollider2DComponent,
        CircleCollider2DComponent,
        MeshRendererComponent,
        DirectionalLightComponent,
        PointLightComponent,
        ScriptComponent,
        AudioSourceComponent,
        UICanvasComponent,
        UIWidgetComponent,
        UIImageComponent,
        UITextComponent,
        UIButtonComponent,
        UIProgressBarComponent
    > ;

    // TransformComponent 在 ComponentGroup 里走 GetComponent 分支（已存在）

// 其余走 AddComponent 分支

    using AllDeserializable = ComponentGroup
    <
        TransformComponent,
        CameraComponent,
        SpriteRendererComponent,
        SpriteAnimatorComponent,
        CircleRendererComponent,
        Rigidbody2DComponent,
        BoxCollider2DComponent,
        CircleCollider2DComponent,
        MeshRendererComponent,
        DirectionalLightComponent,
        PointLightComponent,
        ScriptComponent,
        AudioSourceComponent,
        UICanvasComponent,
        UIWidgetComponent,
        UIImageComponent,
        UITextComponent,
        UIButtonComponent,
        UIProgressBarComponent
    > ;

    // ─── 通用序列化循环（fold expression 展开）──────────────────────────
    template<typename... Ts>
    static void SerializeComponents(ComponentGroup<Ts...>,
        YAML::Emitter& out, Entity entity)
    {
        ([&] {
            if (entity.HasComponent<Ts>()) {
                ComponentSerializer<Ts>::Serialize(out, entity.GetComponent<Ts>());
            }
            }(), ...);
    }

    // ─── 通用反序列化循环──────────────────────────────────────────────
    template<typename... Ts>
    static void DeserializeComponents(ComponentGroup<Ts...>,
        const YAML::Node& node, Entity entity)
    {
        ([&] {
            constexpr const char* key = ComponentSerializer<Ts>::Key;
            if (auto n = node[key]) {
                // TransformComponent 已存在（CreateEntity 时自动添加），特殊处理
                if constexpr (std::is_same_v<Ts, TransformComponent>) {
                    ComponentSerializer<Ts>::Deserialize(n, entity.GetComponent<Ts>());
                }
                else {
                    ComponentSerializer<Ts>::Deserialize(n, entity.AddComponent<Ts>());
                }
            }
            }(), ...);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  实体序列化 / 反序列化（主逻辑，极度精简）
    // ═══════════════════════════════════════════════════════════════════

    static void SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        HZ_CORE_ASSERT(entity.HasComponent<IDComponent>(), "Entity missing IDComponent");

        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

        // Tag 和 Transform 是必有组件，直接序列化
        ComponentSerializer<TagComponent>::Serialize(out, entity.GetComponent<TagComponent>());
        ComponentSerializer<TransformComponent>::Serialize(out, entity.GetComponent<TransformComponent>());

        // 其余组件：有就序列化，没有跳过
        SerializeComponents(SerializableComponents{}, out, entity);

        out << YAML::EndMap;
    }

    static Entity DeserializeEntityFromNode(const YAML::Node& node,
        Scene* scene, bool newUUID)
    {
        uint64_t uuid = newUUID ? (uint64_t)UUID() : node["Entity"].as<uint64_t>();
        std::string name;
        if (auto n = node["TagComponent"]) name = n["Tag"].as<std::string>();

        Entity entity = scene->CreateEntityWithUUID(uuid, name);

        
        DeserializeComponents(AllDeserializable{}, node, entity);

        return entity;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  SceneSerializer 公共接口（基本不用动）
    // ═══════════════════════════════════════════════════════════════════

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
        : m_Scene(scene) {
    }

    void SceneSerializer::SerializeYaml(const std::filesystem::path& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << "Untitled";
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
        m_Scene->m_Registry.each([&](auto id) {
            SerializeEntity(out, { id, m_Scene.get() });
            });
        out << YAML::EndSeq << YAML::EndMap;

        std::ofstream f(filepath);
        HZ_CORE_ASSERT(f.is_open(), "Failed to open file for serialization");
        f << out.c_str();
    }

    bool SceneSerializer::DeserializeYaml(const std::filesystem::path& filepath)
    {
        YAML::Node data;
        try { data = YAML::LoadFile(filepath.string()); }
        catch (const YAML::Exception& e) {
            HZ_CORE_ERROR("Failed to load '{}': {}", filepath.string(), e.what());
            return false;
        }
        if (!data["Scene"]) return false;
        if (auto entities = data["Entities"])
            for (auto n : entities)
                DeserializeEntityFromNode(n, m_Scene.get(), false);
        return true;
    }

    bool SceneSerializer::SerializePrefab(Entity entity,
        const std::filesystem::path& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Prefab" << YAML::Value << entity.GetName();
        out << YAML::Key << "Entity";
        SerializeEntity(out, entity);
        out << YAML::EndMap;

        std::ofstream f(filepath);
        if (!f.is_open()) {
            HZ_CORE_ERROR("PrefabSerializer: failed to open '{}'", filepath.string());
            return false;
        }
        f << out.c_str();
        return true;
    }

    Entity SceneSerializer::DeserializePrefab(const std::filesystem::path& filepath,
        Scene* scene)
    {
        YAML::Node data;
        try { data = YAML::LoadFile(filepath.string()); }
        catch (const YAML::Exception& e) {
            HZ_CORE_ERROR("PrefabSerializer: failed to load '{}': {}", filepath.string(), e.what());
            return {};
        }
        if (!data["Prefab"] || !data["Entity"]) {
            HZ_CORE_ERROR("PrefabSerializer: invalid file '{}'", filepath.string());
            return {};
        }
        return DeserializeEntityFromNode(data["Entity"], scene, true);
    }

} // namespace Hazel