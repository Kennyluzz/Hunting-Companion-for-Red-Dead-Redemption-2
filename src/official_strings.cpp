#include "official_strings.hpp"
#include "embedded_official_strings.hpp"

#include <algorithm>
#include <cstring>

namespace hunting_companion {

bool OfficialStringTable::select_locale(const char* locale) noexcept {
    clear();
    if (locale == nullptr || locale[0] == '\0') return false;
    for (const auto& candidate : embedded_official::kLocales) {
        if (std::strcmp(candidate.locale, locale) == 0) {
            entries_ = candidate.entries;
            count_ = candidate.count;
            return count_ != 0;
        }
    }
    return false;
}

const char* OfficialStringTable::find(std::uint32_t labelHash) const noexcept {
    if (entries_ == nullptr || count_ == 0 || labelHash == 0) return nullptr;
    const OfficialStringEntry* first = entries_;
    const OfficialStringEntry* last = entries_ + count_;
    const OfficialStringEntry* found = std::lower_bound(first, last, labelHash,
        [](const OfficialStringEntry& entry, std::uint32_t value) { return entry.id < value; });
    return found != last && found->id == labelHash ? found->value : nullptr;
}

} // namespace hunting_companion
