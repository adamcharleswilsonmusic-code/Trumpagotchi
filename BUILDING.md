# Build Trumpagotchi

Prebuilt users should follow [README.md](README.md#download-and-install). Source builds require Python 3, a C compiler and Git. The target compiler is Linux x86-64; other hosts need Docker running.

Run from this repository’s root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-build.txt
sh tools/get_toolchain.sh "$PWD/tooling/jieli"
git clone --branch AC79NN_SDK_V1.2.1_2023-12-13 --depth 1 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK tooling/ac79-sdk
export JIELI_TOOLCHAIN="$PWD/tooling/jieli/toolchain"
export AC79_SDK="$PWD/tooling/ac79-sdk"
export PYTHON="$PWD/.venv/bin/python"
export X0X_FONTSET=terminus
export JIELI_DOCKER_IMAGE='debian:bookworm-slim@sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587'
python tools/import_voice_samples.py
sh tests/run_tests.sh
sh build.sh
```

Output: `build/trumpagotchi-preview.fwsc`, identity **FM-1_710**, plus `build/omni.bin` and `build/loader/ota.bin`. Builds and tests do not install or access hardware. The published firmware is the tested frozen build, renamed for downloading.

The tested compiler was jieli-linux-toolchains-20260730.1; the official download endpoint can change. SDK commit: d179b4484759423312073f5fbb232501aa491047. The build checks the packaged SDK files, application and RAM capacity, RAM-only flash routines and hardware access boundaries.

For the native simulator, use `sh host/build_host.sh`. For a browser preview, install Emscripten 4.0.23, source its emsdk_env.sh and run `sh web/emu/build.sh`, then `python3 -m http.server 8792 --bind 127.0.0.1 --directory build/emu`. The preview does not simulate USB installation or establish hardware timing.

The pet, audio, input, storage and animation tests are under tests/pet; the retained installer/loader/platform tests are also included. Historical unused Fart Machine One audio and its dedicated tests are omitted from this release.
