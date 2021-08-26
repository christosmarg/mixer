#!/bin/sh

cd mixer_lib && doas make all install clean cleandepend && cd ..
cd mixer_prog && doas make all install clean cleandepend && cd ..
