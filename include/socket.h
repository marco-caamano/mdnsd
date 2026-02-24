#ifndef SOCKET_H
#define SOCKET_H

int mdns_socket_open(const char *ifname);
void mdns_socket_close(int fd);

#endif
