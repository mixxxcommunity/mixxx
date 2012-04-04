#!/bin/bash
# This script ceates the links to the clementine tagreader

mkdir include 

ln -s ../../daschuer-mixxx-clementine/bin/clementine-tagreader ../../clementine-tagreader
ln -s ../../../../daschuer-mixxx-clementine/ext/libclementine-tagreaderclient/build/libclementine-tagreaderclient.so libclementine-tagreaderclient.so

ln -s ../../../../../daschuer-mixxx-clementine/src/core/tagreaderclient.h include/tagreaderclient.h

