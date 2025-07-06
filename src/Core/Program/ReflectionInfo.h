#pragma once
#include <string>
#include <nvrhi/nvrhi.h>

struct ReflectionInfo
{
    std::string name;
    nvrhi::BindingLayoutItem bindingLayoutItem;
    nvrhi::BindingSetItem bindingSetItem;
    uint32_t bindingSpace;
};