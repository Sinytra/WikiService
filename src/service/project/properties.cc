#include "properties.h"

#include "schemas.h"
#include "util.h"

namespace content {
    // TODO Cache
    nlohmann::json parseItemProperties(const std::filesystem::path &path, const std::string &id) {
        const auto json = parseJsonFile(path);
        if (!json) {
            return nullptr;
        }

        if (const auto error = validateJson(schemas::properties, *json)) {
            return nullptr;
        }

        if (!json->contains(id)) {
            return nullptr;
        }

        return (*json)[id];
    }
}
