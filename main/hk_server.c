#include "hk_server.h"
#include <string.h>
#include <mdns.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_log.h>
#include "hks_utils.h"
#include "hks_client.h"
#include "hks_http.h"

static const char *TAG = "hk-server";

struct hk_server_s
{
  tcpip_adapter_if_t tcpip_if;
  mdns_server_t *mdns;
  int fd;
  xSemaphoreHandle lock;

  hks_client_t *clients;
};

static const char *_HK_HAP_SERVICE = "_hap";
static const char *_HK_HAP_PROTO   = "_tcp";

static esp_err_t _hk_server_bind( hk_server_t *hks, uint16_t port );
static esp_err_t _hk_server_accept( hk_server_t *hks );
static esp_err_t _hk_server_close_client(
  hk_server_t *hks,
  hks_client_t *client,
  hks_client_t *previous
);
static esp_err_t _hk_server_read_client( hks_client_t *c );

esp_err_t hk_server_init( tcpip_adapter_if_t tcpip_if, hk_server_t **hks )
{
  esp_err_t err = ESP_OK;

  err = hksu_validate_if( tcpip_if );
  if ( err )
    return err;

  hk_server_t *server = (hk_server_t *)malloc( sizeof( hk_server_t ) );
  if ( !server )
    return ESP_ERR_NO_MEM;

  server->tcpip_if = tcpip_if;
  server->mdns = NULL;
  server->fd = -1;
  server->clients = NULL;

  err = mdns_init( TCPIP_ADAPTER_IF_STA, &server->mdns );
  if ( err )
  {
    free( server );
    return err;
  }

  server->lock = xSemaphoreCreateMutex();
  if ( !server->lock )
  {
    free( server );
    return ESP_ERR_NO_MEM;
  }

  *hks = server;

  return ESP_OK;
}

void hk_server_free( hk_server_t *hks )
{
  if ( hks == NULL )
    return;

  hk_server_stop( hks );

  mdns_free( hks->mdns );
  vSemaphoreDelete( hks->lock );

  free( hks );
}

esp_err_t hk_server_listen( hk_server_t *hks, uint16_t port )
{
  esp_err_t err = ESP_OK;
  if ( hks == NULL || hks->fd >= 0 )
    return ESP_ERR_INVALID_STATE;

  int fd = lwip_socket( AF_INET, SOCK_STREAM, 0 );
  if ( fd < 0 )
    return ESP_FAIL;

  hks->fd = fd;

  err = _hk_server_bind( hks, port );
  if ( err )
    return err;

  err = mdns_service_add( hks->mdns, _HK_HAP_SERVICE, _HK_HAP_PROTO, port );
  if ( err )
    return err;

  const char *hapTxtData[8] = {
    "c#=1",
    "ff=1",
    "id=c4:b3:01:c3:f7:9d",
    "md=ESP32,1",
    "pv=1.0",
    "s#=1",
    "sf=1",
    "ci=7"
  };
  err = mdns_service_txt_set( hks->mdns, _HK_HAP_SERVICE, _HK_HAP_PROTO, 8, hapTxtData );
  if ( err )
    return err;

  return ESP_OK;
}

esp_err_t hk_server_stop( hk_server_t *hks )
{
  esp_err_t err = ESP_OK;

  if ( hks == NULL )
    return ESP_ERR_INVALID_STATE;

  err = mdns_service_remove( hks->mdns, _HK_HAP_SERVICE, _HK_HAP_PROTO );
  if ( err )
    return err;

  if ( lwip_close( hks->fd ) )
    return ESP_FAIL;

  return ESP_OK;
}

esp_err_t hk_server_accept( hk_server_t *hks )
{
  esp_err_t err = ESP_OK;

  if ( hks == NULL || hks->fd < 0)
    return ESP_ERR_INVALID_STATE;

  fd_set fds;
  struct timeval tv = { .tv_usec = 250 };

  FD_ZERO( &fds );
  FD_SET( hks->fd, &fds );

  int result = lwip_select( hks->fd + 1, &fds, NULL, NULL, &tv );
  if ( result == 0 )
    return ESP_ERR_TIMEOUT;

  if ( result < 0 )
    return ESP_FAIL;

  if ( FD_ISSET( hks->fd, &fds ) )
  {
    err = _hk_server_accept( hks );
    if ( err )
      return err;
  }

  return ESP_OK;
}

esp_err_t hk_server_process_clients( hk_server_t *hks )
{
  esp_err_t err = ESP_OK;

  if ( hks == NULL || hks->fd < 0)
    return ESP_ERR_INVALID_STATE;

  fd_set fds;
  struct timeval tv = { .tv_usec = 250 };

  FD_ZERO( &fds );
  FD_SET( hks->fd, &fds );

  struct timeval now;
  gettimeofday( &now, NULL );

  int maxfd = -1;
  hks_client_t *prev = NULL;
  for ( hks_client_t *client = hks->clients; client != NULL; /* see below */ )
  {
    if ( (now.tv_sec - client->last_read.tv_sec) > 60 )
    {
      ESP_LOGI( TAG, "Closing client (%d) due to timeout", client->fd );

      hks_client_t *next = client->next;
      err = _hk_server_close_client( hks, client, prev ); // close and remove
      if ( err == ESP_ERR_INVALID_STATE )
        return ESP_ERR_INVALID_STATE; // Fatal
      client = next;
      continue;
    }

    FD_SET( client->fd, &fds );

    if ( client->fd > maxfd )
      maxfd = client->fd;

    prev = client;
    client = client->next;
  }

  if ( maxfd < -1 )
    return ESP_OK; // No clients to process

  int result = lwip_select( maxfd + 1, &fds, NULL, NULL, &tv );
  if ( result == 0 )
    return ESP_ERR_TIMEOUT;

  if ( result < 0 )
    return ESP_FAIL;

  prev = NULL;
  for ( hks_client_t *client = hks->clients; client != NULL; /* see below */ )
  {
    if ( FD_ISSET( client->fd, &fds ) )
    {
      err = _hk_server_read_client( client );
      if ( err )
      {
        hks_client_t *next = client->next;
        err = _hk_server_close_client( hks, client, prev ); // close and remove
        if ( err == ESP_ERR_INVALID_STATE )
          return ESP_ERR_INVALID_STATE; // Fatal
        client = next;
        continue;
      }
    }
    prev = client;
    client = client->next;
  }

  return ESP_OK;
}

esp_err_t _hk_server_bind( hk_server_t *hks, uint16_t port )
{
  struct sockaddr_in sock_addr;
  bzero( &sock_addr, sizeof(sock_addr) );
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = 0;
  sock_addr.sin_port = htons( port );

  if ( lwip_bind( hks->fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr) ) < 0 )
    return ESP_FAIL;

  int flags = lwip_fcntl( hks->fd, F_GETFL, 0 );
  if ( flags < 0 )
    return ESP_FAIL;

  if ( lwip_fcntl( hks->fd, F_SETFL, flags | O_NONBLOCK ) < 0 )
    return ESP_FAIL;

  if ( lwip_listen( hks->fd, 3 ) )
    return ESP_FAIL;

  return ESP_OK;
}

esp_err_t _hk_server_accept( hk_server_t *hks )
{
  int new_socket = 0;
  socklen_t addr_len;
  struct sockaddr_in sock_addr;
  if ((new_socket = lwip_accept( hks->fd, (struct sockaddr *)&sock_addr, (socklen_t *)&addr_len )) < 0)
    return ESP_FAIL;

  ESP_LOGI( TAG, "Accepted new client (%d)", new_socket );

  return hks_client_new( new_socket, &hks->clients );
}

esp_err_t _hk_server_close_client( hk_server_t *hks, hks_client_t *c, hks_client_t *p )
{
  __unused esp_err_t err = ESP_OK;

  if ( hks == NULL || c == NULL )
    return ESP_ERR_INVALID_STATE;

  err = hks_client_close( c );

  if ( p )
  {
    p->next = c->next;
  }
  else if ( hks->clients == c )
  {
    hks->clients = c->next;
  }
  else
  {
    return ESP_ERR_INVALID_STATE;
  }

  hks_client_free( c );

  return ESP_OK;
}

esp_err_t _hk_server_read_client( hks_client_t *c )
{
  esp_err_t err = ESP_OK;

  if ( c == NULL )
    return ESP_ERR_INVALID_STATE;

  gettimeofday( &c->last_read, NULL );

  uint8_t buffer[256];
  bzero( buffer, 256 );
  int n = lwip_read( c->fd, buffer, 255 );
  if (n < 0)
  {
    return ESP_FAIL;
  }

  hks_http_request_t request;
  err = hks_http_request_parse( &request, buffer, n );
  if ( err )
    return err;


  ESP_LOGI( TAG, "%.*s Request: %.*s",
    request.method_len,
    request.method,
    request.path_len,
    request.path
  );

  return ESP_OK;
}