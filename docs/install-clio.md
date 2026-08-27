# How to install Clio

Clio is published as a Debian package for 64-bit x86 Linux, in the XRPL Foundation repositories at <https://packages.xrplf.org>. To build from source instead, see [How to build Clio](./build-clio.md).

## Install

Clio publishes to the same repositories as [`xrpld`](https://github.com/XRPLF/rippled), so configure APT by following [its instructions](https://github.com/XRPLF/rippled/blob/develop/docs/install.md#with-the-apt-package-manager), which also document the [release channels](https://github.com/XRPLF/rippled/blob/develop/docs/install.md#release-channels), and install `clio` in place of `xrpld`:

```bash
sudo apt -y install clio
```

## The clio service

The package installs `/opt/clio/bin/clio_server` (symlinked into `/usr/bin`), a config at `/opt/clio/etc/config.json`, a log directory at `/var/log/clio`, and a systemd unit, all owned by the `clio` system user it creates.

The unit is not enabled, as Clio needs a configured database and `xrpld` node before it can start. Edit the config — see [How to configure Clio and xrpld](./configure-clio.md) — then:

```bash
sudo systemctl enable --now clio
```

Upgrades do not restart a running server; `systemctl restart clio` picks up the new binary.

## Publishing

Only tags are published. CPack builds the package in the `package` job of [`release.yml`](../.github/workflows/release.yml), and [`reusable-publish-package.yml`](../.github/workflows/reusable-publish-package.yml) uploads it with `publish_pkg.py` from the [`xrpld` packaging image](https://github.com/XRPLF/rippled/blob/develop/package/README.md#publishing-packages), so that Clio and `xrpld` agree on what a channel means. [`XRPLF/actions/release-info`](https://github.com/XRPLF/actions/blob/main/release-info/action.yml) picks the channel:

| Tag         | Package version | Channel  |
| ----------- | --------------- | -------- |
| `X.Y.Z`     | `X.Y.Z`         | `stable` |
| `X.Y.Z-rcN` | `X.Y.Z~rcN`     | `rc`     |
| `X.Y.Z-bN`  | `X.Y.Z~bN`      | `beta`   |

[`cmake/pkg/deb.cmake`](../cmake/pkg/deb.cmake) converts the `-` to `~`, which Debian sorts below everything, so `X.Y.Z~rc1` ranks below the `X.Y.Z` it precedes.
