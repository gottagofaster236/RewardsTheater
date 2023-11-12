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
2. Navigate to `../obs-studio`. OBS Studio should be cloned already by the build script. Then build it according to the instructions on the [OBS website](https://obsproject.com/wiki/Building-OBS-Studio).
2. Open the `RewardsTheater/build64` directory. Open `CMakeCache.txt` and add the following line:  ```OBS_BUILD_DIR:FILEPATH=../../obs-studio/build64```
   
   Then build RewardsTheater again via the build script.
3. Open the project in Visual Studio. Right-click the `ALL_BUILD` project and set the debugging target to the OBS binary at `your_obs_clone_path\obs-studio\build64\rundir\Debug\bin\64bit\obs64.exe`, and the working directory to `your_obs_clone_path\obs-studio\build64\rundir\Debug\bin\64bit`. Now when you hit "Run" inside Visual Studio, the plugin is copied automatically to the rundir and then VS launches OBS for debugging.

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

