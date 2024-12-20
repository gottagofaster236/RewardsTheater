// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2023, Lev Leontev

#pragma once

#include <util/config-file.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

struct SourcePlaybackSettings {
    bool randomPositionEnabled;
    bool loopVideoEnabled;
    double loopVideoDurationSeconds;
};

class Settings {
public:
    Settings(config_t* config);

    /// Whether to play a reward immediately (possibly simultaneously with other rewards) or put it in a queue.
    bool isRewardRedemptionQueueEnabled() const;
    void setRewardRedemptionQueueEnabled(bool rewardRedemptionQueueEnabled);

    /// How much to wait between two reward redemptions in a queue, isRewardRedemptionQueueEnabled() is true.
    double getIntervalBetweenRewardsSeconds() const;
    void setIntervalBetweenRewardsSeconds(double intervalBetweenRewardsSeconds);

    std::optional<std::string> getTwitchAccessToken() const;
    void setTwitchAccessToken(const std::optional<std::string>& accessToken);

    std::optional<std::string> getObsSourceName(const std::string& rewardId) const;
    void setObsSourceName(const std::string& rewardId, const std::optional<std::string>& obsSourceName);

    bool isRandomPositionEnabled(const std::string& rewardId) const;
    void setRandomPositionEnabled(const std::string& rewardId, bool randomPositionEnabled);

    bool isLoopVideoEnabled(const std::string& rewardId) const;
    void setLoopVideoEnabled(const std::string& rewardId, bool loopVideoEnabled);

    double getLoopVideoDurationSeconds(const std::string& rewardId) const;
    void setLoopVideoDurationSeconds(const std::string& rewardId, double loopVideoDuration);

    SourcePlaybackSettings getSourcePlaybackSettings(const std::string& rewardId) const;
    void setSourcePlaybackSettings(const std::string& rewardId, const SourcePlaybackSettings& sourcePlaybackSettings);

    // We can't get the video size of a Media Source while it's not playing, thus, we save the size while it is playing.
    std::optional<std::pair<std::uint32_t, std::uint32_t>> getLastVideoSize(
        const std::string& rewardId,
        const std::string& obsSourceName,
        std::size_t playlistIndex
    ) const;
    void setLastVideoSize(
        const std::string& rewardId,
        const std::string& obsSourceName,
        std::size_t playlistIndex,
        std::size_t playlistSize,
        const std::optional<std::pair<std::uint32_t, std::uint32_t>>& lastVideoSize
    );

    void deleteReward(const std::string& rewardId);

    std::optional<bool> isPluginDisabled() const;
    void setPluginDisabled(bool pluginDisabled);

private:
    std::string getLastObsSourceName(const std::string& rewardId) const;
    void setLastObsSourceName(const std::string& rewardId, const std::string& lastObsSource);

    std::size_t getLastPlaylistSize(const std::string& rewardId) const;
    void setLastPlaylistSize(const std::string& rewardId, std::size_t lastPlaylistSize);

    config_t* config;
    // config_get_string returns a char* which we copy to a std::string.
    // But that char* can be freed - therefore we need a mutex for string get/set operations.
    mutable std::recursive_mutex configMutex;
};
