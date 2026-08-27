---
layout: default
title: Package Manager
nav_order: 3
permalink: /package-manager/
---

# Orion Package Manager

The `orion` package manager installs Orion applications independently from
GitHub Release artifacts. Its commands follow the familiar Homebrew model while
keeping the implementation small, auditable, and usable from POSIX shells.

## Install Orion

Bootstrap the package manager and shared runtime on macOS or Linux:

```sh
curl -fsSL https://raw.githubusercontent.com/corepunch/orion-ui/main/install.sh | sudo sh
export PATH="/opt/orion/bin:$PATH"
```

The default prefix is `/opt/orion`. For an installation owned by the current
user, no administrator permission is required:

```sh
curl -fsSL https://raw.githubusercontent.com/corepunch/orion-ui/main/install.sh |
  ORION_PREFIX="$HOME/.local" sh
export PATH="$HOME/.local/bin:$PATH"
```

Set `ORION_PREFIX` on later commands when using a non-default prefix:

```sh
ORION_PREFIX="$HOME/.local" orion install scener
```

## Commands

```sh
# Browse available applications and tools
orion search
orion search editor
orion info scener

# Install one or more packages
orion install scener
orion install imageeditor terminal

# Inspect and maintain installed packages
orion list
orion update
orion uninstall scener

# Package-manager version and command summary
orion --version
orion --help
```

`orion remove` is an alias for `orion uninstall`, and `orion upgrade` is an
alias for `orion update`.

## Package Model

`orion-core` is installed during bootstrap and contains:

- The `orion` package-manager executable.
- Orion runtime libraries.
- Shared framework assets.
- Core offline documentation.

Every application and developer tool is a separate release archive. An
application package can contain its standalone executable, loadable GEM,
component plugin, application assets, and documentation. Installing one
application therefore does not install the complete suite.

Dependencies are resolved from the release index. For example,
`orion install shell` also installs the FileManager and Terminal packages that
the Shell loads as GEMs.

Package receipts are stored under `<prefix>/.orion/installed`. They record the
installed version and owned files so packages can be listed, updated, and
removed without deleting files owned by another package.

## Security And Releases

Before extracting an archive, `orion` verifies its SHA-256 checksum against the
platform package index and rejects archive paths that are absolute or contain
parent-directory traversal.

Tagged releases publish separate packages for each supported platform:

| Platform | Identifier |
|---|---|
| macOS on Apple silicon | `macos-arm64` |
| Linux on x86-64 | `linux-x86_64` |

The default channel follows the latest GitHub release. Pin a specific release
when reproducibility is required:

```sh
ORION_RELEASE=v1.0.0 orion install scener
```

The release page contains one archive per package and platform, plus a
checksummed `packages-<platform>.tsv` index. The first published release is
[Orion v1.0.0](https://github.com/corepunch/orion-ui/releases/tag/v1.0.0).

## Environment Variables

| Variable | Purpose | Default |
|---|---|---|
| `ORION_PREFIX` | Installation root | `/opt/orion` |
| `ORION_RELEASE` | Release tag or `latest` | `latest` |
| `ORION_REPOSITORY` | GitHub owner and repository | `corepunch/orion-ui` |
| `ORION_RELEASE_BASE` | Complete release asset URL override | unset |

`ORION_RELEASE_BASE` is useful for mirrors and offline test servers. When set,
it takes precedence over `ORION_RELEASE` and `ORION_REPOSITORY`.

## Installed Layout

```text
<prefix>/bin/                 package manager, applications, and tools
<prefix>/lib/                 runtime libraries and component plugins
<prefix>/lib/orion/gems/      loadable Orion applications
<prefix>/share/orion/         framework assets
<prefix>/share/<app>/         application assets
<prefix>/share/doc/orion/     framework and application documentation
<prefix>/.orion/installed/    package receipts
```

## Building Release Packages

The release workflow is defined in `.github/workflows/release.yml`. A `v*` tag
builds macOS and Linux packages, uploads the matrix artifacts, creates the
corresponding GitHub Release, and attaches each package separately.

Release archives can be generated locally after building the repository:

```sh
sh scripts/package-release.sh 1.0.0 macos-arm64 dist
```

The package catalog is `packaging/packages.tsv`. Each row declares the package
name, description, and optional dependencies. `scripts/package-release.sh`
stages the normal `make install` output and splits it according to that catalog.

## Installing The Complete Suite From Source

The package manager is intended for released, on-demand installations. To
install every application, GEM, tool, runtime library, asset, and document from
a source checkout instead, use:

```sh
make all
sudo make install
```

For a user-local source installation:

```sh
make install PREFIX="$HOME/.local"
```