#!/usr/bin/env python
#-*- coding:utf-8 -*-

import logging
import threading
import time
import socket
import random

import rflib.ipc.IPC as IPC
import rflib.ipc.MongoIPC as MongoIPC
from rflib.ipc.RFProtocol import *
from rflib.ipc.RFProtocolFactory import RFProtocolFactory
from rflib.defs import *
from rflib.types.Match import *
from rflib.types.Action import *
from rflib.types.Option import *


class RFMonitor(RFProtocolFactory, IPC.IPCMessageProcessor):
    """Monitors all the controller instances for failiure

    Attributes-
    controllers: A dictionary mapping controller address and
                 port to controller role and number of devices 
                 it is connected to.
    monitors: A dictionary mapping controllers to monitor objects
              responsible for scheduling tests.
    eligible_masters: A dictionary mapping controllers to the maximum
                      count of devices they are connected too.

    """
    def __init__(self, *arg, **kwargs):
        self.controllers = dict()
        self.monitors = dict()
        self.eligible_masters = dict()
        self.controllerLock = threading.Lock()
        self.ipc = MongoIPC.MongoIPCMessageService(MONGO_ADDRESS,
                                                   MONGO_DB_NAME,
                                                   RFMONITOR_ID,
                                                   threading.Thread,
                                                   time.sleep)
        self.ipc.listen(RFMONITOR_RFPROXY_CHANNEL, self, self, False)
        self.log = logging.getLogger("rfmonitor")
        self.log.setLevel(logging.INFO)
        ch = logging.StreamHandler()
        ch.setLevel(logging.INFO)
        ch.setFormatter(logging.Formatter(logging.BASIC_FORMAT))
        self.log.addHandler(ch)
        self.test_controllers()

    def process(self, _from, to, channel, msg):
        """Process messages sent by controllers.

        Types of messages being handled:
        CONTROLLER_REGISTER -- Register Controller details with RFMonitor.

        """        
        type_ = msg.get_type()
        address = msg.get_ct_addr()
        port = msg.get_ct_port()
        role = msg.get_ct_role()
        if type_ == CONTROLLER_REGISTER:
            self.controllerLock.acquire()
            try:
                if ((address + ':' + str(port)) not in 
                    self.controllers):
                    self.controllers[address + ':' + str(port)] = {
                        'role': role,
                        'count': 1
                    }
                    self.log.info("A %s controller at %s:%s is up",
                                  role, address, port)
                else:
                    self.controllers[msg.get_ct_addr() + ':'
                                     + str(msg.get_ct_port())]['count'] += 1
                controller_count = self.controllers[address + ':' 
                                                   + str(port)]['count']

                if not self.eligible_masters:
                    self.eligible_masters[address + ':' + str(port)] = \
                        controller_count
                else:
                    maximum_controller_count = self.eligible_masters.values()[0]
                    if maximum_controller_count < controller_count:
                        self.eligible_masters = {}
                        self.eligible_masters[address + ':' + str(port)] = \
                            controller_count
                    elif maximum_controller_count == controller_count:
                        self.eligible_masters[address + ':' + str(port)] = \
                            controller_count

            finally:
                self.controllerLock.release()

    def test_controllers(self):
        """Invoke test on all the controllers"""
        while True:
            #Extract all the keys from self.controllers first so that 
            #the main thread does not block the IPC thread
            self.controllerLock.acquire()
            try:
                controllers = self.controllers.keys()
            finally:
                self.controllerLock.release()
            for controller in controllers:
                host, port = controller.split(':')
                port = int(port)
                if controller in self.monitors:
                    monitor = self.monitors[controller]
                    #check if scheduled time has passed
                    if monitor.timeout < time.time():
                        self.test(host, port)
                        monitor.schedule_test()
                    else:
                        continue
                else:
                    monitor = Monitor(host, port, callback_time=5000)
                    self.monitors[controller] = monitor

    def test(self, host, port):
        """Test if a controller is up.

        Keyword Arguments:
        host -- host ip address at which controller is listening.
        port -- port at which the controller is listening at `host` address.

        """
        s = socket(AF_INET, SOCK_STREAM)
        s.settimeout(1)
        result = s.connect_ex((host, port))

        if result != 0:
            self.log.info("Controller listening at %s:%s died", host, port)
            self.handle_controller_death(host, port)
        s.close()

    def handle_controller_death(self, host, port):
        """Remove all entries coresponding to a controller and 
        elect new master if master controller is dead

        Keyword Arguments:
        host -- host ip address at which controller was listening.
        port -- port at which the controller was listening at `host` address.

        """
        master = False
        self.controllerLock.acquire()
        try:
            if self.controllers[host + ':' + str(port)]['role'] == "master":
                master = True
            self.controllers.pop(host + ':' + str(port), None)
            self.monitors.pop(host + ':' + str(port), None)
            self.eligible_masters.pop(host + ':' + str(port), None)
        finally:
            self.controllerLock.release()
        if master:
            self.elect_new_master()

    def elect_new_master(self):
        """Elect new master controller and inform to rfproxy"""
        master_key = random.randint(0, len(self.eligible_masters)-1)
        new_master = self.eligible_masters.keys()[master_key]
        self.log.info("The new master is %s", new_master)
        host, port = new_master.split(":")
        msg = ElectMaster(ct_addr=host, ct_port=port)
        self.ipc.send(RFMONITOR_RFPROXY_CHANNEL, str(0), msg)


class Monitor(object):
    """Monitors each controller individually"""
    def __init__(self, host, port, callback_time=1000):
        """Initialize Monitor

        Keyword Arguments:
        host -- host ip address at which controller is listening.
        port -- port at which the controller is listening at `host` address.
        test -- callback function to be called periodically.
        callback_time -- time interval (in milliseconds) at which `test` is run.

        """
        super(Monitor, self).__init__()
        self.host = host
        self.port = port
        self.callback_time = callback_time
        self.timeout = time.time()
        self.schedule_test()

    def schedule_test(self):
        """Schedule the next test"""
        current_time = time.time()
        if self.timeout <= current_time:
            self.timeout += self.callback_time/1000.00


if __name__ == "__main__":
    description = 'RFMonitor monitors RFProxy instances for failiure'
    epilog = 'Report bugs to: https://github.com/routeflow/RouteFlow/issues'
    RFMonitor()
