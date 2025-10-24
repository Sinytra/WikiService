#include "resolved.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

#define DELIMITER "---"

using namespace logging;

namespace service {
    std::optional<Frontmatter> ResolvedProject::readPageAttributes(const std::string &path) const {
        const auto filePath = format_.getLocalizedFilePath(removeLeadingSlash(path));
        std::ifstream ifs(filePath);
        if (!ifs) {
            return std::nullopt;
        }

        // Check first delimiter
        std::string frontmatter;
        std::string line;
        std::getline(ifs, line);
        if (line != DELIMITER) {
            ifs.close();
            return std::nullopt;
        }
        // Read YAML contents
        bool valid = false;
        while (std::getline(ifs, line)) {
            if (line == DELIMITER) {
                valid = true;
                break;
            }
            frontmatter += line + "\n";
        }

        ifs.close();
        if (!valid) {
            return std::nullopt;
        }

        try {
            const auto root = YAML::Load(frontmatter);
            if (!root) {
                return std::nullopt;
            }

            const auto id = root["id"] ? root["id"].as<std::string>() : "";
            const auto title = root["title"] ? root["title"].as<std::string>() : "";
            const auto icon = root["icon"] ? root["icon"].as<std::string>() : "";

            return Frontmatter{ .id = id, .title = title, .icon = icon };
        } catch (YAML::Exception e) {
            const auto msg = std::format("{} ({}:{})", e.msg, e.mark.line + 2, e.mark.column);
            addProjectIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::PAGE, ProjectError::INVALID_FRONTMATTER, msg, path);
            logger_->error("Error parsing page metadata at path {}: {}", path, msg);
        }
        return std::nullopt;
    }
}