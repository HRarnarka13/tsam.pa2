#!/bin/bash
echo "Running HTTP server on port: `/labs/tsam15/my_port`. Hit Ctrl-C to quit"
./src/httpd $(/labs/tsam15/my_port)
