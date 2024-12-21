cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# Add your custom source files here - header files are optional and only required for visibility
# e.g. in Xcode or Visual Studio
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE images/icons.qrc
          src/RewardsTheaterMain.cpp
          src/SettingsDialog.cpp
          src/SettingsDialog.h
          src/TwitchAuth.cpp
          src/TwitchAuth.h
          src/Reward.h
          src/Reward.cpp
          src/RewardRedemptionQueue.h
          src/RewardRedemptionQueue.cpp
          src/TwitchRewardsApi.h
          src/TwitchRewardsApi.cpp
          src/TwitchAuthDialog.cpp
          src/TwitchAuthDialog.h
          src/RewardsTheaterPlugin.cpp
          src/RewardsTheaterPlugin.h
          src/Settings.h
          src/Settings.cpp
          src/HttpClient.h
          src/HttpClient.cpp
          src/Log.h
          src/BoostAsio.h
          src/IoThreadPool.h
          src/IoThreadPool.cpp
          src/EditRewardDialog.h
          src/EditRewardDialog.cpp
          src/RewardWidget.h
          src/RewardWidget.cpp
          src/GithubUpdateApi.h
          src/GithubUpdateApi.cpp
          src/ErrorMessageBox.h
          src/ErrorMessageBox.cpp
          src/ConfirmDeleteReward.h
          src/ConfirmDeleteReward.cpp
          src/PubsubListener.h
          src/PubsubListener.cpp
          src/RewardRedemptionWidget.h
          src/RewardRedemptionWidget.cpp
          src/RewardRedemptionQueueDialog.h
          src/RewardRedemptionQueueDialog.cpp
          src/QObjectCallback.h
          src/LibVlc.h
          src/LibVlc.cpp
          src/OnTopDialog.h
          src/OnTopDialog.cpp
)

# GCC ignores the "pragma GCC diagnostic ignored" in precompiled headers
if(MSVC)
  target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE src/BoostAsio.h)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /bigobj)
endif()
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost CONFIG REQUIRED COMPONENTS system url json)
IF (WIN32)
  set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
find_package(OpenSSL REQUIRED)
if(DEFINED FMT_DIRECTORY)
  add_subdirectory(${FMT_DIRECTORY})
else()
  find_package(fmt REQUIRED)
endif()
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC Boost::system Boost::url Boost::json
                                                   OpenSSL::SSL OpenSSL::Crypto fmt::fmt-header-only)

# Import libobs as main plugin dependency
find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Core Widgets Gui)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

configure_file(src/RewardsTheaterVersion.h.in
               ${CMAKE_SOURCE_DIR}/src/RewardsTheaterVersion.generated.h)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/RewardsTheaterVersion.generated.h)



set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
