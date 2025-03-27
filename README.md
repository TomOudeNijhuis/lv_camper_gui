Install:
```
git submodule update --init --recursive
```

Build:
```
cmake -B build -S .
make -C build -j
```

Run:
```
./build/bin/camper-gui
```

### Installing LVGL

It is possible to install LVGL to your system however, this is currently only
supported with cmake.

```
cmake --install ./build
```
