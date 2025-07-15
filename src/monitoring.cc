#include "monitoring.h"
#include "version.h"

#include <sentry.h>

namespace monitoring {
    void initSentry(const std::string &dsn) {
        sentry_options_t *options = sentry_options_new();
        sentry_options_set_dsn(options, dsn.c_str());

        sentry_options_set_database_path(options, ".sentry-native");
#ifdef PROJECT_GIT_HASH_FULL
        sentry_options_set_release(options, PROJECT_GIT_HASH_FULL);
#endif
        sentry_options_set_debug(options, 1);
        sentry_init(options);
    }

    void closeSentry() { sentry_close(); }
}
