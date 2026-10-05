// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include "lua.h"
esp_err_t cap_lua_register_module(const char *name, lua_CFunction loader);
