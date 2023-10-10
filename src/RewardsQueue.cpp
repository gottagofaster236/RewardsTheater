// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2023, Lev Leontev

#include "RewardsQueue.h"

#include <algorithm>
#include <cstring>

RewardsQueue::RewardsQueue(const Settings& settings) : settings(settings) {}

std::vector<Reward> RewardsQueue::getRewardsQueue() const {
    std::lock_guard<std::mutex> lock(rewardsMutex);
    auto rewardsQueueCopy = rewardsQueue;
    return std::vector<Reward>(rewardsQueueCopy.begin(), rewardsQueueCopy.end());
}

void RewardsQueue::queueReward(const Reward& reward) {
    bool playImmediately;
    {
        std::lock_guard<std::mutex> lock(rewardsMutex);
        playImmediately = rewardsQueue.empty();
        rewardsQueue.push_back(reward);
    }
    if (playImmediately) {
        playNextReward();
    }
}

void RewardsQueue::removeReward(const Reward& reward) {
    std::lock_guard<std::mutex> lock(rewardsMutex);
    auto position = std::find(rewardsQueue.begin(), rewardsQueue.end(), reward);
    if (position != rewardsQueue.end()) {
        rewardsQueue.erase(position);
    }
}

void RewardsQueue::playNextReward() {
    std::optional<OBSSourceAutoRelease> sourceOptional = popNextReward();
    if (!sourceOptional) {
        return;
    }
    const OBSSourceAutoRelease& source = sourceOptional.value();

    signal_handler_t* sh = obs_source_get_signal_handler(source);
    mediaEndSignal = OBSSignal(sh, "media_ended", &onMediaEnded, this);

    playObsSource(source);
}

void RewardsQueue::playObsSource(const std::string& obsSourceName) {
    playObsSource(getObsSource(obsSourceName));
}

void RewardsQueue::stopObsSource(const std::string& obsSourceName) {
    stopObsSource(getObsSource(obsSourceName));
}

bool RewardsQueue::isMediaSource(const std::string& obsSourceName) {
    return isMediaSource(getObsSource(obsSourceName));
}

std::vector<std::string> RewardsQueue::enumObsSources() {
    std::vector<std::string> sources;
    obs_enum_sources(&RewardsQueue::enumObsSourcesProc, &sources);
    return sources;
}

std::optional<OBSSourceAutoRelease> RewardsQueue::popNextReward() {
    std::lock_guard<std::mutex> lock(rewardsMutex);
    while (!rewardsQueue.empty()) {
        Reward nextReward = rewardsQueue.front();
        rewardsQueue.pop_front();
        std::optional<std::string> obsSourceName = settings.getObsSourceName(nextReward.id);
        if (!obsSourceName) {
            continue;
        }
        OBSSourceAutoRelease source = obs_get_source_by_name(obsSourceName.value().c_str());
        if (!isMediaSource(source)) {
            continue;
        }
        return source;
    }
    return {};
}

OBSSourceAutoRelease RewardsQueue::getObsSource(const std::string& obsSourceName) {
    return obs_get_source_by_name(obsSourceName.c_str());
}

void RewardsQueue::playObsSource(obs_source_t* source) {
    if (!source) {
        return;
    }
    obs_enum_scenes(&RewardsQueue::showSourceEnumProc, source);
    proc_handler_call(obs_source_get_proc_handler(source), "restart", nullptr);
}

bool RewardsQueue::showSourceEnumProc(void* sourceParam, obs_source_t* sceneParam) {
    obs_source_t* source = static_cast<obs_source_t*>(sourceParam);
    obs_scene_t* scene = obs_scene_from_source(sceneParam);
    obs_sceneitem_t* sceneItem = obs_scene_find_source_recursive(scene, obs_source_get_name(source));
    if (sceneItem) {
        obs_sceneitem_set_visible(sceneItem, true);
    }
    return true;
}

void RewardsQueue::stopObsSource(const obs_source_t* source) {
    if (!source) {
        return;
    }
    proc_handler_call(obs_source_get_proc_handler(source), "stop", nullptr);
}

bool RewardsQueue::hideSourceEnumProc(void* source, obs_source_t* scene) {
    (void) source;
    (void) scene;
    return false;
}

bool RewardsQueue::isMediaSource(const obs_source_t* source) {
    if (!source) {
        return false;
    }
    return std::strcmp(obs_source_get_id(source), "ffmpeg_source") == 0;
}

bool RewardsQueue::enumObsSourcesProc(void* param, obs_source_t* source) {
    std::vector<std::string>& sources = *reinterpret_cast<std::vector<std::string>*>(param);
    if (isMediaSource(source)) {
        sources.emplace_back(obs_source_get_name(source));
    }
    return true;
}

void RewardsQueue::onMediaEnded(void* param, [[maybe_unused]] calldata_t* data) {
    static_cast<RewardsQueue*>(param)->playNextReward();
}
