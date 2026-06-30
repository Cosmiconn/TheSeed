// =============================================================================
// client/Animation.cpp — Skeletal Animation Implementation (AP-04)
// =============================================================================
#include "Animation.h"
#include "../../core/Log.h"
#include <algorithm>

namespace render {

// =============================================================================
// AnimationTrack Implementation
// =============================================================================
math::Vec3 AnimationTrack::SamplePosition(float time) const {
    if (positionKeys.empty()) return math::Vec3{0.0f, 0.0f, 0.0f};
    if (positionKeys.size() == 1) return positionKeys[0].position;

    // Find keyframes
    size_t idx = 0;
    for (size_t i = 0; i < positionKeys.size() - 1; ++i) {
        if (time >= positionKeys[i].time && time <= positionKeys[i + 1].time) {
            idx = i;
            break;
        }
    }

    if (idx >= positionKeys.size() - 1) return positionKeys.back().position;

    const auto& k1 = positionKeys[idx];
    const auto& k2 = positionKeys[idx + 1];

    float t = (time - k1.time) / (k2.time - k1.time);
    return k1.position.Lerp(k2.position, t);
}

math::Quaternion AnimationTrack::SampleRotation(float time) const {
    if (rotationKeys.empty()) return math::Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
    if (rotationKeys.size() == 1) return rotationKeys[0].rotation;

    size_t idx = 0;
    for (size_t i = 0; i < rotationKeys.size() - 1; ++i) {
        if (time >= rotationKeys[i].time && time <= rotationKeys[i + 1].time) {
            idx = i;
            break;
        }
    }

    if (idx >= rotationKeys.size() - 1) return rotationKeys.back().rotation;

    const auto& k1 = rotationKeys[idx];
    const auto& k2 = rotationKeys[idx + 1];

    float t = (time - k1.time) / (k2.time - k1.time);
    return k1.rotation.Slerp(k2.rotation, t);
}

math::Vec3 AnimationTrack::SampleScale(float time) const {
    if (scaleKeys.empty()) return math::Vec3{1.0f, 1.0f, 1.0f};
    if (scaleKeys.size() == 1) return scaleKeys[0].scale;

    size_t idx = 0;
    for (size_t i = 0; i < scaleKeys.size() - 1; ++i) {
        if (time >= scaleKeys[i].time && time <= scaleKeys[i + 1].time) {
            idx = i;
            break;
        }
    }

    if (idx >= scaleKeys.size() - 1) return scaleKeys.back().scale;

    const auto& k1 = scaleKeys[idx];
    const auto& k2 = scaleKeys[idx + 1];

    float t = (time - k1.time) / (k2.time - k1.time);
    return k1.scale.Lerp(k2.scale, t);
}

// =============================================================================
// Animator Implementation
// =============================================================================
void Animator::SetSkeleton(std::vector<Bone> bones) {
    skeleton = std::move(bones);
    poseMatrices.resize(skeleton.size(), math::Mat4::Identity());
    skinningMatrices.resize(skeleton.size(), math::Mat4::Identity());

    // Calculate inverse bind poses
    for (size_t i = 0; i < skeleton.size(); ++i) {
        UpdateSkeletonHierarchy(static_cast<int>(i));
    }

    for (auto& bone : skeleton) {
        bone.inverseBindPose = bone.worldMatrix.Inverse();
    }
}

int Animator::FindBoneIndex(std::string_view name) const {
    for (size_t i = 0; i < skeleton.size(); ++i) {
        if (skeleton[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

void Animator::AddClip(const AnimationClip& clip) {
    clips[clip.name] = clip;
}

AnimationClip* Animator::GetClip(std::string_view name) {
    auto it = clips.find(std::string(name));
    return it != clips.end() ? &it->second : nullptr;
}

void Animator::MapStateToClip(AnimationState state, std::string_view clipName) {
    stateToClip[state] = std::string(clipName);
}

void Animator::AddTransition(const AnimationTransition& transition) {
    transitions.push_back(transition);
}

void Animator::SetState(AnimationState state) {
    if (currentState == state) return;

    nextState = state;
    isBlending = true;
    blendTime = 0.0f;

    // Find transition
    for (const auto& trans : transitions) {
        if (trans.from == currentState && trans.to == state) {
            blendDuration = trans.blendDuration;
            return;
        }
    }

    // Default blend duration
    blendDuration = 0.2f;
}

void Animator::Update(float deltaTime) {
    stateTime += deltaTime;

    // Evaluate transitions
    EvaluateTransitions();

    // Update blending
    if (isBlending) {
        blendTime += deltaTime;
        if (blendTime >= blendDuration) {
            isBlending = false;
            currentState = nextState;
            stateTime = 0.0f;
            blendTime = 0.0f;
        }
    }

    // Sample current animation
    auto currentClipName = stateToClip.find(currentState);
    if (currentClipName == stateToClip.end()) return;

    AnimationClip* currentClip = GetClip(currentClipName->second);
    if (!currentClip) return;

    float currentTime = stateTime * currentClip->ticksPerSecond;
    if (currentClip->loop) {
        currentTime = std::fmod(currentTime, currentClip->duration);
    } else {
        currentTime = std::min(currentTime, currentClip->duration);
    }

    // Sample pose
    SampleClip(*currentClip, currentTime, poseMatrices);

    // Blend with next state if transitioning
    if (isBlending) {
        auto nextClipName = stateToClip.find(nextState);
        if (nextClipName != stateToClip.end()) {
            AnimationClip* nextClip = GetClip(nextClipName->second);
            if (nextClip) {
                float nextTime = std::fmod(stateTime * nextClip->ticksPerSecond, nextClip->duration);

                std::vector<math::Mat4> nextPose(skeleton.size(), math::Mat4::Identity());
                SampleClip(*nextClip, nextTime, nextPose);

                float blendFactor = blendTime / blendDuration;
                for (size_t i = 0; i < poseMatrices.size(); ++i) {
                    poseMatrices[i] = InterpolatePose(poseMatrices[i], nextPose[i], blendFactor);
                }
            }
        }
    }

    // Apply IK
    if (!ikTargets.empty()) {
        SolveIK();
    }

    // Update skeleton hierarchy
    for (size_t i = 0; i < skeleton.size(); ++i) {
        UpdateSkeletonHierarchy(static_cast<int>(i));
    }

    // Update skinning matrices
    UpdateSkinningMatrices();

    // Fire events
    if (eventCallback) {
        auto it = clipEvents.find(currentClipName->second);
        if (it != clipEvents.end()) {
            float normalizedTime = currentTime / currentClip->duration;
            for (auto& event : it->second) {
                if (!event.fired && normalizedTime >= event.time) {
                    event.fired = true;
                    eventCallback(event.name);
                }
            }
        }
    }
}

void Animator::SampleClip(const AnimationClip& clip, float time, std::span<math::Mat4> outMatrices) {
    if (outMatrices.size() < skeleton.size()) return;

    // Initialize with bind pose
    for (size_t i = 0; i < skeleton.size(); ++i) {
        outMatrices[i] = skeleton[i].localMatrix;
    }

    // Apply animation tracks
    for (const auto& track : clip.tracks) {
        if (track.boneIndex < 0 || track.boneIndex >= static_cast<int>(skeleton.size())) continue;

        math::Vec3 pos = track.SamplePosition(time);
        math::Quaternion rot = track.SampleRotation(time);
        math::Vec3 scale = track.SampleScale(time);

        math::Mat4 translation = math::Mat4::Translation(pos.x, pos.y, pos.z);
        math::Mat4 rotation = rot.ToMatrix();
        math::Mat4 scaling = math::Mat4::Scaling(scale.x, scale.y, scale.z);

        outMatrices[track.boneIndex] = translation * rotation * scaling;
    }
}

math::Mat4 Animator::InterpolatePose(const math::Mat4& a, const math::Mat4& b, float t) {
    // Decompose matrices
    math::Vec3 posA, scaleA, posB, scaleB;
    math::Quaternion rotA, rotB;

    a.Decompose(posA, rotA, scaleA);
    b.Decompose(posB, rotB, scaleB);

    math::Vec3 pos = posA.Lerp(posB, t);
    math::Quaternion rot = rotA.Slerp(rotB, t);
    math::Vec3 scale = scaleA.Lerp(scaleB, t);

    return math::Mat4::Translation(pos.x, pos.y, pos.z) *
           rot.ToMatrix() *
           math::Mat4::Scaling(scale.x, scale.y, scale.z);
}

void Animator::UpdateSkeletonHierarchy(int boneIndex) {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(skeleton.size())) return;

    auto& bone = skeleton[boneIndex];

    if (bone.parentIndex >= 0) {
        bone.worldMatrix = skeleton[bone.parentIndex].worldMatrix * poseMatrices[boneIndex];
    } else {
        bone.worldMatrix = poseMatrices[boneIndex];
    }
}

void Animator::UpdateSkinningMatrices() {
    for (size_t i = 0; i < skeleton.size(); ++i) {
        skinningMatrices[i] = skeleton[i].worldMatrix * skeleton[i].inverseBindPose;
    }
}

// =============================================================================
// IK Implementation
// =============================================================================
void Animator::AddIKTarget(int boneIndex, const math::Vec3& target, float weight) {
    ikTargets.push_back(IKTarget{boneIndex, target, weight, 10});
}

void Animator::ClearIKTargets() {
    ikTargets.clear();
}

void Animator::SolveIK() {
    for (const auto& ik : ikTargets) {
        if (ik.boneIndex < 0 || ik.boneIndex >= static_cast<int>(skeleton.size())) continue;
        ApplyIK(ik.boneIndex, ik.targetPosition, ik.iterations);
    }
}

void Animator::ApplyIK(int boneIndex, const math::Vec3& target, int iterations) {
    // Simple CCD (Cyclic Coordinate Descent) IK
    for (int iter = 0; iter < iterations; ++iter) {
        int currentBone = boneIndex;

        while (currentBone >= 0) {
            math::Vec3 bonePos = skeleton[currentBone].worldMatrix.GetTranslation();
            math::Vec3 endEffector = skeleton[boneIndex].worldMatrix.GetTranslation();

            math::Vec3 toEnd = endEffector - bonePos;
            math::Vec3 toTarget = target - bonePos;

            if (toEnd.LengthSq() < 0.0001f || toTarget.LengthSq() < 0.0001f) break;

            toEnd.Normalize();
            toTarget.Normalize();

            float dot = toEnd.Dot(toTarget);
            dot = std::clamp(dot, -1.0f, 1.0f);

            float angle = std::acos(dot);
            if (angle < 0.001f) break;

            math::Vec3 axis = toEnd.Cross(toTarget);
            if (axis.LengthSq() < 0.0001f) break;
            axis.Normalize();

            math::Quaternion rot(axis, angle);
            skeleton[currentBone].localRotation = rot * skeleton[currentBone].localRotation;
            skeleton[currentBone].localRotation.Normalize();

            // Update hierarchy from this bone
            UpdateSkeletonHierarchy(currentBone);
            for (size_t i = currentBone + 1; i < skeleton.size(); ++i) {
                if (skeleton[i].parentIndex >= currentBone) {
                    UpdateSkeletonHierarchy(static_cast<int>(i));
                }
            }

            currentBone = skeleton[currentBone].parentIndex;
        }
    }
}

// =============================================================================
// Root Motion
// =============================================================================
math::Vec3 Animator::ExtractRootMotion(float deltaTime) const {
    // Extract root bone (index 0) movement
    if (skeleton.empty()) return math::Vec3{0.0f, 0.0f, 0.0f};

    auto currentClipName = stateToClip.find(currentState);
    if (currentClipName == stateToClip.end()) return math::Vec3{0.0f, 0.0f, 0.0f};

    AnimationClip* clip = GetClip(currentClipName->second);
    if (!clip || clip->tracks.empty()) return math::Vec3{0.0f, 0.0f, 0.0f};

    // Find root track
    for (const auto& track : clip->tracks) {
        if (track.boneIndex == 0) {
            float time1 = stateTime * clip->ticksPerSecond;
            float time2 = (stateTime + deltaTime) * clip->ticksPerSecond;

            if (clip->loop) {
                time1 = std::fmod(time1, clip->duration);
                time2 = std::fmod(time2, clip->duration);
            }

            math::Vec3 pos1 = track.SamplePosition(time1);
            math::Vec3 pos2 = track.SamplePosition(time2);

            return pos2 - pos1;
        }
    }

    return math::Vec3{0.0f, 0.0f, 0.0f};
}

math::Quaternion Animator::ExtractRootRotation(float deltaTime) const {
    (void)deltaTime;
    if (skeleton.empty()) return math::Quaternion{0.0f, 0.0f, 0.0f, 1.0f};

    // Simplified: return current root rotation
    return skeleton[0].localRotation;
}

// =============================================================================
// Events
// =============================================================================
void Animator::AddEvent(std::string_view clipName, float normalizedTime, std::string_view eventName) {
    clipEvents[std::string(clipName)].push_back(TimedEvent{normalizedTime, std::string(eventName), false});
}

void Animator::SetEventCallback(AnimationEventCallback cb) {
    eventCallback = std::move(cb);
}

void Animator::EvaluateTransitions() {
    for (const auto& trans : transitions) {
        if (trans.from == currentState && trans.to == nextState) continue;
        if (trans.from != currentState) continue;

        if (trans.condition && trans.condition()) {
            if (trans.hasExitTime) {
                auto clipName = stateToClip.find(currentState);
                if (clipName != stateToClip.end()) {
                    AnimationClip* clip = GetClip(clipName->second);
                    if (clip) {
                        float normalizedTime = (stateTime * clip->ticksPerSecond) / clip->duration;
                        if (normalizedTime >= trans.exitTime) {
                            SetState(trans.to);
                        }
                    }
                }
            } else {
                SetState(trans.to);
            }
        }
    }
}

// =============================================================================
// AnimationLoader (Stub — würde Assimp/GLTF nutzen)
// =============================================================================
std::unique_ptr<Animator> AnimationLoader::LoadFromFile(std::string_view path) {
    (void)path;
    AddLog("[Animation] LoadFromFile not yet implemented (requires Assimp/GLTF loader)");
    return std::make_unique<Animator>();
}

std::vector<Bone> AnimationLoader::LoadSkeleton(std::string_view path) {
    (void)path;
    return {};
}

std::vector<AnimationClip> AnimationLoader::LoadClips(std::string_view path) {
    (void)path;
    return {};
}

} // namespace render
