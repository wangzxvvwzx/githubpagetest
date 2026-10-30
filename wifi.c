#include "wifi.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "portmacro.h"
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "WIFI_LOG";

//退避策略参数配置
#define INITIAL_BACKOFF_MS 1000 //初始退避时间1秒
#define MAX_BACKOFF_MS 60000    //最大退避时间60秒
#define BACKOFF_MULTIPLIER 2    //退避时间2倍增长
static uint64_t s_current_backoff_ms = INITIAL_BACKOFF_MS;
static httpd_handle_t s_web_server = NULL;
static esp_timer_handle_t s_reconnect_timer = NULL;

static void start_wifi_reconnect_backoff(void) {
  if (s_reconnect_timer == NULL) {
    return;
  }
  if (!esp_timer_is_active(s_reconnect_timer)) {
    ESP_LOGW(TAG, "将在%llu秒后尝试重连WIFI", s_current_backoff_ms / 1000);
    ESP_ERROR_CHECK(esp_timer_start_once(s_reconnect_timer,
                                         s_current_backoff_ms * 1000ULL));
    //每次事件翻倍 封顶60秒
    s_current_backoff_ms *= BACKOFF_MULTIPLIER;
    if (s_current_backoff_ms >= MAX_BACKOFF_MS) {
      s_current_backoff_ms = MAX_BACKOFF_MS;
    }
  } else {
    ESP_LOGI(TAG, "定时器已启动，在等待执行。");
  }
}

static void wifi_reconnect_timer_cb(void *arg) {
  ESP_LOGW(TAG, "定时器触发，正在发起WIFI重连。。。");
  esp_err_t err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WIFI重连失败 err:%s", esp_err_to_name(err));
    start_wifi_reconnect_backoff();
  }
}
/**
 * @brief   WiFi事件处理回调函数
 * @param   arg         参数
 * @param   event_base  事件基类
 * @param   event_id    事件ID
 * @param   event_data  事件数据
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  //如果是WIFI事件 并且是STA启动事件 就可以尝试连接WIFI了
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "开始尝试连接WIFI");
    ESP_ERROR_CHECK(esp_wifi_connect());
  }
  //如果是WIFI时间并且是STA断开连接事件 尝试重新连接
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {

    start_wifi_reconnect_backoff();
  }
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    // 1. 将 event_data 强制转换为对应的结构体指针
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "连接wifi成功，获取了ip地址为：" IPSTR,
             IP2STR(&event->ip_info.ip));
    s_current_backoff_ms = INITIAL_BACKOFF_MS;
    if (esp_timer_is_active(s_reconnect_timer)) {
      esp_timer_stop(s_reconnect_timer);
    }
  }
}

static void wifi_reconnect_timer_init(void) {
  const esp_timer_create_args_t timer_args = {.callback =
                                                  &wifi_reconnect_timer_cb,
                                              .arg = NULL,
                                              .dispatch_method = ESP_TIMER_TASK,
                                              .name = "wifi_reconnect_tmr",
                                              .skip_unhandled_events = true};
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));
}

void wifi_init(void) {
  //初始化网络接口
  ESP_ERROR_CHECK(esp_netif_init());
  //初始化事件循环
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  //创建网络接口实例
  esp_netif_create_default_wifi_sta();
  //初始化wifi驱动
  wifi_init_config_t cft = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cft));
  //注册WIFI监听事件
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  wifi_reconnect_timer_init();
}
