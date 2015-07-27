#!/bin/bash

LD_PRELOAD=./libfitter_C.so ./voltlogger_analyzer "$@"

