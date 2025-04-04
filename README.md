## Install

```bash
git submodule update --init --recursive
sudo apt-get install libcurl4-openssl-dev
apt-get install -y make cmake build-essential
apt-get install -y libsdl2-dev libsdl2-image-dev
```
 to cross:
```bash
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

sudo dpkg --add-architecture arm64
sudo apt-get update

sudo apt-get install libcurl4-openssl-dev:arm64

```

On pi:
```bash
sudo apt-get install libcurl4 libsdl2-2.0-0 libsdl2-image-2.0-0
```

## x64

Build x64:
```bash
cmake -B build -S .
make -C build -j
```

Run:
```bash
./build/bin/camper-gui
```

## ARM64

Build ARM64:
```bash
docker compose run --remove-orphans cross-build 
```

with shell:
```bash
docker compose run cross-shell --remove-orphans
```

inside container:
```bash
mkdir -p build-arm64
cd build-arm64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm64.cmake
make -j
```

copy to pi:
```
scp build-arm64/bin/camper-gui tom@192.168.128.50:/home/tom
```

### Installing LVGL

It is possible to install LVGL to your system however, this is currently only
supported with cmake.

```
cmake --install ./build
```
