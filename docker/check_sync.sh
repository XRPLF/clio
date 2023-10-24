#!/usr/bin/bash 

if [[ $(curl 127.0.0.1:5005 -sd'{"method": "server_info"}' | grep -o '"complete_ledgers":"[^"]*' | grep -o '[^"]*$') != "empty" ]]; then
  echo "ready"
else
  echo "Not ready!"
  exit 1
fi
