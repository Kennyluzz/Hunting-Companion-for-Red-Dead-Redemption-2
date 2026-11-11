#pragma once

#include <cstddef>
#include <cstdint>

namespace hunting_companion {

struct OfficialStringEntry {
    std::uint32_t id;
    const char* value;
};

// Immutable, compact Rockstar catalogue strings embedded in the ASI.
class OfficialStringTable {
public:
    bool select_locale(const char* locale) noexcept;
    const char* find(std::uint32_t labelHash) const noexcept;
    std::size_t size() const noexcept { return count_; }
    void clear() noexcept { entries_ = nullptr; count_ = 0; }

private:
    const OfficialStringEntry* entries_{};
    std::size_t count_{};
};

} // namespace hunting_companion
