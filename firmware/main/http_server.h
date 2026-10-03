// HTTP server: serves the embedded web UI and the /api/* REST routes.
#pragma once

#include "esp_err.h"

esp_err_t http_server_start(void);
