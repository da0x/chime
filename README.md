# chime

Run a command, and chime when it ends.

```sh
chime make -j16
```

Start a long build, deploy, backup or upload, look away, and `chime` tells you when it's
done. It plays your desktop's *complete* sound when the command succeeded and its error
sound when it didn't, rings the terminal's bell, and shows a notification saying how the
command ended and how long it took:

```
make -j16 finished
in 4m10s
```

It exits with the command's own status, so you can put it in front of anything, in a
script or an `&&` chain, without changing what happens next.

## Usage

```
chime [--name JOB] command [argument...]
chime [--error] [--message TEXT]
```

| Option | What it does |
|---|---|
| `--name JOB` | Names the job in the notification: "Deploy finished", or "Deploy failed with 1" |
| `--message TEXT` | What the notification says when chiming at once |
| `--error` | Chimes the error sound when chiming at once |
| `--` | The command starts here, even if it starts with a dash |
| `-h`, `--help` | Shows the options |
| `--version` | Shows the version |

Without a command, `chime` chimes at once.

### Examples

```sh
chime make -j16                                   # build, chime when it's done
chime --name "Backup" rsync -a ~/ /mnt/backup/    # "Backup finished"
chime -- ./deploy --yes                           # a command whose arguments look like options
make && chime --message "Built" || chime --error --message "Build failed"
```

### How it ends

| The command | The notification says | chime exits with |
|---|---|---|
| exits 0 | `… finished` | 0 |
| exits with *n* | `… failed with n` | *n* |
| is stopped by a signal | `… stopped by SIGINT` | 128 + the signal's number |
| can't be found | `… failed with 127` | 127 |
| can't be run | `… failed with 126` | 126 |

Ctrl-C goes to the command, not to `chime`, which still reports how it ended.

## Sounds and notifications

`chime` uses whatever the system has, in this order:

1. `canberra-gtk-play`, playing your desktop's sound theme, so the chime is the one your
   desktop already uses;
2. the freedesktop sound theme's file, played with `pw-play`, `paplay` or `aplay`;
3. the terminal's bell, always.

The notification is sent with `notify-send`. All of these are optional. Each is given a
second at most, so a stalled audio server or notification daemon never holds `chime`
up, and none of them failing changes its exit status.

## Installing

### From source

It needs a C++23 compiler with `<print>` (GCC 14 or newer) and CMake 3.25 or newer.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build          # optional: runs tests/run.sh, which plays no sound
sudo cmake --install build      # /usr/local/bin/chime and its man page
```

`cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local` installs it for yourself
alone. `chime(1)` is the manual.

### For packagers

- **Build:** CMake with `GNUInstallDirs`; `DESTDIR` and `CMAKE_INSTALL_PREFIX` work as
  usual. It installs `bin/chime`, `share/man/man1/chime.1` and
  `share/doc/chime/README.md`.
- **Tests:** `ctest` runs `tests/run.sh`, which needs only `/bin/sh` and plays no sound.
  Turn them off with `-DBUILD_TESTING=OFF`.
- **Runtime dependencies:** none are required. Each of these adds to it:

  | What | Arch | Debian, Ubuntu | Fedora |
  |---|---|---|---|
  | The desktop's sound theme | `libcanberra` | `gnome-session-canberra` | `libcanberra-gtk3` |
  | The sound files | `sound-theme-freedesktop` | `sound-theme-freedesktop` | `sound-theme-freedesktop` |
  | A player | `pipewire` or `libpulse` or `alsa-utils` | `pipewire-bin` or `pulseaudio-utils` or `alsa-utils` | `pipewire-utils` or `pulseaudio-utils` or `alsa-utils` |
  | Notifications | `libnotify` | `libnotify-bin` | `libnotify` |

- **Portability:** it uses only POSIX and the C++ standard library. Signal names come
  from glibc 2.32 or newer, with a fallback for musl and older C libraries.

## Similar tools

- [noti](https://github.com/variadico/noti) notifies when a command ends, with many
  notification services. `chime` is smaller, one file of C++, and plays the desktop's
  own sounds.
- [undistract-me](https://github.com/jml/undistract-me) notifies about any long command
  in bash, without being asked.
- [done](https://github.com/franciscolourenco/done) does the same for fish.
- The Python library [chime](https://pypi.org/project/chime/) plays notification sounds
  from Python programs. It's a different project with the same name.

## License

Copyright © 2026 Daher Alfawares.

`chime` is free software: you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation, either
version 3 of the License, or (at your option) any later version. See [COPYING](COPYING).
