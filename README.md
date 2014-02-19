# RouteFlow
RouteFlow is a platform for providing virtualized IP routing services on top of 
OpenFlow networks.

You can learn more about RouteFlow in our
[page in GitHub](http://routeflow.github.io/RouteFlow/), in the
[wiki](https://github.com/routeflow/RouteFlow/wiki) and in our 
[website](https://sites.google.com/site/routeflow/).

RouteFlow depends on many different pieces of software to work; please be aware 
of POX, OpenFlow, Open vSwitch, Quagga, MongoDB, jQuery, JIT and RouteFlow 
licenses and terms.

# Distribution overview
The RouteFlow core is composed by three basic applications: RFClient, RFServer 
and RFProxy.

* RFClient runs as a daemon in the Virtual Machine (VM), detecting changes in 
the Linux ARP and routing tables. Routing information is sent to the RFServer 
when there's an update.

* RFServer is a standalone application that connects to and manages the 
RFClient instances running in VMs. The RFServer keeps the mapping between the 
RFClient instances and interfaces and the corresponding switches and ports. It 
connects to RFProxy to configure flows mapped from the virtual environment.

* RFProxy is an application (for POX and other controllers) responsible for the 
interactions with the OpenFlow switches (identified by datapaths) via the 
OpenFlow protocol. It listens to instructions from the RFServer and notifies it 
about events in the network. We recommend running POX when you are 
experimenting and testing your network. Other implementations in different 
controllers are available using the `build.sh` script.

High-availability is possible through the use of the optional RFMonitor module.

There is also a library of common functions (`rflib`). It has implementations 
of the IPC, utilities like custom types for IP and MAC addresses manipulation 
and OpenFlow message creation.

Additionally, there's `rfweb`, an extra module that provides an web interface 
for RouteFlow.


# Building

> If you just want to get started, follow these first steps. If you're 
> developing or diving deeper, there are more advanced options, modules and 
> newer versions of dependencies that can be installed through the `build.sh` 
> script. See its 
> [source](https://github.com/routeflow/RouteFlow/blob/master/build.sh) for 
> more information.

1. Install the dependencies (we strongly recommend Ubuntu 12.04):
```
sudo apt-get install build-essential git libboost-dev \
  libboost-program-options-dev libboost-thread-dev \
  libboost-filesystem-dev iproute-dev openvswitch-switch \
  mongodb python-pymongo
```

2. Clone RouteFlow's repository on GitHub:
```
$ git clone git://github.com/routeflow/RouteFlow.git
```

3. Build `rfclient`
```
make rfclient
```

That's it! Now you can run tests 1 and 2. The setup to run them is described in 
the "Running" section.


# Running
The folder rftest contains all that is needed to create and run two test cases.

## Virtual environment
First, create the default LXC containers that will run as virtual machines:
```
$ cd rftest
$ sudo ./create
```
The containers will have a default ubuntu/ubuntu user/password combination. 
**You should change that if you plan to deploy RouteFlow**.

By default, the tests below will use the LXC containers created  by the 
`create` script. You can use other virtualization technologies. If you have 
experience with or questions about setting up RouteFlow on a particular 
technology, contact us! See the "Support" section.


## Test scenarios

Default configuration files are provided for these scenarios in the `rftest` 
directory (you don't need to change anything).
You can stops them at any time by pressing CTRL+C.

### rftest1

> For a description of this scenario, see its 
> [tutorial](https://github.com/routeflow/RouteFlow/wiki/Tutorial-1:-rftest1).

1. Run:
```
$ sudo ./rftest1
```

2. You can then log in to the LXC container b1 and try to ping b2:
```
$ sudo lxc-console -n b1
```

3. Inside b1, run:
```
# ping 172.31.2.2
```

### rftest2

> For a description of this scenario, see its 
> [tutorial](https://github.com/routeflow/RouteFlow/wiki/Tutorial-2:-rftest2).

This test should be run with a [Mininet](http://mininet.org/) simulated 
network.
In the steps below, replace [guest address] with the IP address you use to 
access your Mininet VM.
The same applies to [host address], that should be the address to access the 
host from inside the VM.

1. Run:
```
$ sudo ./rftest2
```

2. Once you have a Mininet VM up and running, copy the network topology files 
in rftest to the VM:
```
$ scp topo-4sw-4host.py mininet@[guest address]:/home/mininet/mininet/custom
$ scp ipconf mininet@[guest address]:/home/mininet
```

3. Then start the network:
```
$ sudo mn --custom mininet/custom/topo-4sw-4host.py --topo=rftest2 --controller=remote,ip=[host address],port=6633 --pre=ipconf
```

Wait for the network to converge (it should take a few seconds), and try to 
ping:
```
mininet> pingall
...
mininet> h2 ping h3
```


# Now what?
If you want to use the web interface to inspect RouteFlow behavior, see the 
wiki page on 
[rfweb](https://github.com/routeflow/RouteFlow/wiki/The-web-interface).

If you want to create your custom configurations schemes for a given setup, 
check out the 
[configuration section of the first tutorial](https://github.com/routeflow/RouteFlow/wiki/Tutorial-1:-rftest1#configuration-file) 
and the guide on 
[how to create your virtual environment](https://github.com/routeflow/RouteFlow/wiki/Virtual-environment-creation).

See the `build.sh` script for advanced options. You'll probably want to use it 
if you are doing research or exploring some new feature. See its 
[source](https://github.com/routeflow/RouteFlow/blob/master/build.sh) for more 
information.


# Support
If you want to know more or need to contact us regarding the project for 
anything (questions, suggestions, bug reports, discussions about RouteFlow and 
SDN in general) you can use the following resources:
* RouteFlow repository [wiki](https://github.com/routeflow/RouteFlow/wiki) and 
[issues](https://github.com/routeflow/RouteFlow/issues) in GitHub

* Google Groups 
[mailing list](http://groups.google.com/group/routeflow-discuss?hl=en_US)


_RouteFlow - Copyright (c) CPqD_
