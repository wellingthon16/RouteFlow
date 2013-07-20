#!/usr/bin/env python
#-*- coding:utf-8 -*-

import rflib.ipc.IPC as IPC
from rflib.ipc.RFProtocol import *
from rflib.ipc.RFProtocolFactory import RFProtocolFactory
from rflib.types.Match import *
from rflib.types.Action import *
from rflib.types.Option import *


class RFMonitor(RFProtocolFactory, IPC.IPCMessageProcessor):
    """Monitors all the controller instances for failiure"""
    def __init__(self, *arg, **kwargs):
        super(RFMonitor, self).__init__(*args, **kwargs)

    def process():
        pass

    def test():
        pass


class Monitor(object):
    """Monitors each controller individually"""
    def __init__(self, host, port, callback_time):
        super(Monitor, self).__init__()
        self.host = host
        self.port = port
        self.callback_time = callback_time

    def start_test():
        pass

    def stop_test():
        pass

    def schedule_test():
        pass

    def run_test():
        pass
