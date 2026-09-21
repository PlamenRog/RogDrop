# RogDrop
RogDrop is a small live color picker for wlroots-based Wayland compositors. For its X based alternative see: [RogDropper](https://github.com/PlamenRog/RogDropper)

It doesn't depend on xdg portal and is therefore very flexible.

Preferred backend is `ext-image-copy-capture-v1`, but will also do with `wlr-screencopy-unstable-v1`.

## Dependencies & runtime requirements
 - GCC 
 - GNU Make
 - pkg-config
 - Wayland client libraries and `wayland-scanner`
 - `wayland-protocols`
 - `wlr-protocols`
 - `xkbcommon`
 - `wlr-layer-shell-unstable-v1`
 - `ext-image-copy-capture-v1`

## Build with Nix

```sh
nix-shell --run 'make clean && make'
```
The included `shell.nix` explicitly exports the `wayland-protocols` and `wlr-protocols` data directories.

## Usage
```sh
./rogdrop
```

Though you probably want to set a keybind in order to not have to pull up a terminal each time you want to use it, in which case you should do something like:

```sh
./rogdrop | wl-copy
```
to save the output to the clipboard.

