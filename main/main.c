#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hk_server.h"

#define HAP_TEST_WIFI_SSID CONFIG_WIFI_SSID
#define HAP_TEST_WIFI_PASS CONFIG_WIFI_PASSWORD

static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static const char *TAG = "hap-test";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch(event->event_id)
  {
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      /* enable ipv6 */
      tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      /* This is a workaround as ESP32 WiFi libs don't currently
         auto-reassociate. */
      esp_wifi_connect();
      xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
      break;
    default:
      break;
  }
  return ESP_OK;
}

static void initialise_wifi(void)
{
  tcpip_adapter_init();
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = HAP_TEST_WIFI_SSID,
      .password = HAP_TEST_WIFI_PASS,
    },
  };
  ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_ERROR_CHECK( esp_wifi_start() );
}

static void hks_task( void *pvParameters )
{
  hk_server_t *hks = NULL;
  for(;;)
  {
    xEventGroupWaitBits( wifi_event_group, CONNECTED_BIT,
                         false, true, portMAX_DELAY );

    if ( hks == NULL )
    {
      ESP_LOGI( TAG, "Starting HomeKit server..." );

      esp_err_t err = hk_server_init( TCPIP_ADAPTER_IF_STA, &hks );
      if (err)
      {
        ESP_LOGE( TAG, "Failed starting HomeKit server: %u", err );
        continue;
      }

      err = hk_server_listen( hks, 42424 );
      if (err)
      {
        ESP_LOGE( TAG, "Failed starting HomeKit server: %u", err );
        continue;
      }
    }

    if ( hks != NULL )
    {
      esp_err_t err = hk_server_accept( hks );
      if ( err && err != ESP_ERR_TIMEOUT )
      {
        ESP_LOGE( TAG, "Failed running HomeKit server: %u", err );
        continue;
      }

      err = hk_server_process_clients( hks );
      if ( err && err != ESP_ERR_TIMEOUT )
      {
        if ( err == ESP_ERR_INVALID_STATE )
          ESP_LOGE( TAG, "Fatal error processing HomeKit clients: %u", err );
        
        ESP_LOGE( TAG, "Failed processing HomeKit clients: %u", err );
        continue;
      }
    }

    taskYIELD();
  }
}

void app_main()
{
  nvs_flash_init();
  initialise_wifi();
  xTaskCreate( &hks_task, "hks_task", 2048, NULL, 5, NULL );
}
