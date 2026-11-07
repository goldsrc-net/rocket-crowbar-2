# cl_dll/rc2

Rocket Crowbar 2's own client files: what SDK 2.3's client doesn't have. client.dll keeps no file
names, so each is named after what it holds, and CMake picks up every `.cpp`/`.h` here
(`cl_dll/CMakeLists.txt`). Stock client functions RC2 changed stay in their SDK file in `cl_dll/`.
