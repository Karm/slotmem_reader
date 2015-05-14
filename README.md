slotmem_reader
==============

A tiny utility for reading mod_cluster shm slotmem files.


Usage
=====

    gcc reader.c -o reader  $(apr-1-config --cflags --cppflags --includes --link-ld) -IMOD_CLUSTER_SOURCES/native/include/ -IMOD_CLUSTER_SOURCES/native/mod_manager/   -IHTTPD_BUILD_DIRECTORY/include/ -Wall -std=c99

    ./reader  HTTPD_ROOT/cache/mod_cluster/manager.host.hosts.slotmem

Supported versions
==================

Please, note that this utility compiles and works with mod_cluster 1.2.x and 1.3.x.
