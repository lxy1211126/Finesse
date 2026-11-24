# Destor Bridge Status

`destor_bridge.h` exposes a C ABI for obtaining chunk ranges from the
independent C++ Rabin chunker. It is compiled and tested from a C caller.

The Destor executable was fetched and built locally after adapting its
Autotools generation and supplying compatibility flags for Ubuntu 22.04,
GLib 2.72, OpenSSL 3, and GCC's default `-fno-common`. It runs its help path.
The bridge is not wired into Destor's full backup pipeline: Destor's Rabin
chunker carries rolling-window state between returned chunks, while the
standalone implementation resets the rolling hash per chunk. Replacing that
path directly would change chunk boundaries. The independent experiment
pipeline therefore remains authoritative; Destor integration is intentionally
left optional.

Destor is optional and is not included in this repository. To reproduce the
local integration checkout, clone it separately first:

```bash
git clone https://github.com/fomy/destor.git third_party/destor
```

The build workaround used for this checkout was:

```bash
cd third_party/destor
mkdir -p .deps/compat
ln -sf /usr/lib/x86_64-linux-gnu/libglib-2.0.so .deps/compat/libglib.so
autoreconf -fi
CPPFLAGS="$(pkg-config --cflags glib-2.0)" \
LDFLAGS="-L$PWD/.deps/compat" \
ac_cv_lib_crypto_SHA_Update=yes \
./configure --prefix="$PWD/install"
make clean
make -j2 CFLAGS="-g -O2 -Wall -fcommon \
  -DSHA_Init=SHA1_Init -DSHA_Update=SHA1_Update -DSHA_Final=SHA1_Final"
LD_LIBRARY_PATH="$PWD/.deps/compat" ./destor -h
```
