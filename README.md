# ThrowbackLoader

ThrowbackLoader launches any older season of Rainbow Six Siege, created for **Operation Throwback**. This is a further development of [ThrowbackLoader](https://github.com/lungu19/ThrowbackLoader) by lungu19.

> [!WARNING]
> You **must own** a copy of Tom Clancy's Rainbow Six® Siege and use this only on a **legally obtained** copy. Pirated copies, or any game other than R6S, are **not** supported.

> Currently, operators are locked when launching **Y8S3 Heavy Mettle** or newer.

## Installation

- Download the latest [release](https://github.com/Xeralin/ThrowbackLoader/releases) and extract it
- Move all the files into your game folder
- Run `LaunchR6S.exe`

## Building

The loaders and launcher are Windows x64 binaries, built with MinGW-w64. Install [MSYS2](https://www.msys2.org/), clone the repo, then run the following in the MINGW64 shell:

```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja zip
./build.sh
```