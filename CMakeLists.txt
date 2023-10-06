cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0111 OLD)

project(RewardsTheater VERSION 1.0.0)
add_library(${CMAKE_PROJECT_NAME} MODULE)

# Replace `Your Name Here` with the name (yours or your organization's) you want to see as the
# author of the plugin (in the plugin's metadata itself and in the installers)
set(PLUGIN_AUTHOR "gottagofaster")

# Replace 'https://www.example.com` with a link to the website of your plugin or repository
set(PLUGIN_WEBSITE "https://github.com/gottagofaster236/RewardsTheater")

# Replace `com.example.obs-plugin-template` with a unique Bundle ID for macOS releases (used both in
# the installer and when submitting the installer for notarization)
set(MACOS_BUNDLEID "com.gottagofaster.RewardsTheater")

# Replace `me@contoso.com` with the maintainer email address you want to put in Linux packages
set(LINUX_MAINTAINER_EMAIL "lr.soft.now@gmail.com")

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# Add your custom source files here - header files are optional and only required for visibility
# e.g. in Xcode or Visual Studio
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/RewardsTheaterMain.cpp
          src/SettingsDialog.cpp
          src/SettingsDialog.h
          src/TwitchAuth.cpp
          src/TwitchAuth.h
          src/Reward.h
          src/RewardsQueue.h
          src/RewardsQueue.cpp
          src/TwitchRewardsApi.h
          src/TwitchRewardsApi.cpp
          src/AuthenticateWithTwitchDialog.cpp
          src/AuthenticateWithTwitchDialog.h
          src/RewardsTheaterPlugin.cpp
          src/RewardsTheaterPlugin.h
          src/Settings.h
          src/Settings.cpp
          src/HttpUtil.h
          src/HttpUtil.cpp
          src/Log.h
          src/BoostAsio.h
          src/IoThreadPool.h
          src/IoThreadPool.cpp
          src/EditRewardDialog.h
          src/EditRewardDialog.cpp
          src/RewardWidget.h
          src/RewardWidget.cpp)

# GCC ignores the "pragma GCC diagnostic ignored" in precompiled headers
if(MSVC)
  target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE src/BoostAsio.h)
endif()

# Libraries from vcpkg
find_package(Boost REQUIRED COMPONENTS system url json)
find_package(OpenSSL REQUIRED)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC Boost::system Boost::url Boost::json
                                                   OpenSSL::SSL OpenSSL::Crypto)

# Import libobs as main plugin dependency
find_package(libobs REQUIRED)
include(cmake/ObsPluginHelpers.cmake)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_qt(COMPONENTS Core Widgets Gui)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets Qt::Gui)

set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

# /!\ TAKE NOTE: No need to edit things past this point /!\

# --- Platform-independent build settings ---

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
  configure_file(cmake/bundle/windows/installer-Windows.iss.in
                 ${CMAKE_BINARY_DIR}/installer-Windows.generated.iss)

  configure_file(cmake/bundle/windows/resource.rc.in ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc)

  if(MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4)
  endif()
  # --- End of section ---

  # -- macOS specific build settings and tasks --
elseif(OS_MACOS)
  configure_file(cmake/bundle/macos/installer-macos.pkgproj.in
                 ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)

  set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
  set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")

  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)
  # --- End of section ---

  # --- Linux-specific build settings and tasks ---
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)
endif()
# --- End of section ---

setup_plugin_target(${CMAKE_PROJECT_NAME})
