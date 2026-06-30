// =============================================================================
// client/Renderer.h — Deferred Shading Pipeline (AP-04)
// =============================================================================
// KORREKTUR: Deferred Shading Pipeline mit G-Buffer implementiert.
// 1. Geometry Pass → G-Buffer (Position, Normal, Albedo, Material)
// 2. Lighting Pass → Deferred Lighting (mehrere Lichtquellen)
// 3. Post-Processing → Tonemapping, FXAA
// =============================================================================
#pragma once
#include "../../math/Vector.h"
#include "../../math/Matrix.h"
#include "../../ecs/ecs_EcsWorld.h"
#include "../../ecs/Components.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <span>

namespace render {

// =============================================================================
// Shader Program
// =============================================================================
class ShaderProgram {
    GLuint programId = 0;
    std::unordered_map<std::string, GLint> uniformCache;

public:
    ShaderProgram() = default;
    ~ShaderProgram() { Destroy(); }

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    [[nodiscard]] bool Load(std::string_view vertexSource, std::string_view fragmentSource);
    [[nodiscard]] bool LoadFromFiles(std::string_view vertexPath, std::string_view fragmentPath);
    void Use() const;
    void Destroy();

    [[nodiscard]] GLint GetUniformLocation(std::string_view name);
    void SetUniform(std::string_view name, float value);
    void SetUniform(std::string_view name, const math::Vec3& value);
    void SetUniform(std::string_view name, const math::Mat4& value);
    void SetUniform(std::string_view name, int value);
    void SetUniform(std::string_view name, GLuint textureUnit);

    [[nodiscard]] GLuint GetId() const { return programId; }
};

// =============================================================================
// G-Buffer für Deferred Shading
// =============================================================================
struct GBuffer {
    GLuint fbo = 0;
    GLuint positionTexture = 0;    // RGB16F (World Position)
    GLuint normalTexture = 0;      // RGB16F (World Normal)
    GLuint albedoTexture = 0;      // RGBA8 (Albedo + AO)
    GLuint materialTexture = 0;    // RGBA8 (Roughness, Metallic, Emissive, ShadingModel)
    GLuint depthTexture = 0;       // D24S8 (Depth + Stencil)

    int width = 0, height = 0;

    [[nodiscard]] bool Create(int w, int h);
    void Destroy();
    void Bind() const;
    void Unbind() const;
    void BindTextures(GLuint startUnit = 0) const;
};

// =============================================================================
// Light Source
// =============================================================================
struct Light {
    enum class Type : uint8_t {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    Type type = Type::Directional;
    math::Vec3 position{0.0f, 10.0f, 0.0f};
    math::Vec3 direction{0.0f, -1.0f, 0.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 100.0f;
    float spotAngle = 45.0f;  // Degrees, for spot lights
    float spotSoftness = 0.1f;

    // Shadows
    bool castShadows = false;
    GLuint shadowMap = 0;
    math::Mat4 shadowMatrix;
};

// =============================================================================
// Mesh
// =============================================================================
struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    uint32_t indexCount = 0;

    void Destroy();
};

// =============================================================================
// Material
// =============================================================================
struct Material {
    math::Vec3 albedo{1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emissive = 0.0f;
    float ao = 1.0f;

    GLuint albedoTexture = 0;
    GLuint normalTexture = 0;
    GLuint roughnessTexture = 0;
    GLuint metallicTexture = 0;

    bool useAlbedoTexture = false;
    bool useNormalTexture = false;
    bool useRoughnessTexture = false;
    bool useMetallicTexture = false;
};

// =============================================================================
// Camera
// =============================================================================
struct Camera {
    math::Vec3 position{0.0f, 5.0f, 10.0f};
    math::Vec3 target{0.0f, 0.0f, 0.0f};
    math::Vec3 up{0.0f, 1.0f, 0.0f};

    float fov = 60.0f;        // Degrees
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float aspectRatio = 16.0f / 9.0f;

    [[nodiscard]] math::Mat4 GetViewMatrix() const;
    [[nodiscard]] math::Mat4 GetProjectionMatrix() const;
    [[nodiscard]] math::Mat4 GetViewProjectionMatrix() const;
    [[nodiscard]] math::Frustum GetFrustum() const;
};

// =============================================================================
// Deferred Renderer
// =============================================================================
class DeferredRenderer {
    GLFWwindow* window = nullptr;
    int windowWidth = 1920;
    int windowHeight = 1080;

    // G-Buffer
    GBuffer gBuffer;

    // Shaders
    ShaderProgram geometryShader;
    ShaderProgram lightingShader;
    ShaderProgram postProcessShader;
    ShaderProgram shadowShader;

    // Fullscreen Quad (for lighting pass)
    GLuint fullscreenVao = 0;
    GLuint fullscreenVbo = 0;

    // Lights
    std::vector<Light> lights;
    static constexpr size_t MAX_LIGHTS = 64;

    // Post-processing
    GLuint postProcessFbo = 0;
    GLuint postProcessTexture = 0;

    // Stats
    uint32_t drawCalls = 0;
    uint32_t triangleCount = 0;

public:
    DeferredRenderer() = default;
    ~DeferredRenderer() { Shutdown(); }

    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Initialize(GLFWwindow* win);
    void Shutdown();
    void Resize(int width, int height);

    // ===================================================================
    // Frame Rendering
    // ===================================================================
    void BeginFrame();
    void RenderGeometry(ecs::EcsWorld& world, const Camera& camera);
    void RenderLighting(const Camera& camera);
    void RenderPostProcess();
    void EndFrame();

    // ===================================================================
    // Lighting
    // ===================================================================
    void AddLight(const Light& light);
    void ClearLights();
    void SetAmbientLight(const math::Vec3& color, float intensity);

    // ===================================================================
    // Mesh/Material Helpers
    // ===================================================================
    [[nodiscard]] Mesh CreateCubeMesh();
    [[nodiscard]] Mesh CreateSphereMesh(int segments = 32, int rings = 16);
    [[nodiscard]] Mesh CreatePlaneMesh(float size = 100.0f);
    void DrawMesh(const Mesh& mesh, const math::Mat4& transform, const Material& material);

    // ===================================================================
    // Stats
    // ===================================================================
    [[nodiscard]] uint32_t GetDrawCalls() const { return drawCalls; }
    [[nodiscard]] uint32_t GetTriangleCount() const { return triangleCount; }

private:
    [[nodiscard]] bool CreateShaders();
    [[nodiscard]] bool CreateFullscreenQuad();
    void UpdateGBufferSize();
    void BindLightUniforms(const ShaderProgram& shader);
};

} // namespace render
