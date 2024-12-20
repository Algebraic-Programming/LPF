
To build a simple NOC example, follow the usual LPF make procedure and then
compile the standalone test manually:

```
g++ -I./include/ tests/functional/noc-udp-standalone.cpp /tmp/lpfinstall/lib/libnoc_standalone_udp.a
```

After compilation, one may run the test to verify the functionality of the
standalone non-coherent RDMA extension.

