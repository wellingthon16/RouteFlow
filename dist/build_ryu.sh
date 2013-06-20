#!/bin/sh

RYU_GIT="https://github.com/routeflow/ryu-rfproxy.git"
RYU_BRANCH="origin/master"

RYU_DEPS="python-gevent python-webob python-routes"

# XXX: Lock ryu at v1.9 until it works with later versions
RYU_PIP_URL="https://pypi.python.org/packages/source/r/ryu/ryu-1.9.tar.gz#md5=9cea02610b9b2e1741297c1756a2378d"

get_ryu() {
    if [ "$OVS_VERSION" != "git" ]; then
        print_status "We recommend using ovs-1.10.0 or above with Ryu for OF1.2+ Support" \
            $YELLOW
    fi

    pkg_install "$MONGO_DEPS"
    print_status "Fetching Ryu controller"

    if [ $FETCH_ONLY -ne 1 ]; then
        $SUPER pip install oslo.config $RYU_PIP_URL ||
            fail "Failed to fetch ryu controller"
    fi

    fetch "ryu-" "rfproxy" $RYU_GIT $RYU_BRANCH ||
        fail "Couldn't fetch ryu-rfproxy"
    $DO cd -
}
