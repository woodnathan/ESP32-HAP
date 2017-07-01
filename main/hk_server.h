#include <stdio.h>
#include <tcpip_adapter.h>
#include <esp_err.h>

struct hk_server_s;
typedef struct hk_server_s hk_server_t;

extern esp_err_t hk_server_init( tcpip_adapter_if_t tcpip_if, hk_server_t **hks );
extern void hk_server_free( hk_server_t *hks );

extern esp_err_t hk_server_listen( hk_server_t *hks, uint16_t port );
extern esp_err_t hk_server_stop( hk_server_t *hks );

extern esp_err_t hk_server_set_name( hk_server_t *hks, const char *name );

// client accept loop
extern esp_err_t hk_server_accept( hk_server_t *hks );
extern esp_err_t hk_server_process_clients( hk_server_t *hks );