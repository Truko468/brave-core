/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/ads/serving/eligible_ads/exclusion_rules/creative_instance_exclusion_rule.h"

#include "brave/components/brave_ads/core/internal/ads/ad_events/ad_event_unittest_util.h"
#include "brave/components/brave_ads/core/internal/ads/ad_unittest_constants.h"
#include "brave/components/brave_ads/core/internal/common/unittest/unittest_base.h"
#include "brave/components/brave_ads/core/internal/common/unittest/unittest_time_util.h"

// npm run test -- brave_unit_tests --filter=BatAds*

namespace brave_ads {

class BatAdsCreativeInstanceExclusionRuleTest : public UnitTestBase {};

TEST_F(BatAdsCreativeInstanceExclusionRuleTest, AllowAdIfThereIsNoAdsHistory) {
  // Arrange
  CreativeAdInfo creative_ad;
  creative_ad.creative_instance_id = kCreativeInstanceId;

  CreativeInstanceExclusionRule exclusion_rule({});

  // Act

  // Assert
  EXPECT_FALSE(exclusion_rule.ShouldExclude(creative_ad));
}

TEST_F(BatAdsCreativeInstanceExclusionRuleTest, AdAllowedAfter1Hour) {
  // Arrange
  CreativeAdInfo creative_ad;
  creative_ad.creative_instance_id = kCreativeInstanceId;

  AdEventList ad_events;

  const AdEventInfo ad_event = BuildAdEvent(
      creative_ad, AdType::kNotificationAd, ConfirmationType::kServed, Now());

  ad_events.push_back(ad_event);

  CreativeInstanceExclusionRule exclusion_rule(ad_events);

  AdvanceClockBy(base::Hours(1));

  // Act

  // Assert
  EXPECT_FALSE(exclusion_rule.ShouldExclude(creative_ad));
}

TEST_F(BatAdsCreativeInstanceExclusionRuleTest,
       AdAllowedAfter1HourForMultipleTypes) {
  // Arrange
  CreativeAdInfo creative_ad;
  creative_ad.creative_instance_id = kCreativeInstanceId;

  AdEventList ad_events;

  const AdEventInfo ad_event_1 = BuildAdEvent(
      creative_ad, AdType::kNotificationAd, ConfirmationType::kServed, Now());
  ad_events.push_back(ad_event_1);

  const AdEventInfo ad_event_2 = BuildAdEvent(
      creative_ad, AdType::kNewTabPageAd, ConfirmationType::kServed, Now());
  ad_events.push_back(ad_event_2);

  const AdEventInfo ad_event_3 =
      BuildAdEvent(creative_ad, AdType::kPromotedContentAd,
                   ConfirmationType::kServed, Now());
  ad_events.push_back(ad_event_3);

  const AdEventInfo ad_event_4 = BuildAdEvent(
      creative_ad, AdType::kSearchResultAd, ConfirmationType::kServed, Now());
  ad_events.push_back(ad_event_4);

  CreativeInstanceExclusionRule exclusion_rule(ad_events);

  AdvanceClockBy(base::Hours(1));

  // Act

  // Assert
  EXPECT_FALSE(exclusion_rule.ShouldExclude(creative_ad));
}

TEST_F(BatAdsCreativeInstanceExclusionRuleTest,
       DoNotAllowTheSameAdWithin1Hour) {
  // Arrange
  CreativeAdInfo creative_ad;
  creative_ad.creative_instance_id = kCreativeInstanceId;

  AdEventList ad_events;

  const AdEventInfo ad_event = BuildAdEvent(
      creative_ad, AdType::kNotificationAd, ConfirmationType::kServed, Now());

  ad_events.push_back(ad_event);

  CreativeInstanceExclusionRule exclusion_rule(ad_events);

  AdvanceClockBy(base::Hours(1) - base::Milliseconds(1));

  // Act

  // Assert
  EXPECT_TRUE(exclusion_rule.ShouldExclude(creative_ad));
}

}  // namespace brave_ads
