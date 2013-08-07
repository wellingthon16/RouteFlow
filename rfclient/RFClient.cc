#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <ifaddrs.h>
#include <syslog.h>
#include <cstdlib>
#include <boost/thread.hpp>
#include <iomanip>

#include "RFClient.hh"
#include "converter.h"
#include "defs.h"
#include "FlowTable.h"

using namespace std;

/* Get the MAC address of the interface. */
int get_hwaddr_byname(const char * ifname, uint8_t hwaddr[]) {
    struct ifreq ifr;
    int sock;

    if ((NULL == ifname) || (NULL == hwaddr)) {
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

    if (-1 == ioctl(sock, SIOCGIFHWADDR, &ifr)) {
        perror("ioctl(SIOCGIFHWADDR) ");
        return -1;
    }

    std::memcpy(hwaddr, ifr.ifr_ifru.ifru_hwaddr.sa_data, IFHWADDRLEN);

    close(sock);

    return 0;
}

/* Get the interface associated VM identification number. */
uint64_t get_interface_id(const char *ifname) {
    if (ifname == NULL)
        return 0;

    uint8_t mac[6];
    uint64_t id;
    stringstream hexmac;

    if (get_hwaddr_byname(ifname, mac) == -1)
        return 0;

    for (int i = 0; i < 6; i++)
        hexmac << std::hex << setfill ('0') << setw (2) << (int) mac[i];
    hexmac >> id;
    return id;
}

bool RFClient::findInterface(const char *ifName, Interface *dst) {
    boost::lock_guard<boost::mutex> lock(this->ifMutex);

    map<string, Interface>::iterator it = this->ifacesMap.find(ifName);
    if (it == this->ifacesMap.end()) {
        return false;
    }

    *dst = it->second;
    return true;
}

RFClient::RFClient(uint64_t id, const string &address) {
    this->id = id;
    syslog(LOG_INFO, "Starting RFClient (vm_id=%s)", to_string<uint64_t>(this->id).c_str());
    ipc = (IPCMessageService*) new MongoIPCMessageService(address, MONGO_DB_NAME, to_string<uint64_t>(this->id));

    vector<Interface>::iterator it;
    vector<Interface> ifaces = this->load_interfaces();

    for (it = ifaces.begin(); it != ifaces.end(); it++) {
        this->ifacesMap[it->name] = *it;
        this->interfaces[it->port] = &this->ifacesMap[it->name];

        PortRegister msg(this->id, it->port, it->hwaddress);
        this->ipc->send(RFCLIENT_RFSERVER_CHANNEL, RFSERVER_ID, msg);
        syslog(LOG_INFO, "Registering client port (vm_port=%d)", it->port);
    }

    this->startFlowTable();
    this->startPortMapper(ifaces);

    ipc->listen(RFCLIENT_RFSERVER_CHANNEL, this, this, true);
}

void RFClient::startFlowTable() {
    boost::thread t(&FlowTable::start, this->id, this, this->ipc);
    t.detach();
}

void RFClient::startPortMapper(vector<Interface> ifaces) {
    this->portMapper = new PortMapper(this->id, ifaces);
    boost::thread t(*this->portMapper);
    t.detach();
}

bool RFClient::process(const string &, const string &, const string &, IPCMessage& msg) {
    int type = msg.get_type();
    if (type == PORT_CONFIG) {
        boost::lock_guard<boost::mutex> lock(this->ifMutex);

        PortConfig *config = dynamic_cast<PortConfig*>(&msg);
        uint32_t vm_port = config->get_vm_port();
        uint32_t operation_id = config->get_operation_id();

        switch (operation_id) {
        case PCT_MAP_REQUEST:
            syslog(LOG_WARNING, "Received deprecated PortConfig (vm_port=%d) "
                   "(type: %d)", vm_port, operation_id);
            break;
        case PCT_RESET:
            syslog(LOG_INFO, "Received port reset (vm_port=%d)", vm_port);
            this->interfaces[vm_port]->active = false;
            break;
        case PCT_MAP_SUCCESS:
            syslog(LOG_INFO, "Successfully mapped port(vm_port=%d)", vm_port);
            this->interfaces[vm_port]->active = true;
            break;
        default:
            syslog(LOG_WARNING, "Recieved unrecognised PortConfig message");
            return false;
        }
    } else {
        return false;
    }

    return true;
}

/* Set the MAC address of the interface. */
int RFClient::set_hwaddr_byname(const char * ifname, uint8_t hwaddr[], int16_t flags) {
    struct ifreq ifr;
    int sock;

    if ((NULL == ifname) || (NULL == hwaddr)) {
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
    ifr.ifr_ifru.ifru_flags = flags & (~IFF_UP);

    if (-1 == ioctl(sock, SIOCSIFFLAGS, &ifr)) {
        perror("ioctl(SIOCSIFFLAGS) ");
        return -1;
    }

    ifr.ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_ETHER;
    std::memcpy(ifr.ifr_ifru.ifru_hwaddr.sa_data, hwaddr, IFHWADDRLEN);

    if (-1 == ioctl(sock, SIOCSIFHWADDR, &ifr)) {
        perror("ioctl(SIOCSIFHWADDR) ");
        return -1;
    }

    ifr.ifr_ifru.ifru_flags = flags | IFF_UP;

    if (-1 == ioctl(sock, SIOCSIFFLAGS, &ifr)) {
        perror("ioctl(SIOCSIFFLAGS) ");
        return -1;
    }

    close(sock);

    return 0;
}

uint32_t RFClient::get_port_number(string ifName) {
    size_t pos = ifName.find_first_of("123456789");
    string port_num = ifName.substr(pos, ifName.length() - pos + 1);
    return atoi(port_num.c_str());
}

/* Gather all of the interfaces on the system. */
vector<Interface> RFClient::load_interfaces() {
    struct ifaddrs *ifaddr, *ifa;
    vector<Interface> result;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit( EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        int family = ifa->ifa_addr->sa_family;

        if (family == AF_PACKET && strcmp(ifa->ifa_name, "eth0") != 0
            && strcmp(ifa->ifa_name, "lo") != 0) {
            string ifaceName = ifa->ifa_name;
            get_hwaddr_byname(ifa->ifa_name, hwaddress);

            Interface interface;
            interface.name = ifaceName;
            interface.port = get_port_number(ifaceName);
            interface.hwaddress = MACAddress(hwaddress);
            interface.active = false;

            printf("Loaded interface %s\n", interface.name.c_str());
            syslog(LOG_INFO, "Loaded interface %s\n", interface.name.c_str());

            result.push_back(interface);
        }
    }

    freeifaddrs(ifaddr);
    return result;
}

int main(int argc, char* argv[]) {
    char c;
    stringstream ss;
    string id;
    string address = MONGO_ADDRESS;

    while ((c = getopt (argc, argv, "n:i:a:")) != -1)
        switch (c) {
            case 'n':
                fprintf (stderr, "Custom naming not supported yet.");
                exit(EXIT_FAILURE);
                /* TODO: support custom naming for VMs.
                if (!id.empty()) {
                    fprintf (stderr, "-i is already defined");
                    exit(EXIT_FAILURE);
                }
                id = optarg;
                */
                break;
            case 'i':
                if (!id.empty()) {
                    fprintf(stderr, "-n is already defined");
                    exit(EXIT_FAILURE);
                }
                id = to_string<uint64_t>(get_interface_id(optarg));
                break;
            case 'a':
                address = optarg;
                break;
            case '?':
                if (optopt == 'n' || optopt == 'i' || optopt == 'a')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return EXIT_FAILURE;
            default:
                abort();
        }


    openlog("rfclient", LOG_NDELAY | LOG_NOWAIT | LOG_PID, SYSLOGFACILITY);
    RFClient s(get_interface_id(DEFAULT_RFCLIENT_INTERFACE), address);

    return 0;
}
