// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#define pdTRUE 1
#define pdMS_TO_TICKS(value) (value)
typedef void *SemaphoreHandle_t;
#ifndef __APPLE__
size_t strlcpy(char *dest, const char *src, size_t size);
#endif
