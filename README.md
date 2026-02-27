# Lawnch Plugins

This repo contains plugins for [Lawnch](https://github.com/hoppxi/lawnch).

## Plugins

- [Calculator](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/calculator) – powerful calculator
- [Clipboard](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/clipboard) – query clipboard from cliphist
- [Command](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/command) – run command
- [Emoji](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/emoji) – query emojis if your font supports them
- [Files](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/files) – file indexing with option to open folders and files
- [Google](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/google) – google things with suggestions
- [Power Menu](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/powermenu) – power menu to create wlogout like window
- [URL](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/url) - url opener
- [Wallpapers](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/wallpapers) – query wallpapers from some path
- [YouTube](https://github.com/hoppxi/lawnch-plugins/tree/main/plugins/youtube) – search on youtube

## Plugin Development

Lawnch plugins are written in C++ and compiled as shared libraries (`.so`). They communicate with the host application via a VTable API.
Look into powermenu and google plugins for comprenshive example.
