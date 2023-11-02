#!/usr/bin/bash

if [[ $(/opt/ripple/bin/rippled --silent --conf /opt/ripple/etc/rippled.cfg server_info | grep -o '"complete_ledgers" : "[^"]*' | grep -o '[^"]*$') != "empty" ]]; then
  echo "ready"
else
  echo "Not ready!"
  exit 1
fi
