#!/bin/sh

while true; do
  echo load -j examples/csip_load | socat - UNIX-CONNECT:../otdb.sock;
  sleep 1;
done
