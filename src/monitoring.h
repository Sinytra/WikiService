#pragma once

#include <string>

namespace monitoring {
    void initSentry(const std::string &dsn);

    void closeSentry();
}