#include "hks_client.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <esp_log.h>
#include <lwip/sockets.h>

esp_err_t hks_client_new( int fd, hks_client_t **client )
{
  hks_client_t *new_client = (hks_client_t *)malloc( sizeof( hks_client_t ) );
  if ( !new_client )
    return ESP_ERR_NO_MEM;

  new_client->fd = fd;

  gettimeofday( &new_client->last_read, NULL );

  if ( client != NULL )
  {
    new_client->next = *client;
    *client = new_client;
  }

  return ESP_OK;
}

void hks_client_free( hks_client_t *c )
{
  free( c );
}

esp_err_t hks_client_close( hks_client_t *c )
{
  if ( c == NULL || c->fd < 0 )
    return ESP_ERR_INVALID_STATE;

  if ( lwip_close( c->fd ) )
    return ESP_FAIL;

  c->fd = -1;

  return ESP_OK;
}
