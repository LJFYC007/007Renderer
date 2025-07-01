#pragma once
#include <memory>

template<typename T>
using ref = std::shared_ptr<T>;

template<typename T, typename... Args>
ref<T> make_ref(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}
