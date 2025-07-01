#include "Pass.h"
#include "Utils/Logger.h"

void Pass::trackingResourceState(nvrhi::CommandListHandle commandList)
{
    // Set up resource states for all bound resources
    const nvrhi::BindingSetDesc* desc = m_BindingSetManager->getBindingSet()->getDesc();
    for (const nvrhi::BindingSetItem& item : desc->bindings)
    {
        if (item.type == nvrhi::ResourceType::StructuredBuffer_SRV || item.type == nvrhi::ResourceType::StructuredBuffer_UAV)
        {
            nvrhi::IBuffer* buffer = dynamic_cast<nvrhi::IBuffer*>(item.resourceHandle);
            if (buffer)
                commandList->beginTrackingBufferState(
                    buffer,
                    item.type == nvrhi::ResourceType::StructuredBuffer_SRV ? nvrhi::ResourceStates::ShaderResource
                                                                           : nvrhi::ResourceStates::UnorderedAccess
                );
            else
                LOG_WARN("[RayTracingPass] WARNING: Resource handle for slot {} is not a buffer", item.slot);
        }
        else if (item.type == nvrhi::ResourceType::Texture_SRV || item.type == nvrhi::ResourceType::Texture_UAV)
        {
            nvrhi::ITexture* texture = dynamic_cast<nvrhi::ITexture*>(item.resourceHandle);
            if (texture)
            {
                nvrhi::TextureSubresourceSet allSubresources;
                commandList->beginTrackingTextureState(
                    texture,
                    allSubresources,
                    item.type == nvrhi::ResourceType::Texture_SRV ? nvrhi::ResourceStates::ShaderResource : nvrhi::ResourceStates::UnorderedAccess
                );
            }
            else
                LOG_WARN("[RayTracingPass] WARNING: Resource handle for slot {} is not a texture", item.slot);
        }
        else if (item.type == nvrhi::ResourceType::ConstantBuffer)
        {
            nvrhi::IBuffer* buffer = dynamic_cast<nvrhi::IBuffer*>(item.resourceHandle);
            if (buffer)
                commandList->beginTrackingBufferState(buffer, nvrhi::ResourceStates::ConstantBuffer);
            else
                LOG_WARN("[RayTracingPass] WARNING: Resource handle for slot {} is not a constant buffer", item.slot);
        }
        else if (item.type == nvrhi::ResourceType::RayTracingAccelStruct)
        {
            // Ray tracing acceleration structures are automatically managed by NVRHI
            // No explicit state tracking needed for acceleration structures
            LOG_TRACE("[RayTracingPass] Acceleration structure resource found for slot {}", item.slot);
        }
        else
            LOG_WARN("[RayTracingPass] WARNING: Unsupported resource type for slot {}", item.slot);
    }

    // Commit all resource state transitions
    commandList->commitBarriers();
}