#pragma once
#include <cstdint>

// Global flags system for tracking render pass refresh requirements.
enum class RenderPassRefreshFlags : uint32_t
{
    None = 0x0,
    ResetAccumulation = 0x1, ///< Requests that accumulation be reset on the next frame.
};

// Bitwise operations for the flags
inline RenderPassRefreshFlags operator|(RenderPassRefreshFlags lhs, RenderPassRefreshFlags rhs)
{
    return static_cast<RenderPassRefreshFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RenderPassRefreshFlags operator&(RenderPassRefreshFlags lhs, RenderPassRefreshFlags rhs)
{
    return static_cast<RenderPassRefreshFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline RenderPassRefreshFlags& operator|=(RenderPassRefreshFlags& lhs, RenderPassRefreshFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

// Helper functions
inline bool hasFlag(RenderPassRefreshFlags flags, RenderPassRefreshFlags flag)
{
    return (flags & flag) != RenderPassRefreshFlags::None;
}
