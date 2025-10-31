#pragma once
#include <string>
#include <nvrhi/nvrhi.h>

struct ReflectionInfo
{
    std::string name;
    nvrhi::BindingLayoutItem bindingLayoutItem;
    nvrhi::BindingSetItem bindingSetItem;
    uint32_t bindingSpace;

    // Descriptor table support
    bool isDescriptorTable = false;
    uint32_t descriptorTableSize = 0; // Size of the descriptor table (0 for unbounded)
};