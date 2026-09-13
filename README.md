# Handsdown

This is handsdown the best mouse control tool that only requires the keyboard.

## Dependencies

- C compiler (`cc`/`gcc`/`clang`)
- `make`
- `pkg-config`
- Wayland client library (`wayland-client`)
- GTK3 (for the overlay)
- gtk-layer-shell (for Wayland layer-shell overlays)
- Linux kernel headers (for the `uinput` virtual device)

## Install dependencies

Arch:
```
sudo pacman -S --needed base-devel wayland gtk3 gtk-layer-shell linux-api-headers
```

Debian / Ubuntu:
```
sudo apt install build-essential pkg-config libwayland-dev libgtk-3-dev libgtk-layer-shell-dev linux-libc-dev
```

Supported actions:

- None

Supported platforms:

- None

Actions that are planned (by order):

- grid absolute movement
- refined relational movement
- elements scan movement

Platforms that are planned to be supported (by order):

- Wayland
- X11
- Windows / mac
