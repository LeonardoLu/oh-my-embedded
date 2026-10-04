#pragma once

#include <stdio.h>
#define ESP_LOGE(tag, ...) do { (void)(tag); fprintf(stderr, __VA_ARGS__); } while (0)
#define ESP_LOGW(tag, ...) do { (void)(tag); fprintf(stderr, __VA_ARGS__); } while (0)
#define ESP_LOGI(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGD(tag, ...) do { (void)(tag); } while (0)
