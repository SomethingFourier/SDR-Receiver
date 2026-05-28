#pragma once

#include <stdint.h>

void web_server_set_phy(const void *phy); // pass pointer to lan8720_t
void web_server_start(void);