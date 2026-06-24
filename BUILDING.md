# Building RewardsTheater

## Building locally on Windows
1. Install a new version of [PowerShell](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-windows?view=powershell-7.3)
2. Install [Visual Studio 2022 (or later)](https://visualstudio.microsoft.com/vs/) and install the bundled C++ compiler.
3. Install [Chocolatey](https://chocolatey.org/).
4. Instal OpenSSL via chocolatey: `choco install openssl`.
5. Install [7zip](https://www.7-zip.org/) and add `C:\Program Files\7-Zip` to [PATH](https://www.wikihow.com/Change-the-PATH-Environment-Variable-on-Windows).
6. Restart the PowerShell terminal so that `OpenSSL` and `7z` commands are visible.
7. Clone this repository via Git.
8. In PowerShell, `cd` to the cloned repository, and execute these two commands to install build dependencies:
   ```
   . ./.github/scripts/utils.pwsh/Windows-Deps.ps1
   Windows-Deps
   ```
9. Run `cmake --preset windows-x64` from the checkout root directory (override any variables per project needs)
10. Either run `cmake --build --preset windows-x64` _OR_ open the solution file created in the `build_x64` directory and build the plugin in Visual Studio
    * If a specific configuration is desired, run `cmake --build --preset windows-x64 --config <NAME OF CONFIG>` instead
11. Run `cmake --install build_x64 --prefix release --config RelWithDebInfo` to gather all plugin files for distribution 
12. Find the plugin files in the prefix location provided in step 11 in a configuration sub-directory ("RelWithDebInfo" by default)
13. Archive the files in this directory to distribute the plugin

## Setting up development environment on Windows
These steps are a continuation of the previous section on building.

1. Install [pre-commit](https://pre-commit.com/) and run `pre-commit install` in the root of the repository in order to format code automatically before a commit. (This uses [.pre-commit-config.yaml](.pre-commit-config.yaml) and [.clang-format](.clang-format)).
2. - Clone [OBS Studio](https://github.com/obsproject/obs-studio) **recursively**:
     ```
     git clone https://github.com/obsproject/obs-studio --recursive
     ```
   - Check out a tag you want to compile (e.g. `git reset --hard 31.1.1`).
   - Execute the following command to generate a Visual Studio solution:
     ```
     cmake --preset windows-x64
     ```

   - Open the `obs-studio/build_x64` folder in Visual Studio and build the solution as `Debug`.
3. Open the `RewardsTheater/build_x64` directory. Open `CMakeCache.txt` and add the following line:
   ```
   OBS_BUILD_DIR:FILEPATH=../../obs-studio/build_x64/rundir/Debug
   ```
   This is needed for CMake to copy the plugin into the OBS installation.

4. Then generate the Visual Studio solution again via `cmake --preset windows-x64`.
5. Open the project in Visual Studio. Right-click the `ALL_BUILD` project. Select "Debug" in the "Configuration:" dropdown. Open the "Debugging" pane on the left. Set the "Command:" to the OBS binary at `your_user_dir\source\repos\obs-studio\build64\rundir\Debug\bin\64bit\obs64.exe`, and the working directory to `your_user_dir\source\repos\obs-studio\build64\rundir\Debug\bin\64bit`.
6. Now when you hit "Run" inside Visual Studio, the plugin is copied automatically to the rundir and then Visual Studio launches OBS for debugging.

## Building locally on Linux
1. Install essential build tools for the build system: `sudo apt-get install cmake ninja-build pkg-config build-essential`
2. Make sure you have GCC 12 (or any later).
3. Add the OBS Project PPA repository to the local `apt` configuration: `sudo add-apt-repository --yes ppa:obsproject/obs-studio`
4. Install OBS Studio to get access to a current source distribution: `sudo apt-get install obs-studio libgles2-mesa-dev libsimde-dev`
5. Install Qt6 sources: `sudo apt-get install qt6-base-dev libqt6svg6-dev qt6-base-private-dev`
6. Install OpenSSL and fmtlib: `sudo apt-get install libssl-dev libfmt-dev`
7. Clone the repository.
8. Run `./.github/scripts/utils.zsh/linux-deps.sh`
9. Run `cmake --preset ubuntu-x86_64` from the checkout root directory (override any variables per project needs)
    * If a specific configuration is desired, add `-DCMAKE_BUILD_TYPE=<YOUR CONFIG>`
    * To change the configuration for the same project, provide a different value as `CMAKE_BUILD_TYPE` and add `--fresh`, e.g. `cmake --preset ubuntu-x86_64 -DCMAKE_BUILD_TYPE=MinSizeRel --fresh`
10. Run `cmake --build --preset ubuntu-x86_64` to build the project
11. Run `cmake --install build_x86_64 --prefix <DESIRED LOCATION>` to gather all plugin files for distribution 
12. Find the plugin files in the prefix location provided in step 11
13. Archive the files in this directory to distribute the plugin

## Building locally on macOS
1. Install a recent version of Xcode.
2. Clone the repository.
3. Run `.github/scripts/utils.zsh/macos-deps.zsh`
4. Run `cmake --preset macos` from the checkout root directory (override any variables per project needs)
5. Either run `cmake --build --preset macos` _OR_ open the Xcode project file created in the `build_macos` directory and build the plugin in Xcode
    * If a specific configuration is desired, run `cmake --build --preset macos --config <NAME OF CONFIG>` instead
6. Run `cmake --install build_macos --prefix <DESIRED LOCATION>` to gather all plugin files for distribution 
    * If a specific configuration is desired, add the `--config <NAME OF CONFIG>` argument as required
7. Find the plugin bundle, `DSYM` bundle, and package installer in the prefix location provided in step 5 in a configuration sub-directory ("RelWithDebInfo" by default)
8. Distribute the plugin bundle and `DSYM` bundle separately as a compressed archive

## GitHub Actions & CI

Default GitHub Actions workflows are available for the following repository actions:

* `push`: Run for commits or tags pushed to `master` or `main` branches.
* `pr-pull`: Run when a Pull Request has been pushed or synchronized.
* `dispatch`: Run when triggered by the workflow dispatch in GitHub's user interface.
* `build-project`: Builds the actual project and is triggered by other workflows.
* `check-format`: Checks CMake and plugin source code formatting and is triggered by other workflows.

The workflows make use of GitHub repository actions (contained in `.github/actions`) and build scripts (contained in `.github/scripts`) which are not needed for local development, but might need to be adjusted if additional/different steps are required to build the plugin.

### Retrieving build artifacts

Successful builds on GitHub Actions will produce build artifacts that can be downloaded for testing. These artifacts are commonly simple archives and will not contain package installers or installation programs.

### Building a Release

To create a release, an appropriately named tag needs to be pushed to the `main`/`master` branch using semantic versioning (e.g., `12.3.4`, `23.4.5-beta2`). A draft release will be created on the associated repository with generated installer packages or installation programs attached as release artifacts.
