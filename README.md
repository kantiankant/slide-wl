# slide-wl

A Wayland compositor port of [slide](https://github.com/kantiankant/slide) from Xserver to Wayland, built on the wlroots library (built against wlroots 0.20).

> **Work in progress:** This is highly experimental, so expect things to break before it becomes usable

## Concept

Go read the README.md at [slide](https://github.com/kantiankant/slide), then come back

## Dependencies

Build-time:
- `wlroots-0.20`
- `wayland-server`
- `xkbcommon`
- `libinput`
- `wayland-scanner` (for the layer shell protocol header)
- `bmake` (most likely to work because the only OS where slide-wl has been tested on is FreeBSD, at the moment)

Optional (used in the default config, but for the love of Christ, please edit it to your needs beforehand):
- `foot`  (terminal)
- `rofi`  (app launcher)
- `grim` + `slurp`  (screenshots)
- `pipewire` & co  (volume control)
- `dunst` (notifications)
  
## Building

FreeBSD's wlroots package doesn't ship the protocol XMLs, so you need to vendor the layer shell XML first:

```sh
mkdir -p protocols
fetch -o protocols/wlr-layer-shell-unstable-v1.xml \
  https://raw.githubusercontent.com/swaywm/wlroots/master/protocol/wlr-layer-shell-unstable-v1.xml
```



On Linux, use `wget` or `curl` instead of `fetch`. Then:

On Linux:

```sh
bmake
sudo/doas bmake install   # installs to /usr/local/bin by default
```

To set a custom prefix:

```sh
make PREFIX=~/.local install
```

## Configuration

Edit `config.h` before building. The defaults assume `foot`, `rofi`, `yazi`, `kew`, `ani-cli`, and `kantbar`. Swap them out for whatever you actually use.

Key tunables in `config.h`:

| Constant | Default | Description |
|---|---|---|
| `WIN_MOVE_STEP` | 60 | Pixels per keyboard window-move |
| `PAN_STEP` | 120 | Pixels per keyboard viewport pan |

## Keybinds (defaults)

### Launching things

| Key | Action |
|---|---|
| `Super+Q` | Open terminal (foot) |
| `Super+E` | File manager (yazi) |
| `Super+R` | Music player (kew) |
| `Super+Shift+R` | ani-cli |
| `Super+Space` | App launcher (rofi drun) |
| `Print` | Screenshot (grim + slurp) |

### Window management

| Key | Action |
|---|---|
| `Super+W` | Kill focused window |
| `Super+C` | Center focused window |
| `Super+F` | Toggle fullscreen |
| `Super+Shift+H/J/K/L` | Move window left/down/up/right (doesn't work yet) |
| `Super+Ctrl+H/L` | Cycle focus backward/forward |

### Viewport / canvas

| Key | Action |
|---|---|
| `Super+H/J/K/L` | Pan viewport left/down/up/right |

### Mouse

| Gesture | Action |
|---|---|
| Click | Focus window |
| `Super+LMB drag` | Move window |
| `Super+RMB drag` | Resize window |
| `Super+Shift+RMB drag` | Pan viewport |

### System

| Key | Action |
|---|---|
| `Super+Shift+E` | Quit compositor (doesn't work on the *BSDs for some wretched reason) |
| Volume keys | Adjust audio via wpctl |
| Brightness keys | Adjust backlight |

## Running

```sh
slide-wl [-s startup_command] (but for the love of Christ, please use dbus to launch it)
```

The `-s` flag runs a command at startup (useful for launching a bar or autostart script).

`WAYLAND_DISPLAY`, `XDG_SESSION_TYPE`, and `XDG_CURRENT_DESKTOP` are set automatically.

## Tested on

- FreeBSD (because I'm on FreeBSD at the moment. Anyone on GNU/Linux is welcome to test it and report back)


## Checklist

- [ ] zoom support
- [ ] layer focus (so you can input text into rofi's search bar
- [x] XWayland via xwayland-satallite
- [x] Fixing the movement keybinds
- [ ] rc.conf-style autostart in the config.h file 


## Thanks

- [TinyWL](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl) for teaching me how to build small Wayland compositors
- [Slide](https://github.com/kantiankant/slide) for inspiring me
- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) for powering slide-wl




## License

GPL-3.0
