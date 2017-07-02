#include <stdio.h>
#include <esp_err.h>
#include "hks_types.h"

#define HKS_TXT_RECORD_COUNT 8
#define HKS_TXT_RECORD_LENGTH 32 // length of each string

struct hks_txt_s {
  char *records[HKS_TXT_RECORD_COUNT];
};
typedef struct hks_txt_s hks_txt_t;

typedef enum {
    HKS_TXT_STATE_UNPAIRED      = 0x01,
    HKS_TXT_STATE_NOTCONNECTED  = 0x02,
    HKS_TXT_STATE_ERROR         = 0x04
} hks_txt_state_t;

extern esp_err_t hks_txt_init( hks_txt_t *txt );
extern void hks_txt_free( hks_txt_t *txt );

extern const char **hks_txt_get_records( hks_txt_t *txt, uint8_t *count );

extern esp_err_t hks_txt_set_configuration_number( hks_txt_t *txt, uint32_t v );
extern esp_err_t hks_txt_set_device_id( hks_txt_t *txt, const uint8_t v[6] );
extern esp_err_t hks_txt_set_model_name( hks_txt_t *txt, const char *v );
extern esp_err_t hks_txt_set_protocol_version( hks_txt_t *txt, uint16_t major, uint16_t minor );
extern esp_err_t hks_txt_set_state_flags( hks_txt_t *txt, hks_txt_state_t state );
extern esp_err_t hks_txt_set_category_id( hks_txt_t *txt, hks_category_id_t id );
