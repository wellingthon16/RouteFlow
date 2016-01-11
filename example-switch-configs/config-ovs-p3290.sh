#!/bin/sh

# non-fastpath minimal configuration for Pica8 3290
# known to work at Picos 2.6.4

CTLIP=192.168.3.5
DPID=0000000000000099
BR=br0

ovs-vsctl del-br $BR
ovs-vsctl add-br $BR -- set bridge $BR datapath_type=pica8
ovs-vsctl set bridge $BR datapath_type=pica8
ovs-vsctl add-port $BR ge-1/1/1 -- set Interface ge-1/1/1 type=pica8
ovs-vsctl add-port $BR ge-1/1/2 -- set Interface ge-1/1/2 type=pica8
ovs-vsctl set bridge $BR other-config:datapath-id=$DPID
ovs-vsctl set-controller $BR tcp:$CTLIP:6653
