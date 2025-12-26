#pragma once

#include <cstdint>
#include <atomic>

class SceneNodeHandle
{
private:
    static std::atomic_int32_t s_handleGenerator;
    int32_t handle = c_valueInvalid;

public:
    static constexpr int32_t c_valueInvalid = 0;

    SceneNodeHandle() = default;
    explicit SceneNodeHandle(int32_t inHandle) : handle(inHandle) {}
    explicit operator int32_t() const { return handle; }

    bool operator==(const SceneNodeHandle& other) const { return handle == other.handle; }
    bool operator!=(const SceneNodeHandle& other) const { return !(*this == other); }

    static SceneNodeHandle generateHandle() { return SceneNodeHandle{ ++s_handleGenerator };  }
    static SceneNodeHandle invalidHandle() { return SceneNodeHandle{ c_valueInvalid }; }

    bool isValid() const { return handle != c_valueInvalid; }
};

template<>
struct std::hash<SceneNodeHandle>
{
    std::size_t operator()(const SceneNodeHandle& s) const noexcept
    {
        return static_cast<int>(s);
    }
};
