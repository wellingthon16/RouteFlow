#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <syslog.h>
#include <cstdio>
#include <boost/bind.hpp>

#include "defs.h"
#include "PortMapper.hh"

#define BUFFER_SIZE 23 /* Mapping packet size. */
#define SLEEP_TIME boost::posix_time::seconds(10)

PortMapper::PortMapper(uint64_t vm_id, const vector<Interface> &ifaces) {
    this->id = vm_id;
    this->interfaces = ifaces;
    /* XXX: This list of interfaces is never updated. */
}

/**
 * Loops through interfaces, sending mapping packets out each, then sleeps.
 * Rinse and repeat ad infinitum.
 */
void PortMapper::operator()() {
    while(true) {
        boost::system_time timeout = boost::get_system_time() + SLEEP_TIME;

        /* Grab list of interfaces from RFClient, iterate, send map packet */
        vector<Interface>::iterator it;
        for (it = this->interfaces.begin(); it != this->interfaces.end(); it++) {
            this->send_port_map(*it);
        }

        boost::this_thread::sleep(timeout);
    }
}

/**
 * Sends a portmap packet out the given interface, logging the success or
 * failure of the endeavour.
 */
void PortMapper::send_port_map(Interface &iface) {
    if (send_packet(iface.name.c_str(), iface.port) == -1)
        syslog(LOG_NOTICE, "Error sending mapping packet (vm_port=%d)",
               iface.port);
    else
        syslog(LOG_DEBUG, "Mapping packet was sent to RFVS (vm_port=%d)",
               iface.port);
}

/**
 * Sends the magic RouteFlow portmap packet out an interface with the given
 * name, encoding the given port number inside the message.
 *
 * Returns the number of characters sent, or -1 on failure.
 */
int PortMapper::send_packet(const char ethName[], uint8_t port) {
    char buffer[BUFFER_SIZE];
    char error[BUFSIZ];
    uint16_t ethType;
    struct ifreq req;
    struct sockaddr_ll sll;
    uint8_t srcAddress[IFHWADDRLEN];
    uint8_t dstAddress[IFHWADDRLEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    int SockFd = socket(PF_PACKET, SOCK_RAW, htons(RF_ETH_PROTO));

    strcpy(req.ifr_name, ethName);

    if (ioctl(SockFd, SIOCGIFFLAGS, &req) < 0) {
        strerror_r(errno, error, BUFSIZ);
        syslog(LOG_ERR, "ioctl(SIOCGIFFLAGS): %s", error);
        return -1;
    }

    /* If the interface is down we can't send the packet. */
    printf("FLAG %d\n", req.ifr_flags & IFF_UP);
    if (!(req.ifr_flags & IFF_UP))
        return -1;

    /* Get the interface index. */
    if (ioctl(SockFd, SIOCGIFINDEX, &req) < 0) {
        strerror_r(errno, error, BUFSIZ);
        syslog(LOG_ERR, "ioctl(SIOCGIFINDEX): %s", error);
        return -1;
    }

    int ifindex = req.ifr_ifindex;
    int addrLen = sizeof(struct sockaddr_ll);

    if (ioctl(SockFd, SIOCGIFHWADDR, &req) < 0) {
        strerror_r(errno, error, BUFSIZ);
        syslog(LOG_ERR, "ioctl(SIOCGIFHWADDR): %s", error);
        return -1;
    }
    int i;
    for (i = 0; i < IFHWADDRLEN; i++)
        srcAddress[i] = (uint8_t) req.ifr_hwaddr.sa_data[i];

    memset(&sll, 0, sizeof(struct sockaddr_ll));
    sll.sll_family = PF_PACKET;
    sll.sll_ifindex = ifindex;

    if (bind(SockFd, (struct sockaddr *) &sll, addrLen) < 0) {
        strerror_r(errno, error, BUFSIZ);
        syslog(LOG_ERR, "bind(): %s", error);
        return -1;
    }

    memset(buffer, 0, BUFFER_SIZE);

    memcpy((void *) buffer, (void *) dstAddress, IFHWADDRLEN);
    memcpy((void *) (buffer + IFHWADDRLEN), (void *) srcAddress, IFHWADDRLEN);
    ethType = htons(RF_ETH_PROTO);
    memcpy((void *) (buffer + 2 * IFHWADDRLEN), (void *) &ethType,
            sizeof(uint16_t));
    memcpy((void *) (buffer + 14), (void *) &this->id, sizeof(uint64_t));
    memcpy((void *) (buffer + 22), (void *) &port, sizeof(uint8_t));
    return (sendto(SockFd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &sll,
            (socklen_t) addrLen));
}
