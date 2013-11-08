#include "RFProtocolFactory.h"

IPCMessage* RFProtocolFactory::buildForType(int type) {
    switch (type) {
        case PORT_REGISTER:
            return new PortRegister();
        case PORT_CONFIG:
            return new PortConfig();
        case DATAPATH_PORT_REGISTER:
            return new DatapathPortRegister();
        case DATAPATH_DOWN:
            return new DatapathDown();
        case VIRTUAL_PLANE_MAP:
            return new VirtualPlaneMap();
        case DATA_PLANE_MAP:
            return new DataPlaneMap();
        case ROUTE_MOD:
            return new RouteMod();
        case CONTROLLER_REGISTER:
            return new ControllerRegister();
        case ELECT_MASTER:
            return new ElectMaster();
        default:
            return NULL;
    }
}
