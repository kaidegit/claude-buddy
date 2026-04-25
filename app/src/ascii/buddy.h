#pragma once

#include <stdint.h>

typedef void (*StateFn)(uint32_t t);

struct Species {
    const char *name;
    uint16_t bodyColor;
    StateFn states[7];
};
