// =============================================================================
// client/Animation.h — Skeletal Animation System (AP-04)
// =============================================================================
// KORREKTUR: Skeletal Animation mit Skinning, Blend Trees und Animation States.
// Unterstützt FBX/GLTF-Import, IK (Inverse Kinematics) und Animation Blending.
// Integriert mit Deferred Renderer.
// =============================================================================
#pragma once
#include "../../math/Vector.h"
#include "../../math/Matrix.h"
#include "../../math/Quaternion.h"

#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <span>
#include <functional>
#include <queue>

namespace render {

// =============================================================================
// Bone / Joint
// =============================================================================
struct Bone {
    std::string name;
    int parentIndex = -1;  // -1 = root
    math::Vec3 localPosition{0.0f, 0.0f, 0.0f};
    math::Quaternion localRotation{0.0f, 0.0f, 0.0f, 1.0f};
    math::Vec3 localScale{1.0f, 1.0f, 1.0f};

    math::Mat4 inverseBindPose;  // Inverse bind pose matrix
    math::Mat4 localMatrix;      // Current local transform
    math::Mat4 worldMatrix;      // Current world transform
};

// =============================================================================
// Keyframe
// =============================================================================
struct Keyframe {
    float time = 0.0f;
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

// =============================================================================
// Animation Track (für einen Bone)
// =============================================================================
struct AnimationTrack {
    int boneIndex = -1;
    std::vector<Keyframe> positionKeys;
    std::vector<Keyframe> rotationKeys;
    std::vector<Keyframe> scaleKeys;

    [[nodiscard]] math::Vec3 SamplePosition(float time) const;
    [[nodiscard]] math::Quaternion SampleRotation(float time) const;
    [[nodiscard]] math::Vec3 SampleScale(float time) const;
};

// =============================================================================
// Animation Clip
// =============================================================================
struct AnimationClip {
    std::string name;
    float duration = 0.0f;      // In seconds
    float ticksPerSecond = 30.0f;
    bool loop = true;
    std::vector<AnimationTrack> tracks;

    [[nodiscard]] float GetDuration() const { return duration / ticksPerSecond; }
};

// =============================================================================
// Animation State (für State Machine)
// =============================================================================
enum class AnimationState : uint8_t {
    Idle,
    Walk,
    Run,
    Attack,
    Hit,
    Death,
    Jump,
    Fall,
    Land,
    Custom
};

// =============================================================================
// Animation Transition
// =============================================================================
struct AnimationTransition {
    AnimationState from;
    AnimationState to;
    float blendDuration = 0.2f;   // Crossfade duration
    bool hasExitTime = false;
    float exitTime = 0.0f;        // Normalized exit time (0-1)
    std::function<bool()> condition; // Transition condition
};

// =============================================================================
// Animator — Animation State Machine + Blending
// =============================================================================
class Animator {
    std::vector<Bone> skeleton;
    std::unordered_map<std::string, AnimationClip> clips;
    std::unordered_map<AnimationState, std::string> stateToClip;
    std::vector<AnimationTransition> transitions;

    // Current state
    AnimationState currentState = AnimationState::Idle;
    AnimationState nextState = AnimationState::Idle;
    float stateTime = 0.0f;
    float blendTime = 0.0f;
    float blendDuration = 0.0f;
    bool isBlending = false;

    // Pose
    std::vector<math::Mat4> poseMatrices;     // Local pose
    std::vector<math::Mat4> skinningMatrices; // Final skinning matrices

    // IK
    struct IKTarget {
        int boneIndex = -1;
        math::Vec3 targetPosition{0.0f, 0.0f, 0.0f};
        float weight = 1.0f;
        int iterations = 10;
    };
    std::vector<IKTarget> ikTargets;

public:
    Animator() = default;
    ~Animator() = default;

    // ===================================================================
    // Skeleton
    // ===================================================================
    void SetSkeleton(std::vector<Bone> bones);
    [[nodiscard]] const std::vector<Bone>& GetSkeleton() const { return skeleton; }
    [[nodiscard]] int FindBoneIndex(std::string_view name) const;

    // ===================================================================
    // Clips
    // ===================================================================
    void AddClip(const AnimationClip& clip);
    [[nodiscard]] AnimationClip* GetClip(std::string_view name);
    void MapStateToClip(AnimationState state, std::string_view clipName);

    // ===================================================================
    // State Machine
    // ===================================================================
    void AddTransition(const AnimationTransition& transition);
    void SetState(AnimationState state);
    [[nodiscard]] AnimationState GetCurrentState() const { return currentState; }
    [[nodiscard]] bool IsBlending() const { return isBlending; }

    // ===================================================================
    // Update
    // ===================================================================
    void Update(float deltaTime);
    void UpdateSkeleton();

    // ===================================================================
    // Sampling
    // ===================================================================
    void SampleClip(const AnimationClip& clip, float time, std::span<math::Mat4> outMatrices);
    [[nodiscard]] math::Mat4 InterpolatePose(const math::Mat4& a, const math::Mat4& b, float t);

    // ===================================================================
    // Skinning
    // ===================================================================
    [[nodiscard]] std::span<const math::Mat4> GetSkinningMatrices() const { return skinningMatrices; }
    void UpdateSkinningMatrices();

    // ===================================================================
    // IK
    // ===================================================================
    void AddIKTarget(int boneIndex, const math::Vec3& target, float weight = 1.0f);
    void ClearIKTargets();
    void SolveIK();

    // ===================================================================
    // Root Motion
    // ===================================================================
    [[nodiscard]] math::Vec3 ExtractRootMotion(float deltaTime) const;
    [[nodiscard]] math::Quaternion ExtractRootRotation(float deltaTime) const;

    // ===================================================================
    // Events
    // ===================================================================
    using AnimationEventCallback = std::function<void(std::string_view)>;
    void AddEvent(std::string_view clipName, float normalizedTime, std::string_view eventName);
    void SetEventCallback(AnimationEventCallback cb);

private:
    void EvaluateTransitions();
    void ApplyIK(int boneIndex, const math::Vec3& target, int iterations);
    AnimationEventCallback eventCallback;
    struct TimedEvent { float time; std::string name; bool fired = false; };
    std::unordered_map<std::string, std::vector<TimedEvent>> clipEvents;
};

// =============================================================================
// Animation Loader (FBX/GLTF)
// =============================================================================
class AnimationLoader {
public:
    [[nodiscard]] static std::unique_ptr<Animator> LoadFromFile(std::string_view path);
    [[nodiscard]] static std::vector<Bone> LoadSkeleton(std::string_view path);
    [[nodiscard]] static std::vector<AnimationClip> LoadClips(std::string_view path);
};

} // namespace render
