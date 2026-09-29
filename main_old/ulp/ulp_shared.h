#pragma once
#include <stdint.h>

typedef struct {
    uint32_t last_variance;
    uint32_t hits;
    uint32_t wake_reason;   // 0 = none, 1 = feeding suspected
} ulp_shared_t;
