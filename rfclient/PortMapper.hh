#ifndef PORTMAPPER_H
#define PORTMAPPER_H

#include <vector>
#include <boost/thread.hpp>
#include "Interface.hh"

class PortMapper {
    public:
        PortMapper(uint64_t vm_id, const vector<Interface> &ifaces);
        void operator()();

    private:
        uint64_t id;
        vector<Interface> interfaces;

        void send_port_map(Interface &iface);
        int send_packet(const char ethName[], uint8_t port);
};

#endif /* PORTMAPPER_H */
