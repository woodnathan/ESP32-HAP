#include "hks_utils.h"
#include <esp_wifi.h>

esp_err_t hksu_validate_if( tcpip_adapter_if_t tcpip_if )
{
  esp_err_t err = ESP_OK;

  if ( tcpip_if >= TCPIP_ADAPTER_IF_MAX )
    return ESP_ERR_INVALID_ARG;

  uint8_t mode;
  err = esp_wifi_get_mode( (wifi_mode_t *)&mode );
  if ( err )
    return err;

  if ((tcpip_if == TCPIP_ADAPTER_IF_STA && !(mode & WIFI_MODE_STA)) ||
      (tcpip_if == TCPIP_ADAPTER_IF_AP && !(mode & WIFI_MODE_AP)))
  {
    return ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}
