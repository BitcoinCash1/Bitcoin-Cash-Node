# Gitian building with Docker

This is a streamlined guide for running Gitian builds with Docker on Ubuntu,
Debian or Mac hardware.

## Setup

Ensure you have Docker installed.  See <https://docs.docker.com/get-docker/> for
installation instructions.

If you're using Linux, make sure to go through the Linux post-install
walkthrough, especially the
[Manage Docker as a non-root user](https://docs.docker.com/engine/install/linux-postinstall/#manage-docker-as-a-non-root-user)
section, to avoid having to use `sudo` all the time.

### Prepare a build workspace

Create a workspace directory (e.g. `~/bchn-gitian`) and `cd` into it.  You'll
only need to run through this setup once so long as you retain the workspace.

### Install dependencies

You'll need `git` and `curl` installed.  If on Ubuntu for instance, just do `sudo apt install
curl git`.

### Gitian setup

```bash
# Fetch the source code
git clone https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node.git

# Run the initial Gitian setup
cp bitcoin-cash-node/contrib/gitian-build.py .
./gitian-build.py --docker --setup

# If you need to build for MacOS, also fetch this archive which has been
# extracted from the free SDK.
mkdir -p gitian-builder/inputs
(cd gitian-builder/inputs
curl -LO https://github.com/joseluisq/macosx-sdks/releases/download/14.5/MacOSX14.5.sdk.tar.xz
echo "6e146275d19f027faa2e8354da5e0267513abf013b8f16ad65a231653a2b1c5d MacOSX14.5.sdk.tar.xz" | sha256sum -c
# This should echo "MacOSX14.5.sdk.tar.xz: OK"
```

## Build binaries

Finally, run the build process. Replace `23.1.0` (without the "v") with the version you want to build.

```bash
./gitian-build.py --docker --detach-sign --no-commit -b satoshi 23.1.0
```

See the [Verify hashes](../gitian-building.md#verify-hashes) section of the
main Gitian build guide for build verification and signing instructions.
