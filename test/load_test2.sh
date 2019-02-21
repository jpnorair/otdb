#!/bin/sh

while true; do
  echo save -j testsave | socat - UNIX-CONNECT:../otdb.sock;
  sleep 1;
done
