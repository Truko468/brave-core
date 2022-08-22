/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/ads/serving/eligible_ads/exclusion_rules/dislike_exclusion_rule.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "bat/ads/internal/creatives/creative_ad_info.h"
#include "bat/ads/internal/deprecated/client/client_state_manager.h"
#include "bat/ads/internal/deprecated/client/preferences/filtered_advertiser_info.h"

namespace ads {

DislikeExclusionRule::DislikeExclusionRule() = default;

DislikeExclusionRule::~DislikeExclusionRule() = default;

std::string DislikeExclusionRule::GetUuid(
    const CreativeAdInfo& creative_ad) const {
  return creative_ad.advertiser_id;
}

bool DislikeExclusionRule::ShouldExclude(const CreativeAdInfo& creative_ad) {
  if (!DoesRespectCap(creative_ad)) {
    last_message_ =
        base::StringPrintf("advertiserId %s excluded due to being disliked",
                           creative_ad.advertiser_id.c_str());

    return true;
  }

  return false;
}

const std::string& DislikeExclusionRule::GetLastMessage() const {
  return last_message_;
}

bool DislikeExclusionRule::DoesRespectCap(const CreativeAdInfo& creative_ad) {
  const FilteredAdvertiserList& filtered_advertisers =
      ClientStateManager::GetInstance()->GetFilteredAdvertisers();
  if (filtered_advertisers.empty()) {
    return true;
  }

  const auto iter = std::find_if(
      filtered_advertisers.cbegin(), filtered_advertisers.cend(),
      [&creative_ad](const FilteredAdvertiserInfo& filtered_advertiser) {
        return filtered_advertiser.id == creative_ad.advertiser_id;
      });

  if (iter == filtered_advertisers.cend()) {
    return true;
  }

  return false;
}

}  // namespace ads
