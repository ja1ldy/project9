#!/bin/sh
# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

if [ -r /etc/fvwmrc ]; then
    exec fvwm -f /etc/fvwmrc
fi
exec fvwm
