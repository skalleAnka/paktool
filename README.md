# paktool

## Description
**paktool** is a small utility that can be used to list, extract, convert or compare game pack files.

See the full documentation & example usage [here](cli/readme.md)

## Supported pack formats
At this time the following pack formats are supported:

* .pak (id software Quake/Quake 2)
* .pk3 (id software Quake 3 +other games and many modern ports of retro games)
* .zip (.pk3 files are just .zip files with another extension. Note: Only compression method DEFLATE or uncompressed are supported)
* .grp (Build engine games)
* Folder (just your regular disk folder)

## Building
Building the source code requires three things: a C++ compiler with support for **C++20**, **CMake** 3.25 or newer and an internet connection.

The following C++ compilers are known to work:

* gcc 13+
* Clang 16+
* MSVC 2022 (16.11+)

It has dependencies on **boost**, **zlib** and zlib's **minizip** component. If you don't have **boost** development libraries already available to CMake, the CMake script will download a suitable source archive from *boost.org* during configure, and build the relevant components. **Zlib** will always be downloaded from *zlib.org* by the configuration, as many systems don't have the relevant minizip package, have it under a strange name, or just have it plain broken like vcpkg minizip for MinGW.

If you want the man page to be created, the **pandoc** package needs to be installed. If not installed, it will be skipped. A Windows build will always skip this part.

Of course you can open the project folder in your favorite editor/IDE with CMake support and build it from there, but if you just want to build it and be done with it you can just open a command prompt in the project folder and run these commands:

```
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```
The first command configures the build into a new sub-folder called **build**. The second command performs the build, after which you should be able to find the **paktool** executable in the folder **build/cli**.

If the first step complains about your compiler version and you have another compiler on your system that you think might work instead, add the path to your alternative compiler like in this example:

```
cmake -DCMAKE_BUILD_TYPE=Release -B build -DCMAKE_CXX_COMPILER=/usr/bin/clang++-16
```

There is an optional install step you can run that is mostly useful on Linux that will install the executable itself as well as a man page:
```
cmake --install build
```
If you don't want to run the install step (and you probably shouldn't on Windows), you can just grab the executable file from **build/cli** and start using it.

## Q&A
### Q: Does the .PAK file support include S!N or Daikatana?
A: No, it does not. These games used slightly different variations of the format. It wouldn't be too difficult to add if necessary, but I suspect the demand for this would be pretty low.

### Q: Hey wait a minute, if it supports folders and zip files, does this mean I can use this to compare arbitrary folders or zip files on my hard disk?
A: Yes, I suppose it does.