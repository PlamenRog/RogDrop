{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    gcc
    gnumake
    pkg-config
    wayland
    wayland-protocols
    wlr-protocols
    libxkbcommon
  ];

  shellHook = ''
    export WAYLAND_PROTOCOLS="${pkgs.wayland-protocols}/share/wayland-protocols"
    export WLR_PROTOCOLS="${pkgs.wlr-protocols}/share/wlr-protocols"
  '';
}
