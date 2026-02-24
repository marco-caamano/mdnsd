#include "socket.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "mdns.h"

int mdns_socket_open(const char *ifname) {
    int fd;
    int yes;
    unsigned int ifindex;
    struct sockaddr_in6 bind_addr;
    struct ipv6_mreq mreq;
    int hops;

    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        return -1;
    }

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        close(fd);
        return -1;
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
        close(fd);
        return -1;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_port = htons(MDNS_PORT);
    bind_addr.sin6_addr = in6addr_any;

    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        close(fd);
        return -1;
    }

    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET6, "ff02::fb", &mreq.ipv6mr_multiaddr) != 1) {
        close(fd);
        return -1;
    }
    mreq.ipv6mr_interface = ifindex;

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        close(fd);
        return -1;
    }

    hops = 255;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)) < 0) {
        close(fd);
        return -1;
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

void mdns_socket_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}
