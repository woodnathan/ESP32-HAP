#include <sys/time.h>
#include <esp_err.h>

struct hks_client_s {
  int fd;
  struct timeval last_read;
  struct hks_client_s *next;
};
typedef struct hks_client_s hks_client_t;

esp_err_t hks_client_new( int fd, hks_client_t **client );
void hks_client_free( hks_client_t *client );

esp_err_t hks_client_close( hks_client_t *client );
