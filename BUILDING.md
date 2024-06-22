# Building RewardsTheater

## Building locally on Windows
1. Install a new version of [PowerShell](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-windows?view=powershell-7.3)
2. Install [Visual Studio 2022 (or later)](https://visualstudio.microsoft.com/vs/) and install the bundled C++ compiler.
3. Clone the repository recursively via Git:

   ```git clone --recursive https://github.com/gottagofaster236/RewardsTheater```
4. Run `.github/scripts/Build-windows.ps1` inside PowerShell.

## Setting up development environment on Windows
These steps are a continuation of the previous section on building.

1. Install [pre-commit](https://pre-commit.com/) and run `pre-commit install` in the root of the repository in order to format code automatically before a commit. (This uses [.pre-commit-config.yaml](.pre-commit-config.yaml) and [.cmake-format.json](.cmake-format.json)).
2. Navigate to `../obs-studio`. OBS Studio should be cloned already by the build script. Then build it like so:
   ```
   CI/build-windows.PS1 -BuildConfiguration Debug
   ```
3. Open the `RewardsTheater/build_x64` directory. Open `CMakeCache.txt` and add the following line:  ```OBS_BUILD_DIR:FILEPATH=../../obs-studio/build64```
   
   Then build RewardsTheater again via the build script.
4. Open the project in Visual Studio. Right-click the `ALL_BUILD` project. Select "Debug" in the "Configuration:" dropdown. Set the "Command:" to the OBS binary at `your_user_dir\source\repos\obs-studio\build64\rundir\Debug\bin\64bit\obs64.exe`, and the working directory to `your_user_dir\source\repos\obs-studio\build64\rundir\Debug\bin\64bit`. Now when you hit "Run" inside Visual Studio, the plugin is copied automatically to the rundir and then VS launches OBS for debugging.

## Building locally on Linux
1. Install GCC 12 (or later) via `sudo apt install gcc-12 g++-12` (use your favorite package manager).
2. Clone the repository recursively via Git:

   ```git clone --recursive https://github.com/gottagofaster236/RewardsTheater```
3. Run
   ```bash
   export CC=gcc-12
   export CXX=g++-12
   sudo ./.github/scripts/build-linux.sh
   ```

## GitHub Actions & CI

The scripts contained in `github/scripts` can be used to build and package the plugin and take care of setting up obs-studio as well as its own dependencies. A workflow for GitHub Actions is provided and will use these scripts.

### Retrieving build artifacts

Each build produces installers and packages that you can use for testing and releases. These artifacts can be found on the action result page via the "Actions" tab in your GitHub repository.

#### Building a Release

Simply create and push a tag and GitHub Actions will run the pipeline in Release Mode. This mode uses the tag as its version number instead of the git ref in normal mode.

### Packaging on Linux

The install step results in different directory structures depending on the value of `LINUX_PORTABLE` - "OFF" will organize outputs to be placed in the system root, such as `/usr/`, and "ON" will organize outputs for portable installations in the user's home directory. If you are packaging for a Linux distribution, you probably want to set `-DLINUX_PORTABLE=OFF`.

## Building locally on macOS

No automated pipeline for now, I don't have time to set it up :( The main difficulty is building universal libraries (i.e. merged ARM and x86_64 libraries) which can take quite a while.

- Checkout the [macos-build](https://github.com/gottagofaster236/RewardsTheater/commits/macos-build/) branch of RewardsTheater.
- Create a `boost` subdirectory in the root of the RewardsTheater repository.
- Download and unpack Boost 1.85 from [here](https://www.boost.org/users/history/version_1_85_0.html) into the `boost` subdirectory.
- You are now inside `RewardsTheater/boost/boost_1_85_0`. Create and run the following script which compiles a universal static library of Boost (modifed version of [this script](https://stackoverflow.com/a/69924892/6120487))
  ```bash
  #!/bin/sh

  export MACOSX_DEPLOYMENT_TARGET=11.0
  rm -rf arm64 x86_64 universal stage bin.v2
  rm -f b2 project-config*
  ./bootstrap.sh cxxflags="-arch x86_64 -arch arm64" cflags="-arch x86_64 -arch arm64" linkflags="-arch x86_64 -arch arm64" link=static
  ./b2 toolset=clang-darwin target-os=darwin architecture=arm abi=aapcs cxxflags="-arch arm64" cflags="-arch arm64" linkflags="-arch arm64" -a
  mkdir -p arm64 && cp stage/lib/*.a arm64
  ./b2 toolset=clang-darwin target-os=darwin architecture=x86 cxxflags="-arch x86_64" cflags="-arch x86_64" linkflags="-arch x86_64" abi=sysv binary-format=mach-o -a link=static
  mkdir x86_64 && cp stage/lib/*.a x86_64
  mkdir universal
  for dylib in arm64/*; do
    lipo -create -arch arm64 $dylib -arch x86_64 x86_64/$(basename $dylib) -output universal/$(basename $dylib);
  done
  for dylib in universal/*; do
    lipo $dylib -info;
  done
  ```
  It merges the `.a` files for each boost library (instead of `.dylib` ones in the original StackOverflow answer).
- Now you should have a universal static build of Boost in `RewardsTheater/boost/boost_1_85_0/universal`.
- Create an `openssl` subdirectory in the `RewardsTheater` repository.
- Download and unpack OpenSSL 3.3.1 from [here](https://github.com/openssl/openssl/releases/download/openssl-3.3.1/openssl-3.3.1.tar.gz) into that `openssl` subdirectory.
- You are now inside `RewardsTheater/openssl/openssl-3.3.1`. Execute `export MACOSX_DEPLOYMENT_TARGET=11.0`.
- Then follow the steps from [this StackOverflow answer](https://stackoverflow.com/a/75250222/6120487) to build a universal OpenSSL library. Skip the `make install` step, though.
- Now you should have a built universal library of OpenSSL at `RewardsTheater/openssl/openssl-3.3.1/libcrypto.dylib`.
- These paths are hardcoded in `RewardsTheater/.github/scripts/.build.zsh` around line 231. Please change them if you want to build against other versions of Boost or OpenSSL.
- Now, navigate to `RewardsTheater/.github/scripts` and run `./build-macos`. If the build is successful, there's going to be a reference to the resuling `.pkg` file location in the logs. Copy that file and release it.