Performance test
================

Tools needed
------------

The tools used to test the performance of a service are the linux namespaces
and `iperf`. Two namespaces host respectively a server and a client for iperf,
which will test the throughput of the connection between them generating
 traffic for ten seconds.

How to test performance
-----------------------

In order to test the performance of a service,use the script `performance_test.sh`
in the test folder, indicating whether you need the two hosts in the same network (`-i` flag)
or in two different networks (`-e` flag) and how many times you want to run the test.

Afterwards, the script will pause, waiting for you to create and setup
the service you want to test and to connect two ports to the links of the 
namespaces (veth1 and veth2). Once finished, signal the script and it will start 
testing the performance, by starting an `iperf` server in the first namespace 
and running the client in the second one. The results will be printed after each
run.

Example: test simplebridge performance
- Create two namespaces in the same network and run iperf 5 times.
```
./performance_test.sh -i 5
```
- When the script is paused, on another terminal run
```
polycubectl simplebridge add br1 type=TC

polycubectl simplebridge br1 ports add port1
polycubectl simplebridge br1 ports add port2

polycubectl connect br1:port1 veth1
polycubectl connect br1:port2 veth2
```
- press a key in the terminal of the first script and wait for the results