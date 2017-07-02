#include "hks_txt.h"
#include <stdarg.h>
#include <stdlib.h>

typedef enum {
  HKS_TXT_RECORD_CN, // c#: configuration number
  HKS_TXT_RECORD_FF, // ff: feature flags
  HKS_TXT_RECORD_ID, // id: device id/pairing identifier
  HKS_TXT_RECORD_MD, // md: model name
  HKS_TXT_RECORD_PV, // pv: protocol version <major>.<minor>
  HKS_TXT_RECORD_SN, // s#: current state number
  HKS_TXT_RECORD_SF, // sf: status flags
  HKS_TXT_RECORD_CI, // ci: category identifier
} hks_txt_record_key_t;

static esp_err_t hks_txt_set( hks_txt_t *txt, hks_txt_record_key_t key, const char *fmt, ... );
static esp_err_t hks_txt_set_feature_flags( hks_txt_t *txt, uint8_t v );
static esp_err_t hks_txt_set_state_number( hks_txt_t *txt, uint8_t v );

esp_err_t hks_txt_init( hks_txt_t *txt )
{
  for ( int i = 0; i < HKS_TXT_RECORD_COUNT; i++ )
  {
    txt->records[i] = malloc( HKS_TXT_RECORD_LENGTH );

    // TODO: to check result and free preceding items
  }

  uint8_t device_id[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  hks_txt_set_configuration_number( txt, 1 );
  hks_txt_set_feature_flags( txt, 0x01 );
  hks_txt_set_device_id( txt, device_id );
  hks_txt_set_model_name( txt, "ESP1,1" );
  hks_txt_set_protocol_version( txt, 1, 0 );
  hks_txt_set_state_number( txt, 1 );
  hks_txt_set_state_flags( txt, HKS_TXT_STATE_UNPAIRED );
  hks_txt_set_category_id( txt, HKS_CATEGORY_ID_OTHER );

  return ESP_OK;
}

void hks_txt_free( hks_txt_t *txt )
{
  for ( int i = 0; i < HKS_TXT_RECORD_COUNT; i++ )
  {
    free( txt->records[i] );
    txt->records[i] = NULL;
  }
}

const char **hks_txt_get_records( hks_txt_t *txt, uint8_t *count )
{
  if ( count )
    *count = HKS_TXT_RECORD_COUNT;
  return (const char **)txt->records;
}

esp_err_t hks_txt_set( hks_txt_t *txt, hks_txt_record_key_t key, const char *fmt, ... )
{
  if ( txt == NULL )
    return ESP_ERR_INVALID_STATE;

  va_list ap;
  va_start( ap, fmt );
  int n = vsnprintf( txt->records[key], HKS_TXT_RECORD_LENGTH, fmt, ap );
  va_end( ap );

  if ( n < 0 )
    return ESP_FAIL;

  return ESP_OK;
}

esp_err_t hks_txt_set_configuration_number( hks_txt_t *txt, uint32_t v )
{
  return hks_txt_set( txt, HKS_TXT_RECORD_CN, "c#=%u", v );
}

esp_err_t hks_txt_set_feature_flags( hks_txt_t *txt, uint8_t v )
{
  return hks_txt_set( txt, HKS_TXT_RECORD_FF, "ff=%hhu", v );
}

esp_err_t hks_txt_set_device_id( hks_txt_t *txt, const uint8_t v[6] )
{
  return hks_txt_set(
    txt,
    HKS_TXT_RECORD_ID,
    "id=%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
    v[0], v[1], v[2], v[3], v[4], v[5]
  );
}

esp_err_t hks_txt_set_model_name( hks_txt_t *txt, const char *v )
{
  return hks_txt_set( txt, HKS_TXT_RECORD_MD, "md=%s", v );
}

esp_err_t hks_txt_set_protocol_version( hks_txt_t *txt, uint16_t major, uint16_t minor )
{
  return hks_txt_set( txt, HKS_TXT_RECORD_PV, "pv=%hu.%hu", major, minor );
}

esp_err_t hks_txt_set_state_number( hks_txt_t *txt, uint8_t v )
{
  return hks_txt_set( txt, HKS_TXT_RECORD_SN, "s#=%hhu", v );
}

esp_err_t hks_txt_set_state_flags( hks_txt_t *txt, hks_txt_state_t state )
{
  return hks_txt_set( txt, HKS_TXT_RECORD_SF, "sf=%hhu", state );
}

esp_err_t hks_txt_set_category_id( hks_txt_t *txt, hks_category_id_t id )
{
  return hks_txt_set( txt, HKS_TXT_RECORD_CI, "ci=%hu", id );
}
