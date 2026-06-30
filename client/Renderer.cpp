// =============================================================================
// client/Renderer.cpp — Deferred Shading Pipeline Implementation (AP-04)
// =============================================================================
#include "Renderer.h"
#include "../../core/Log.h"
#include <cstring>
#include <cmath>

namespace render {

// =============================================================================
// Camera Implementation
// =============================================================================
math::Mat4 Camera::GetViewMatrix() const {
    math::Vec3 f = (target - position).Normalized();
    math::Vec3 s = f.Cross(up).Normalized();
    math::Vec3 u = s.Cross(f);

    math::Mat4 view;
    view.m[0][0] = s.x; view.m[0][1] = s.y; view.m[0][2] = s.z;
    view.m[1][0] = u.x; view.m[1][1] = u.y; view.m[1][2] = u.z;
    view.m[2][0] = -f.x; view.m[2][1] = -f.y; view.m[2][2] = -f.z;
    view.m[0][3] = -s.Dot(position);
    view.m[1][3] = -u.Dot(position);
    view.m[2][3] = f.Dot(position);
    view.m[3][0] = 0.0f; view.m[3][1] = 0.0f; view.m[3][2] = 0.0f; view.m[3][3] = 1.0f;
    return view;
}

math::Mat4 Camera::GetProjectionMatrix() const {
    float tanHalfFov = std::tan(math::DEG2RAD * fov * 0.5f);
    math::Mat4 proj{};
    proj.m[0][0] = 1.0f / (aspectRatio * tanHalfFov);
    proj.m[1][1] = 1.0f / tanHalfFov;
    proj.m[2][2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    proj.m[2][3] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    proj.m[3][2] = -1.0f;
    return proj;
}

math::Mat4 Camera::GetViewProjectionMatrix() const {
    return GetProjectionMatrix() * GetViewMatrix();
}

math::Frustum Camera::GetFrustum() const {
    math::Mat4 vp = GetViewProjectionMatrix();
    math::Frustum f;
    // Extract frustum planes from view-projection matrix
    // Left
    f.planes[0] = math::Plane(
        vp.m[0][3] + vp.m[0][0],
        vp.m[1][3] + vp.m[1][0],
        vp.m[2][3] + vp.m[2][0],
        vp.m[3][3] + vp.m[3][0]
    );
    // Right
    f.planes[1] = math::Plane(
        vp.m[0][3] - vp.m[0][0],
        vp.m[1][3] - vp.m[1][0],
        vp.m[2][3] - vp.m[2][0],
        vp.m[3][3] - vp.m[3][0]
    );
    // Bottom
    f.planes[2] = math::Plane(
        vp.m[0][3] + vp.m[0][1],
        vp.m[1][3] + vp.m[1][1],
        vp.m[2][3] + vp.m[2][1],
        vp.m[3][3] + vp.m[3][1]
    );
    // Top
    f.planes[3] = math::Plane(
        vp.m[0][3] - vp.m[0][1],
        vp.m[1][3] - vp.m[1][1],
        vp.m[2][3] - vp.m[2][1],
        vp.m[3][3] - vp.m[3][1]
    );
    // Near
    f.planes[4] = math::Plane(
        vp.m[0][3] + vp.m[0][2],
        vp.m[1][3] + vp.m[1][2],
        vp.m[2][3] + vp.m[2][2],
        vp.m[3][3] + vp.m[3][2]
    );
    // Far
    f.planes[5] = math::Plane(
        vp.m[0][3] - vp.m[0][2],
        vp.m[1][3] - vp.m[1][2],
        vp.m[2][3] - vp.m[2][2],
        vp.m[3][3] - vp.m[3][2]
    );
    return f;
}

// =============================================================================
// GBuffer Implementation
// =============================================================================
bool GBuffer::Create(int w, int h) {
    width = w;
    height = h;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Position texture (RGB16F)
    glGenTextures(1, &positionTexture);
    glBindTexture(GL_TEXTURE_2D, positionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positionTexture, 0);

    // Normal texture (RGB16F)
    glGenTextures(1, &normalTexture);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTexture, 0);

    // Albedo texture (RGBA8)
    glGenTextures(1, &albedoTexture);
    glBindTexture(GL_TEXTURE_2D, albedoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, albedoTexture, 0);

    // Material texture (RGBA8: Roughness, Metallic, Emissive, ShadingModel)
    glGenTextures(1, &materialTexture);
    glBindTexture(GL_TEXTURE_2D, materialTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, materialTexture, 0);

    // Depth texture (D24S8)
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    // Set draw buffers
    GLuint attachments[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        AddLog("[Renderer] G-Buffer framebuffer incomplete!");
        Destroy();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    AddLog("[Renderer] G-Buffer created: {}x{}", w, h);
    return true;
}

void GBuffer::Destroy() {
    if (positionTexture) glDeleteTextures(1, &positionTexture);
    if (normalTexture) glDeleteTextures(1, &normalTexture);
    if (albedoTexture) glDeleteTextures(1, &albedoTexture);
    if (materialTexture) glDeleteTextures(1, &materialTexture);
    if (depthTexture) glDeleteTextures(1, &depthTexture);
    if (fbo) glDeleteFramebuffers(1, &fbo);
    positionTexture = normalTexture = albedoTexture = materialTexture = depthTexture = fbo = 0;
}

void GBuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

void GBuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::BindTextures(GLuint startUnit) const {
    glActiveTexture(GL_TEXTURE0 + startUnit);
    glBindTexture(GL_TEXTURE_2D, positionTexture);
    glActiveTexture(GL_TEXTURE0 + startUnit + 1);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glActiveTexture(GL_TEXTURE0 + startUnit + 2);
    glBindTexture(GL_TEXTURE_2D, albedoTexture);
    glActiveTexture(GL_TEXTURE0 + startUnit + 3);
    glBindTexture(GL_TEXTURE_2D, materialTexture);
    glActiveTexture(GL_TEXTURE0 + startUnit + 4);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
}

// =============================================================================
// Mesh Implementation
// =============================================================================
void Mesh::Destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
    indexCount = 0;
}

// =============================================================================
// Shader Implementation
// =============================================================================
bool ShaderProgram::Load(std::string_view vertexSource, std::string_view fragmentSource) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vs = vertexSource.data();
    glShaderSource(vertexShader, 1, &vs, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        AddLog("[Renderer] Vertex shader compilation failed: {}", infoLog);
        glDeleteShader(vertexShader);
        return false;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fs = fragmentSource.data();
    glShaderSource(fragmentShader, 1, &fs, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        AddLog("[Renderer] Fragment shader compilation failed: {}", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    programId = glCreateProgram();
    glAttachShader(programId, vertexShader);
    glAttachShader(programId, fragmentShader);
    glLinkProgram(programId);

    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(programId, 512, nullptr, infoLog);
        AddLog("[Renderer] Shader linking failed: {}", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(programId);
        programId = 0;
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

void ShaderProgram::Use() const {
    glUseProgram(programId);
}

void ShaderProgram::Destroy() {
    if (programId) glDeleteProgram(programId);
    programId = 0;
    uniformCache.clear();
}

GLint ShaderProgram::GetUniformLocation(std::string_view name) {
    auto it = uniformCache.find(std::string(name));
    if (it != uniformCache.end()) return it->second;

    GLint loc = glGetUniformLocation(programId, name.data());
    uniformCache[std::string(name)] = loc;
    return loc;
}

void ShaderProgram::SetUniform(std::string_view name, float value) {
    glUniform1f(GetUniformLocation(name), value);
}

void ShaderProgram::SetUniform(std::string_view name, const math::Vec3& value) {
    glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
}

void ShaderProgram::SetUniform(std::string_view name, const math::Mat4& value) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &value.m[0][0]);
}

void ShaderProgram::SetUniform(std::string_view name, int value) {
    glUniform1i(GetUniformLocation(name), value);
}

void ShaderProgram::SetUniform(std::string_view name, GLuint textureUnit) {
    glUniform1i(GetUniformLocation(name), static_cast<GLint>(textureUnit));
}

// =============================================================================
// Deferred Renderer Implementation
// =============================================================================

// Geometry Pass Vertex Shader
static constexpr std::string_view GEOMETRY_VS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// Geometry Pass Fragment Shader
static constexpr std::string_view GEOMETRY_FS = R"(
#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec4 gMaterial;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 uAlbedo;
uniform float uRoughness;
uniform float uMetallic;
uniform float uEmissive;
uniform float uAO;

void main() {
    gPosition = FragPos;
    gNormal = normalize(Normal);
    gAlbedo = vec4(uAlbedo, uAO);
    gMaterial = vec4(uRoughness, uMetallic, uEmissive, 1.0);
}
)";

// Lighting Pass Vertex Shader
static constexpr std::string_view LIGHTING_VS = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Lighting Pass Fragment Shader
static constexpr std::string_view LIGHTING_FS = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMaterial;
uniform sampler2D gDepth;

uniform vec3 uCameraPos;
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform int uLightCount;

struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    float range;
    int type; // 0 = directional, 1 = point, 2 = spot
    vec3 direction;
    float spotAngle;
    float spotSoftness;
};

uniform Light uLights[64];

const float PI = 3.14159265359;

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    return ggx1 * ggx2;
}

void main() {
    vec3 fragPos = texture(gPosition, TexCoord).rgb;
    vec3 normal = texture(gNormal, TexCoord).rgb;
    vec4 albedoAO = texture(gAlbedo, TexCoord);
    vec4 material = texture(gMaterial, TexCoord);

    vec3 albedo = albedoAO.rgb;
    float ao = albedoAO.a;
    float roughness = material.r;
    float metallic = material.g;
    float emissive = material.b;

    vec3 N = normalize(normal);
    vec3 V = normalize(uCameraPos - fragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < uLightCount; i++) {
        Light light = uLights[i];
        vec3 L;
        float attenuation = 1.0;
        float spotFactor = 1.0;

        if (light.type == 0) { // Directional
            L = normalize(-light.direction);
        } else { // Point or Spot
            L = normalize(light.position - fragPos);
            float distance = length(light.position - fragPos);
            attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
            attenuation *= 1.0 - smoothstep(light.range * 0.5, light.range, distance);

            if (light.type == 2) { // Spot
                float theta = dot(L, normalize(-light.direction));
                float inner = cos(radians(light.spotAngle * (1.0 - light.spotSoftness)));
                float outer = cos(radians(light.spotAngle));
                spotFactor = smoothstep(outer, inner, theta);
            }
        }

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * albedo / PI + specular) * light.color * light.intensity * NdotL * attenuation * spotFactor;
    }

    vec3 ambient = uAmbientColor * uAmbientIntensity * albedo * ao;
    vec3 color = ambient + Lo + (albedo * emissive);

    // Tone mapping (ACES approximation)
    color = color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14);
    color = pow(color, vec3(1.0 / 2.2)); // Gamma correction

    FragColor = vec4(color, 1.0);
}
)";

// Post-Process Vertex Shader
static constexpr std::string_view POST_VS = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Post-Process Fragment Shader (FXAA + Vignette)
static constexpr std::string_view POST_FS = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uScreenTexture;
uniform vec2 uScreenSize;
uniform float uVignetteIntensity;

#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_REDUCE_MIN (1.0 / 128.0)

vec3 Fxaa(vec3 rgbNW, vec3 rgbNE, vec3 rgbSW, vec3 rgbSE, vec3 rgbM) {
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir = vec2(
        -((lumaNW + lumaNE) - (lumaSW + lumaSE)),
        ((lumaNW + lumaSW) - (lumaNE + lumaSE))
    );

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir * rcpDirMin)) / uScreenSize;

    vec3 rgbA = (1.0 / 2.0) * (
        texture(uScreenTexture, TexCoord + dir * (1.0 / 3.0 - 0.5)).xyz +
        texture(uScreenTexture, TexCoord + dir * (2.0 / 3.0 - 0.5)).xyz);

    vec3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) * (
        texture(uScreenTexture, TexCoord + dir * (0.0 / 3.0 - 0.5)).xyz +
        texture(uScreenTexture, TexCoord + dir * (3.0 / 3.0 - 0.5)).xyz);

    float lumaB = dot(rgbB, luma);
    return ((lumaB < lumaMin) || (lumaB > lumaMax)) ? rgbA : rgbB;
}

void main() {
    vec2 texel = 1.0 / uScreenSize;
    vec3 rgbNW = texture(uScreenTexture, TexCoord + vec2(-1.0, -1.0) * texel).rgb;
    vec3 rgbNE = texture(uScreenTexture, TexCoord + vec2(1.0, -1.0) * texel).rgb;
    vec3 rgbSW = texture(uScreenTexture, TexCoord + vec2(-1.0, 1.0) * texel).rgb;
    vec3 rgbSE = texture(uScreenTexture, TexCoord + vec2(1.0, 1.0) * texel).rgb;
    vec3 rgbM = texture(uScreenTexture, TexCoord).rgb;

    vec3 color = Fxaa(rgbNW, rgbNE, rgbSW, rgbSE, rgbM);

    // Vignette
    vec2 uv = TexCoord * 2.0 - 1.0;
    float vignette = 1.0 - dot(uv, uv) * uVignetteIntensity;
    color *= clamp(vignette, 0.0, 1.0);

    FragColor = vec4(color, 1.0);
}
)";

// =============================================================================
// DeferredRenderer Implementation
// =============================================================================
bool DeferredRenderer::Initialize(GLFWwindow* win) {
    window = win;
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

    // Create G-Buffer
    if (!gBuffer.Create(windowWidth, windowHeight)) {
        return false;
    }

    // Create shaders
    if (!CreateShaders()) {
        return false;
    }

    // Create fullscreen quad
    if (!CreateFullscreenQuad()) {
        return false;
    }

    // Create post-process FBO
    glGenFramebuffers(1, &postProcessFbo);
    glGenTextures(1, &postProcessTexture);
    glBindTexture(GL_TEXTURE_2D, postProcessTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, windowWidth, windowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, postProcessFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, postProcessTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    AddLog("[Renderer] Deferred renderer initialized: {}x{}", windowWidth, windowHeight);
    return true;
}

void DeferredRenderer::Shutdown() {
    gBuffer.Destroy();
    geometryShader.Destroy();
    lightingShader.Destroy();
    postProcessShader.Destroy();
    shadowShader.Destroy();

    if (fullscreenVao) glDeleteVertexArrays(1, &fullscreenVao);
    if (fullscreenVbo) glDeleteBuffers(1, &fullscreenVbo);

    if (postProcessFbo) glDeleteFramebuffers(1, &postProcessFbo);
    if (postProcessTexture) glDeleteTextures(1, &postProcessTexture);

    AddLog("[Renderer] Deferred renderer shutdown");
}

void DeferredRenderer::Resize(int width, int height) {
    windowWidth = width;
    windowHeight = height;
    UpdateGBufferSize();

    // Resize post-process texture
    glBindTexture(GL_TEXTURE_2D, postProcessTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void DeferredRenderer::UpdateGBufferSize() {
    gBuffer.Destroy();
    gBuffer.Create(windowWidth, windowHeight);
}

bool DeferredRenderer::CreateShaders() {
    if (!geometryShader.Load(GEOMETRY_VS, GEOMETRY_FS)) return false;
    if (!lightingShader.Load(LIGHTING_VS, LIGHTING_FS)) return false;
    if (!postProcessShader.Load(POST_VS, POST_FS)) return false;
    return true;
}

bool DeferredRenderer::CreateFullscreenQuad() {
    float quadVertices[] = {
        // Positions    // TexCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &fullscreenVao);
    glGenBuffers(1, &fullscreenVbo);
    glBindVertexArray(fullscreenVao);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

// =============================================================================
// Frame Rendering
// =============================================================================
void DeferredRenderer::BeginFrame() {
    drawCalls = 0;
    triangleCount = 0;
}

void DeferredRenderer::RenderGeometry(ecs::EcsWorld& world, const Camera& camera) {
    // Geometry Pass: Write to G-Buffer
    gBuffer.Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    geometryShader.Use();
    geometryShader.SetUniform("view", camera.GetViewMatrix());
    geometryShader.SetUniform("projection", camera.GetProjectionMatrix());

    auto query = world.Query<game::Transform>();
    for (auto [handle] : query) {
        auto* transform = world.GetComponent<game::Transform>(handle);
        auto* render = world.GetComponent<game::RenderInfo>(handle);
        if (!transform) continue;

        // Build model matrix
        math::Mat4 model = math::Mat4::Identity();
        model = model * math::Mat4::Translation(transform->x, transform->y, transform->z);
        model = model * math::Mat4::RotationY(transform->rotationY);

        geometryShader.SetUniform("model", model);

        // Set material
        Material mat{};
        if (render) {
            mat.albedo = math::Vec3(1.0f, 1.0f, 1.0f); // Would load from material system
            mat.roughness = 0.5f;
            mat.metallic = 0.0f;
        }

        geometryShader.SetUniform("uAlbedo", mat.albedo);
        geometryShader.SetUniform("uRoughness", mat.roughness);
        geometryShader.SetUniform("uMetallic", mat.metallic);
        geometryShader.SetUniform("uEmissive", mat.emissive);
        geometryShader.SetUniform("uAO", mat.ao);

        // TODO: Bind mesh VAO and draw
        // For now, just count as a draw call
        drawCalls++;
        triangleCount += 12; // Cube = 12 triangles
    }

    gBuffer.Unbind();
}

void DeferredRenderer::RenderLighting(const Camera& camera) {
    // Lighting Pass: Read G-Buffer, compute lighting
    glBindFramebuffer(GL_FRAMEBUFFER, postProcessFbo);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    lightingShader.Use();

    // Bind G-Buffer textures
    gBuffer.BindTextures(0);
    lightingShader.SetUniform("gPosition", 0);
    lightingShader.SetUniform("gNormal", 1);
    lightingShader.SetUniform("gAlbedo", 2);
    lightingShader.SetUniform("gMaterial", 3);
    lightingShader.SetUniform("gDepth", 4);

    // Camera
    lightingShader.SetUniform("uCameraPos", camera.position);

    // Ambient
    lightingShader.SetUniform("uAmbientColor", math::Vec3(0.1f, 0.1f, 0.15f));
    lightingShader.SetUniform("uAmbientIntensity", 0.3f);

    // Lights
    BindLightUniforms(lightingShader);

    // Draw fullscreen quad
    glBindVertexArray(fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void DeferredRenderer::RenderPostProcess() {
    // Post-Process: FXAA + Vignette
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    postProcessShader.Use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, postProcessTexture);
    postProcessShader.SetUniform("uScreenTexture", 0);
    postProcessShader.SetUniform("uScreenSize", math::Vec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight)));
    postProcessShader.SetUniform("uVignetteIntensity", 0.3f);

    glBindVertexArray(fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void DeferredRenderer::EndFrame() {
    glfwSwapBuffers(window);
}

// =============================================================================
// Lighting
// =============================================================================
void DeferredRenderer::AddLight(const Light& light) {
    if (lights.size() < MAX_LIGHTS) {
        lights.push_back(light);
    }
}

void DeferredRenderer::ClearLights() {
    lights.clear();
}

void DeferredRenderer::SetAmbientLight(const math::Vec3& color, float intensity) {
    // Set in shader during lighting pass
    (void)color;
    (void)intensity;
}

void DeferredRenderer::BindLightUniforms(const ShaderProgram& shader) {
    int count = static_cast<int>(std::min(lights.size(), MAX_LIGHTS));
    shader.SetUniform("uLightCount", count);

    for (int i = 0; i < count; ++i) {
        const auto& light = lights[i];
        std::string base = std::format("uLights[{}].", i);
        shader.SetUniform(base + "position", light.position);
        shader.SetUniform(base + "color", light.color);
        shader.SetUniform(base + "intensity", light.intensity);
        shader.SetUniform(base + "range", light.range);
        shader.SetUniform(base + "type", static_cast<int>(light.type));
        shader.SetUniform(base + "direction", light.direction);
        shader.SetUniform(base + "spotAngle", light.spotAngle);
        shader.SetUniform(base + "spotSoftness", light.spotSoftness);
    }
}

// =============================================================================
// Mesh Helpers
// =============================================================================
Mesh DeferredRenderer::CreateCubeMesh() {
    Mesh mesh{};
    // TODO: Implement cube geometry
    return mesh;
}

Mesh DeferredRenderer::CreateSphereMesh(int segments, int rings) {
    Mesh mesh{};
    // TODO: Implement sphere geometry
    (void)segments;
    (void)rings;
    return mesh;
}

Mesh DeferredRenderer::CreatePlaneMesh(float size) {
    Mesh mesh{};
    // TODO: Implement plane geometry
    (void)size;
    return mesh;
}

void DeferredRenderer::DrawMesh(const Mesh& mesh, const math::Mat4& transform, const Material& material) {
    (void)mesh;
    (void)transform;
    (void)material;
    // TODO: Implement mesh drawing
}

} // namespace render
