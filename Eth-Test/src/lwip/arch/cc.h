#pragma once

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

typedef uint32_t sys_prot_t;

#define LWIP_PLATFORM_DIAG(x) do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { printf("LWIP ASSERT: %s\n", x); panic("lwIP assert"); } while (0)
