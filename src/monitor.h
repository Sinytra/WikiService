#pragma once

#include <string>

namespace monitor {
    void initSentry(const std::string &dsn);

    void closeSentry();
}