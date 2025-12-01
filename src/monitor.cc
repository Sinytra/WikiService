#include "monitor.h"
#include "version.h"

#include <sentry.h>

namespace monitor {
    void initSentry(const std::string &dsn) {
        sentry_options_t *options = sentry_options_new();
        sentry_options_set_dsn(options, dsn.c_str());

        sentry_options_set_database_path(options, ".sentry-native");
#ifdef PROJECT_GIT_HASH_FULL
        sentry_options_set_release(options, PROJECT_GIT_HASH_FULL);
#endif
        sentry_options_set_debug(options, 1);
        sentry_options_set_crashpad_wait_for_upload(options, true);
        sentry_init(options);
    }

    void closeSentry() { sentry_close(); }

    void sendToSentry(const std::exception& e) {
        const sentry_value_t event = sentry_value_new_event();
        const sentry_value_t exc = sentry_value_new_exception("Unhandled exception", e.what());

        sentry_value_set_stacktrace(exc, nullptr, 0);
        sentry_event_add_exception(event, exc);

        sentry_capture_event(event);
    }
}
