# Example of clio monitoring infrastructure

This directory contains an example of docker based infrastructure to collect and visualise metrics from clio.

The structure of the directory:
- `compose.yaml`
   Docker-compose file with Prometheus and Grafana set up.
- `prometheus.yaml`
  Defines metrics collection from Clio and Prometheus itself.
  Demonstrates how to setup Clio target and Clio's admin authorisation in Prometheus.
- `grafana/clio_dashboard.json`
  Json file containing preconfigured dashboard in Grafana format.
- `grafana/dashboard_local.yaml`
  Grafana configuration file defining the directory to search for dashboards json files.
- `grafana/datasources.yaml`
  Grafana configuration file defining Prometheus as a data source for Grafana.

## How to try

1. Make sure you have `docker` and `docker-compose` installed.
2. Run `docker-compose up -d` from this directory. It will start docker containers with Prometheus and Grafana.
3. Open [http://localhost:3000/dashboards](http://localhost:3000/dashboards). Grafana login `admin`, password `grafana`.
There will be preconfigured Clio dashboard.

If Clio is not running yet launch Clio to see metrics. Some of the metrics may appear only after requests to Clio.
