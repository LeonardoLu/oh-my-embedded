/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint8_t StackType_t;
#ifndef configUSE_TRACE_FACILITY
#define configUSE_TRACE_FACILITY 1
#endif
#ifndef configGENERATE_RUN_TIME_STATS
#define configGENERATE_RUN_TIME_STATS 1
#endif
#define INCLUDE_xTaskGetHandle 1
#define configTASKLIST_INCLUDE_COREID 1
#ifndef configRUN_TIME_COUNTER_TYPE
#define configRUN_TIME_COUNTER_TYPE uint32_t
#endif
#define configSTACK_DEPTH_TYPE uint32_t
#define pdPASS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
