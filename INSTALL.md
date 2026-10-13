# Installation

Follow the [README’s download-to-install steps](README.md#download-and-install). Use the firmware and source ZIP from the same release. The installer requires Python 3, mido and python-rtmidi, pinned in requirements-install.txt.

After activating the installation environment, connect the FM-1 and run:

```sh
python tools/fm1_install.py --info
python tools/fm1_install.py trumpagotchi.fwsc
```

The first command only reads the device identity. The second asks for confirmation before installing. Keep USB connected until the write and restart finish. Expected identity: **FM-1_710**. The [quick-start guide](docs/trumpagotchi-quick-start.pdf) covers playing and care.
