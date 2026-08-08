#ifndef ENGINE_SHARED_WEBSOCKETS_H
#define ENGINE_SHARED_WEBSOCKETS_H

#include <base/detect.h>
#include <base/types.h>

#if defined(CONF_FAMILY_UNIX)
#include <sys/select.h>
#elif defined(CONF_FAMILY_WINDOWS)
#include <winsock2.h>
#endif

#include <cstddef>

// NOLINTBEGIN(readability-identifier-naming)
void websocket_init();
int websocket_create(const NETADDR *bindaddr);
void websocket_destroy(int socket);
int websocket_recv(int socket, unsigned char *data, size_t maxsize, NETADDR *addr);
int websocket_send(int socket, const unsigned char *data, size_t size, const NETADDR *addr);
// Whether a live socket still backs this peer. A closed one cannot be reported
// as a packet: the bridge knows neither the security token nor the sequence a
// connection validates against, so it can only be asked.
int websocket_peer_connected(int socket, const NETADDR *addr);
int websocket_fd_set(int socket, fd_set *set);
int websocket_fd_get(int socket, fd_set *set);
// NOLINTEND(readability-identifier-naming)

#endif // ENGINE_SHARED_WEBSOCKETS_H
