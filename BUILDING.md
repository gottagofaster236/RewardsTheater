# Building RewardsTheater

## Building locally on Windows
1. Install a new version of [PowerShell](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-windows?view=powershell-7.3)
2. Install [Visual Studio 2022 (or later)](https://visualstudio.microsoft.com/vs/) and install the bundled C++ compiler.
3. Install [Chocolatey](https://chocolatey.org/).
4. Instal OpenSSL via chocolatey: `choco install openssl`.
5. Install [7zip](https://www.7-zip.org/) and add `C:\Program Files\7-Zip` to [PATH](https://www.wikihow.com/Change-the-PATH-Environment-Variable-on-Windows).
6. Restart the PowerShell terminal so that `OpenSSL` and `7z` commands are visible.
7. Clone this repository via Git.
8. Run `.github/scripts/Build-Windows.ps1` inside PowerShell.

## Setting up development environment on Windows
These steps are a continuation of the previous section on building.

1. Install [pre-commit](https://pre-commit.com/) and run `pre-commit install` in the root of the repository in order to format code automatically before a commit. (This uses [.pre-commit-config.yaml](.pre-commit-config.yaml) and [.cmake-format.json](.cmake-format.json)).
2. Clone [OBS Studio](https://github.com/obsproject/obs-studio) **recursively** (`git clone https://github.com/obsproject/obs-studio/ --recursive`), checkout a tag you want to compile (e.g. `git reset --hard 30.0.0`). Execute the following command:
   ```
   cmake --preset windows-x64
   ```

   Then open the `obs-studio/build_x64` folder in Visual Studio and build it as Debug.
3. Open the `RewardsTheater/build_x64` directory. Open `CMakeCache.txt` and add the following line:  ```OBS_BUILD_DIR:FILEPATH=../../obs-studio/build_x64/rundir/Debug```

   Then build RewardsTheater again via its build script.
4. Open the project in Visual Studio. Right-click the `ALL_BUILD` project. Select "Debug" in the "Configuration:" dropdown. Open the "Debugging" pane. Set the "Command:" to the OBS binary at `your_user_dir\source\repos\obs-studio\build64\rundir\Debug\bin\64bit\obs64.exe`, and the working directory to `your_user_dir\source\repos\obs-studio\build64\rundir\Debug\bin\64bit`. Now when you hit "Run" inside Visual Studio, the plugin is copied automatically to the rundir and then VS launches OBS for debugging.

## Building locally on Linux
1. Install GCC 12 (or later) via `sudo apt install gcc-12 g++-12` (use your favorite package manager).
2. Clone the repository.
3. Run
   ```bash
   export CC=gcc-12
   export CXX=g++-12
   sudo ./.github/scripts/build-linux
   ```

## Building locally on macOS
1. Install a recent version of Xcode.
2. Clone the repository.
3. Run `.github/scripts/build-macos`

## GitHub Actions & CI

The scripts contained in `github/scripts` can be used to build and package the plugin and take care of setting up obs-studio as well as its own dependencies. A workflow for GitHub Actions is provided and will use these scripts.

### Retrieving build artifacts

Each build produces installers and packages that you can use for testing and releases. These artifacts can be found on the action result page via the "Actions" tab in your GitHub repository.

#### Building a Release

Simply create and push a tag and GitHub Actions will run the pipeline in Release Mode. This mode uses the tag as its version number instead of the git ref in normal mode.

### Packaging on Linux

The install step results in different directory structures depending on the value of `LINUX_PORTABLE` - "OFF" will organize outputs to be placed in the system root, such as `/usr/`, and "ON" will organize outputs for portable installations in the user's home directory. If you are packaging for a Linux distribution, you probably want to set `-DLINUX_PORTABLE=OFF`.
