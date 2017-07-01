#include "hks_http.h"

esp_err_t hks_http_request_parse( hks_http_request_t *request, uint8_t *buffer, size_t buffer_len )
{
  uint8_t c;
  uint8_t *ptr = buffer;
  const uint8_t *end = buffer + buffer_len;

  request->content = buffer;
  request->content_len = buffer_len;
  request->method = buffer;
  request->method_len = 0;
  request->path = NULL;
  request->path_len = 0;

  // find space after the method
  while( ( ptr < end ) && ( *ptr != ' ' ) ) ptr++;
  request->method_len = (size_t)( ptr - request->method );
  ptr++;

  // find the space after the path (hope it's URL encoded)
  request->path = ptr;
  while( ( ptr < end ) && ( *ptr != ' ' ) ) ptr++;
  request->path_len = (size_t)( ptr - request->path );
  ptr++;

  // Skip the protocol and version
  while( ( ptr < end ) && ( ( c = *ptr ) != '\r' ) && ( c != '\n' ) ) ptr++;
  ptr++;

  return ESP_OK;
}