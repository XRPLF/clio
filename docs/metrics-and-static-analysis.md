# Metrics and static analysis

## Prometheus metrics collection

Clio natively supports [Prometheus](https://prometheus.io/) metrics collection. It accepts Prometheus requests on the port configured in the `server` section of the config.

Prometheus metrics are enabled by default, and replies to `/metrics` are compressed. To disable compression, and have human readable metrics, add `"prometheus": { "enabled": true, "compress_reply": false }` to Clio's config.

To completely disable Prometheus metrics add `"prometheus": { "enabled": false }` to Clio's config.

It is important to know that Clio responds to Prometheus request only if they are admin requests. If you are using the admin password feature, the same password should be provided in the Authorization header of Prometheus requests.

You can find an example docker-compose file, with Prometheus and Grafana configs, in [examples/infrastructure](../examples/infrastructure/).

## Using `clang-tidy` for static analysis

The minimum [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) version required is 17.0.

Clang-tidy can be run by Cmake when building the project. To achieve this, you just need to provide the option `-o lint=True` for the `conan install` command:

```sh
conan install .. --output-folder . --build missing --settings build_type=Release -o tests=True -o lint=True
```

By default Cmake will try to find `clang-tidy` automatically in your system.
To force Cmake to use your desired binary, set the `CLIO_CLANG_TIDY_BIN` environment variable to the path of the `clang-tidy` binary. For example:

```sh
export CLIO_CLANG_TIDY_BIN=/opt/homebrew/opt/llvm@17/bin/clang-tidy
```
