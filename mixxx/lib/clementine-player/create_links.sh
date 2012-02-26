#!/bin/bash
# This script ceates the links to the clementine Player headers and libraries 

ln -s ../../../../daschuer-mixxx-clementine/src src
ln -s ../../../../daschuer-mixxx-clementine/bin/src/libclementine_lib.a libclementine_lib.a
ln -s ../../../../daschuer-mixxx-clementine/bin/ext/libclementine-common/liblibclementine-common.a liblibclementine-common.a
ln -s ../../../../daschuer-mixxx-clementine/bin/3rdparty/sha2/libsha2.a libsha2.a
ln -s ../../../../daschuer-mixxx-clementine/bin/3rdparty/universalchardet/libchardet.a libchardet.a
ln -s ../../../../daschuer-mixxx-clementine/bin/ext/libclementine-spotifyblob/libclementine-spotifyblob-messages.a libclementine-spotifyblob-messages.a
ln -s ../../../../daschuer-mixxx-clementine/bin/3rdparty/chromaprint/src/libchromaprint_p.a libchromaprint_p.a
ln -s ../../../../daschuer-mixxx-clementine/bin/3rdparty/qtiocompressor/libqtiocompressor.a libqtiocompressor.a
ln -s ../../../../daschuer-mixxx-clementine/bin/gst/afcsrc/libgstafcsrc.a libgstafcsrc.a
ln -s ../../../../daschuer-mixxx-clementine/bin/ext/libclementine-tagreader/liblibclementine-tagreader.a liblibclementine-tagreader.a

mkdir include 
ln -s ../../../../../daschuer-mixxx-clementine/bin/src/config.h  include/config.h
ln -s ../../../../../daschuer-mixxx-clementine/bin/ext/libclementine-tagreader/tagreadermessages.pb.h  include/tagreadermessages.pb.h

