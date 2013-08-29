#!/usr/bin/env python
#-*- coding:utf-8 -*-

import logging
import threading
import time
import socket

import rflib.ipc.IPC as IPC
import rflib.ipc.MongoIPC as MongoIPC
from rflib.ipc.RFProtocol import *
from rflib.ipc.RFProtocolFactory import RFProtocolFactory
from rflib.defs import *
from rflib.types.Match import *
from rflib.types.Action import *
from rflib.types.Option import *


class RFMonitor(RFProtocolFactory, IPC.IPCMessageProcessor):
    """Monitors all the controller instances for failiure"""
    def __init__(self, *arg, **kwargs):
        self.controllers = dict()
        self.monitors = dict()
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
        if type_ == CONTROLLER_REGISTER:
            self.controllerLock.acquire()
            try:
                self.controllers[msg.get_ct_addr() + ':'
                                 + str(msg.get_ct_port())] = msg.get_ct_role()
                self.log.info("A %s controller at %s:%s is up",
                              msg.get_ct_role(), msg.get_ct_addr(),
                              msg.get_ct_port())
            finally:
                self.controllerLock.release()

    def test_controllers(self):
        while True:
            self.controllerLock.acquire()
            try:
                controllers = self.controllers.keys()
            finally:
                self.controllerLock.release()
            for controller in self.controllers:
                host, port = controller.split(':')
                port = int(port)
                if controller in self.monitors:
                    monitor = self.monitors[controller]
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
        self.controllerLock.acquire()
        try:
            self.controllers.pop(host + ':' + str(port), None)
            self.monitors[host + ':' + str(port)].stop_test()
            self.monitors.pop(host + ':' + str(port), None)
        finally:
            self.controllerLock.release()
            
    def stop_monitors(self):
        for x in self.monitors:
            x.stop_test()


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
        self.running = True
        self.timeout = time.time()
        self.schedule_test()

    def start_test(self):
        self.running = True
        self.timeout = time.time()
        self.schedule_test()

    def stop_test(self):
        self.running = False

    def schedule_test(self):
        current_time = time.time()
        if self.timeout <= current_time:
            self.timeout += self.callback_time/1000.00


if __name__ == "__main__":
    description = 'RFMonitor monitors RFProxy instances for failiure'
    epilog = 'Report bugs to: https://github.com/routeflow/RouteFlow/issues'
    RFMonitor()
