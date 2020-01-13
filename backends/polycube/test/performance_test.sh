#!/bin/bash

function cleanup {
	set +e
	for i in `seq 1 2`;
  	do
  		sudo ip link del veth${i}
  		sudo ip netns del ns${i}
  	done
}

if [ $# -ne 2 ]
then
	echo "two arguments required: '-i' for internal network or '-e' for two different networks and how many times to run the test"
	exit 1
fi

re='^[0-9]+$'
if ! [[ $2 =~ $re ]] ; then
   echo "second argument must be a positive integer"
   exit 1
fi

if [ $1 == '-i' ]
then
	echo "internal network"
	prefix=16;
	text="Namespaces created: deploy the service and connect to veth1 (10.0.1.1/16) and veth2 (10.0.2.1/16). Then press any key to make the performance test start..."
elif [ $1 == '-e' ]
then
	echo "two different networks"
	prefix=24
	text="Namespaces created: deploy the service and connect to veth1 (10.0.1.1/24, default gw= 10.0.1.254/24) and veth2 (10.0.2.1/24, default gw=10.0.2.254/24). Then press any key to make the performance test start..."
else
	echo "wrong flags: '-i' for internal network or '-e' for two different networks"
	exit 1
fi

trap cleanup EXIT

set -x
set -e

for i in `seq 1 2`;
  do
  	sudo ip netns add ns${i}
  	sudo ip link add veth${i}_ type veth peer name veth${i}
  	sudo ip link set veth${i}_ netns ns${i}
  	sudo ip netns exec ns${i} ip link set dev veth${i}_ up
  	sudo ip link set dev veth${i} up
        sudo ip netns exec ns${i} ifconfig veth${i}_ 10.0.${i}.1/$prefix
        sudo ip netns exec ns${i} route add default gw 10.0.${i}.254 veth${i}_
  done

read -n 1 -s -r -p "$text"

sudo ip netns exec ns1 iperf -s&
sleep 3
for i in `seq 1 $2`;
do
	sudo ip netns exec ns2 iperf -c 10.0.1.1
done
