#include "time_util.h"

#include <time.h>

#include "esp_timer.h"

static bool s_synced = false;
static int64_t s_fallback = 0;

void time_util_set_synced(bool synced)
{
    s_synced = synced;
}

int64_t time_util_now(void)
{
    if (s_synced) {
        return (int64_t)time(NULL);
    }
    // Offline: strictly increasing boot-local sequence preserves ordering.
    return (int64_t)(esp_timer_get_time() / 1000) + (++s_fallback);
}
