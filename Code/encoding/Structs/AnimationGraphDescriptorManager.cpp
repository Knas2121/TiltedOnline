#include <Structs/AnimationGraphDescriptorManager.h>

AnimationGraphDescriptorManager& AnimationGraphDescriptorManager::Get() noexcept
{
    static AnimationGraphDescriptorManager s_manager;
    return s_manager;
}

const AnimationGraphDescriptor* AnimationGraphDescriptorManager::GetDescriptor(const char* acpName) const noexcept
{
    const auto itor = m_descriptors.find(acpName);
    if (itor != std::end(m_descriptors))
        return &itor->second;

    return nullptr;
}

AnimationGraphDescriptorManager::Builder::Builder(AnimationGraphDescriptorManager& aManager, const char* acpName,
                                                  AnimationGraphDescriptor aAnimationGraphDescriptor) noexcept
{
    aManager.Register(acpName, std::move(aAnimationGraphDescriptor));
}

void AnimationGraphDescriptorManager::Register(const char* acpName, AnimationGraphDescriptor aAnimationGraphDescriptor) noexcept
{
    if (m_descriptors.count(acpName))
        return;

    m_descriptors[acpName] = std::move(aAnimationGraphDescriptor);
}

