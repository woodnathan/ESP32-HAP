#include <esp_err.h>

struct hks_http_request_s {
  uint8_t *content;
  size_t content_len;

  uint8_t *method;
  uint8_t method_len;

  uint8_t *path;
  uint16_t path_len;

  uint8_t *body;
};
typedef struct hks_http_request_s hks_http_request_t;

extern esp_err_t hks_http_request_parse( hks_http_request_t *request, uint8_t *buffer, size_t buffer_len );
