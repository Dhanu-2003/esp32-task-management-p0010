// Monotonic/wall-clock helper: SNTP time when online, boot-order fallback offline.
#pragma once

#include <stdbool.h>
#include <stdint.h>

void time_util_set_synced(bool synced);
int64_t time_util_now(void);
