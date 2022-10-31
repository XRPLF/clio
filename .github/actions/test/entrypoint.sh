#!/bin/bash

pg_ctlcluster 12 main start
su postgres -c"psql -c\"alter user postgres with password 'postgres'\""
su cassandra -c "/opt/cassandra/bin/cassandra -R"
sleep 90
chmod +x ./clio_tests
./clio_tests
