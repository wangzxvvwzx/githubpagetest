#include "flashoperation.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "FLASH_OP";

#define CUSTOM_NVS_NAMESPACE "app_cfg"

void flash_init(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS 分区满或版本更新，正在擦除重新初始化...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG, "NVS Flash 初始化完成！");
}

esp_err_t flash_save_config(const app_custom_config_t *config) {
  if (config == NULL)
    return ESP_ERR_INVALID_ARG;
  nvs_handle_t my_nvs_handle;
  esp_err_t err = nvs_open(CUSTOM_NVS_NAMESPACE, NVS_READWRITE, &my_nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "打开 NVS 失败：%s", esp_err_to_name(err));
    return err;
  }
  err = nvs_set_str(my_nvs_handle, "srv_url", config->server_url);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "写入 srv_url 失败：%S", esp_err_to_name(err));
    nvs_close(my_nvs_handle);
    return err;
  }
  err = nvs_set_i32(my_nvs_handle, "interval", config->report_interval);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "写入 interval 失败: %s", esp_err_to_name(err));
    nvs_close(my_nvs_handle);
    return err;
  }

  err = nvs_set_float(my_nvs_handle, "float_param", config->float_parameter);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "写入 float_parameter失败 %s", esp_err_to_name(err));
    nvs_close(my_nvs_handle);
    return err;
  }

  err = nvs_set_i8(my_nvs_handle, "bool_param", config->bool_parameter ? 1 : 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "写入 bool_parameter %s", esp_err_to_name(err));
    nvs_close(my_nvs_handle);
    return err;
  }

  // 4. 提交更改（必须调用 commit 才会真正写进物理闪存）
  err = nvs_commit(my_nvs_handle);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "参数保存成功 -> URL: %s, 间隔: %ld 秒", config->server_url,
             (long)config->report_interval);
  }
  nvs_close(my_nvs_handle);
  return err;
}

esp_err_t flash_load_config(app_custom_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // 先赋予一套出厂默认值（防止设备刚出厂、客户还没配网时读到空值）
  strncpy(config->server_url, "http://default.example.com",
          sizeof(config->server_url) - 1);
  config->server_url[sizeof(config->server_url) - 1] = '\0';
  config->report_interval = 30;

  nvs_handle_t my_handle;
  // 1. 以只读模式打开 "app_cfg" 命名空间
  esp_err_t err = nvs_open(CUSTOM_NVS_NAMESPACE, NVS_READONLY, &my_handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "首次开机，Flash 中尚无自定义参数，使用出厂默认值。");
    return ESP_OK;
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(err));
    return err;
  }

  // 2. 读取字符串参数 "srv_url"
  size_t required_size = sizeof(config->server_url);
  err = nvs_get_str(my_handle, "srv_url", config->server_url, &required_size);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "读取 srv_url 异常: %s", esp_err_to_name(err));
  }

  // 3. 读取整数参数 "interval"
  err = nvs_get_i32(my_handle, "interval", &config->report_interval);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "读取 interval 异常: %s", esp_err_to_name(err));
  }

  err = nvs_get_float(my_handle, "float_param", &config->float_parameter);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    ESP_LOGE(TAG, "读取 float_param 异常 %s", esp_err_to_name(err));

  uint8_t myint8 = 1;
  if (nvs_get_u8(my_handle, "bool_param", &myint8) == ESP_OK)
    config->bool_parameter = (myint8 != 0);

  ESP_LOGI(TAG, "成功从 Flash 加载参数 -> URL: %s, 间隔: %ld 秒",
           config->server_url, (long)config->report_interval);

  // 4. 关闭句柄
  nvs_close(my_handle);
  return ESP_OK;
}

esp_err_t flash_factory_reset(void) {
  ESP_LOGW(TAG, "正在执行恢复出厂设置，擦除NVS所有数据。。。");
  ESP_ERROR_CHECK(nvs_flash_erase());
  return nvs_flash_init();
}