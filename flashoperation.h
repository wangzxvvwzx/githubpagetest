#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  char server_url[256];
  int32_t report_interval;
  float float_parameter;
  bool bool_parameter;
} app_custom_config_t;

void flash_init(void);
esp_err_t flash_save_config(const app_custom_config_t *config);
esp_err_t flash_load_config(app_custom_config_t *config);