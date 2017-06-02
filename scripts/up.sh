#!/bin/sh

_daemon="dbnsd"

if [ -x $(pgrep "$_daemon") ]; then
    doas rcctl start dbns 
fi
