#pragma once

#include <string>

namespace monitor {
    void initSentry(const std::string &dsn);

    void sendToSentry(const std::exception& e);

    void closeSentry();
}