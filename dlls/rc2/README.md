# dlls/rc2

Rocket Crowbar 2's own source files: the classes SDK 2.3 doesn't have. rc2.dll keeps no file
names, so each is named after its main class (e.g. `crowbarang.cpp` for `CCrowbarang`),
and CMake picks up every `.cpp`/`.h` here (`dlls/CMakeLists.txt`). Stock functions RC2 changed
stay in their SDK file in `dlls/`.
