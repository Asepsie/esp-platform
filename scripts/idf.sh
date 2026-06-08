#!/bin/bash
# ESP-IDF wrapper — sources environment then passes all args to idf.py
# Use this instead of calling idf.py directly in all contexts.
source ~/esp/esp-idf/export.sh > /dev/null 2>&1
exec idf.py "$@"
