/* pgwire_api.h — exports do listener (banco/pgwire.c) para o medidor. */
#ifndef PGWIRE_API_H
#define PGWIRE_API_H

#include <stdint.h>

int pgwire_send_all(int fd, const void *buf, int n);
int pgwire_recv_n(int fd, void *buf, int n);
int pgwire_listen(int want, int *got);
int pgwire_serve_conn(int cfd, int32_t pid, int32_t key);

#endif
