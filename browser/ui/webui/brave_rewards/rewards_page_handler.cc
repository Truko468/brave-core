/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/brave_rewards/rewards_page_handler.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/scoped_observation.h"
#include "brave/browser/brave_ads/ads_service_factory.h"
#include "brave/browser/brave_rewards/rewards_service_factory.h"
#include "brave/components/brave_ads/browser/ads_service.h"
#include "brave/components/brave_ads/core/public/history/ad_history_feature.h"
#include "brave/components/brave_ads/core/public/history/ad_history_value_util.h"
#include "brave/components/brave_ads/core/public/prefs/pref_names.h"
#include "brave/components/brave_ads/core/public/targeting/geographical/subdivision/supported_subdivisions.h"
#include "brave/components/brave_news/common/pref_names.h"
#include "brave/components/brave_rewards/browser/rewards_service.h"
#include "brave/components/brave_rewards/browser/rewards_service_observer.h"
#include "brave/components/brave_rewards/common/mojom/rewards.mojom.h"
#include "brave/components/brave_rewards/common/pref_names.h"
#include "brave/components/l10n/common/country_code_util.h"
#include "brave/components/ntp_background_images/common/pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/brave_components_strings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace brave_rewards {

namespace {

static constexpr auto kPluralStrings =
    base::MakeFixedFlatMap<std::string_view, int>(
        {{"unconnectedAdsViewedText",
          IDS_REWARDS_UNCONNECTED_ADS_VIEWED_TEXT}});

PrefService* GetLocalState() {
  return g_browser_process->local_state();
}

}  // namespace

// Listens for updates to browser data displayed on the Rewards page and
// executes a callback when updates occur.
class RewardsPageHandler::UpdateObserver
    : public RewardsServiceObserver,
      public bat_ads::mojom::BatAdsObserver {
 public:
  UpdateObserver(RewardsService* rewards_service,
                 brave_ads::AdsService* ads_service,
                 PrefService* pref_service,
                 base::RepeatingCallback<void(UpdateSource)> update_callback)
      : update_callback_(std::move(update_callback)) {
    rewards_observation_.Observe(rewards_service);
    ads_service->AddBatAdsObserver(
        ads_observer_receiver_.BindNewPipeAndPassRemote());

    pref_change_registrar_.Init(pref_service);
    AddPrefListener(brave_ads::prefs::kOptedInToNotificationAds,
                    UpdateSource::kAds);
    AddPrefListener(brave_ads::prefs::kMaximumNotificationAdsPerHour,
                    UpdateSource::kAds);
    AddPrefListener(
        brave_ads::prefs::kSubdivisionTargetingUserSelectedSubdivision,
        UpdateSource::kAds);
    AddPrefListener(brave_ads::prefs::kOptedInToSearchResultAds,
                    UpdateSource::kAds);
    AddPrefListener(prefs::kAutoContributeEnabled, UpdateSource::kRewards);
    AddPrefListener(prefs::kAutoContributeAmount, UpdateSource::kRewards);
    AddPrefListener(prefs::kMinVisitTime, UpdateSource::kRewards);
    AddPrefListener(prefs::kMinVisits, UpdateSource::kRewards);
    AddPrefListener(brave_news::prefs::kBraveNewsOptedIn, UpdateSource::kAds);
    AddPrefListener(brave_news::prefs::kNewTabPageShowToday,
                    UpdateSource::kAds);
    AddPrefListener(ntp_background_images::prefs::
                        kNewTabPageShowSponsoredImagesBackgroundImage,
                    UpdateSource::kAds);
  }

  // RewardsServiceObserver:
  void OnRewardsInitialized(RewardsService*) override {
    OnUpdate(UpdateSource::kRewards);
  }

  void OnRewardsWalletCreated() override { OnUpdate(UpdateSource::kRewards); }

  void OnCompleteReset(bool success) override {
    OnUpdate(UpdateSource::kRewards);
  }

  void OnExternalWalletConnected() override {
    OnUpdate(UpdateSource::kRewards);
  }

  void OnExternalWalletLoggedOut() override {
    OnUpdate(UpdateSource::kRewards);
  }

  void OnExternalWalletReconnected() override {
    OnUpdate(UpdateSource::kRewards);
  }

  void OnExternalWalletDisconnected() override {
    OnUpdate(UpdateSource::kRewards);
  }

  // bat_ads::mojom::BatAdsObserver:
  void OnAdRewardsDidChange() override { OnUpdate(UpdateSource::kAds); }
  void OnBrowserUpgradeRequiredToServeAds() override {}
  void OnIneligibleRewardsWalletToServeAds() override {}
  void OnRemindUser(brave_ads::mojom::ReminderType type) override {}

 private:
  void OnUpdate(UpdateSource update_source) {
    update_callback_.Run(update_source);
  }

  void OnPrefChanged(UpdateSource update_source, const std::string& path) {
    OnUpdate(update_source);
  }

  void AddPrefListener(const std::string& path, UpdateSource update_source) {
    // Unretained because `pref_change_registrar_` is owned by `this`.
    pref_change_registrar_.Add(
        path, base::BindRepeating(&UpdateObserver::OnPrefChanged,
                                  base::Unretained(this), update_source));
  }

  base::ScopedObservation<RewardsService, RewardsServiceObserver>
      rewards_observation_{this};
  mojo::Receiver<bat_ads::mojom::BatAdsObserver> ads_observer_receiver_{this};
  PrefChangeRegistrar pref_change_registrar_;
  base::RepeatingCallback<void(UpdateSource)> update_callback_;
};

RewardsPageHandler::RewardsPageHandler(
    mojo::PendingRemote<mojom::RewardsPage> page,
    mojo::PendingReceiver<mojom::RewardsPageHandler> receiver,
    std::unique_ptr<BubbleDelegate> bubble_delegate,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      bubble_delegate_(std::move(bubble_delegate)),
      rewards_service_(RewardsServiceFactory::GetForProfile(profile)),
      ads_service_(brave_ads::AdsServiceFactory::GetForProfile(profile)),
      prefs_(profile->GetPrefs()) {
  CHECK(rewards_service_);
  CHECK(ads_service_);
  CHECK(prefs_);

  // Unretained because `update_observer_` is owned by `this`.
  update_observer_ = std::make_unique<UpdateObserver>(
      rewards_service_, ads_service_, prefs_,
      base::BindRepeating(&RewardsPageHandler::OnUpdate,
                          base::Unretained(this)));
}

RewardsPageHandler::~RewardsPageHandler() = default;

void RewardsPageHandler::OnPageReady() {
  if (bubble_delegate_) {
    bubble_delegate_->ShowUI();
  }
}

void RewardsPageHandler::OpenTab(const std::string& url) {
  if (bubble_delegate_) {
    bubble_delegate_->OpenTab(url);
  }
}

void RewardsPageHandler::GetPluralString(const std::string& key,
                                         int32_t count,
                                         GetPluralStringCallback callback) {
  auto iter = kPluralStrings.find(key);
  CHECK(iter != kPluralStrings.end());
  std::move(callback).Run(l10n_util::GetPluralStringFUTF8(iter->second, count));
}

void RewardsPageHandler::GetRewardsParameters(
    GetRewardsParametersCallback callback) {
  rewards_service_->GetRewardsParameters(std::move(callback));
}

void RewardsPageHandler::GetAvailableCountries(
    GetAvailableCountriesCallback callback) {
  auto get_countries_callback = [](decltype(callback) callback,
                                   mojom::AvailableCountryInfoPtr info,
                                   std::vector<std::string> country_codes) {
    info->country_codes = std::move(country_codes);
    std::move(callback).Run(std::move(info));
  };

  auto info = mojom::AvailableCountryInfo::New();
  info->default_country_code = rewards_service_->GetCountryCode();

  rewards_service_->GetAvailableCountries(base::BindOnce(
      get_countries_callback, std::move(callback), std::move(info)));
}

void RewardsPageHandler::GetRewardsPaymentId(
    GetRewardsPaymentIdCallback callback) {
  auto get_wallet_callback = [](decltype(callback) callback,
                                mojom::RewardsWalletPtr rewards_wallet) {
    std::string payment_id;
    if (rewards_wallet) {
      payment_id = rewards_wallet->payment_id;
    }
    std::move(callback).Run(std::move(payment_id));
  };

  rewards_service_->GetRewardsWallet(
      base::BindOnce(get_wallet_callback, std::move(callback)));
}

void RewardsPageHandler::GetCountryCode(GetCountryCodeCallback callback) {
  std::move(callback).Run(rewards_service_->GetCountryCode());
}

void RewardsPageHandler::GetExternalWallet(GetExternalWalletCallback callback) {
  rewards_service_->GetExternalWallet(std::move(callback));
}

void RewardsPageHandler::GetExternalWalletProviders(
    GetExternalWalletProvidersCallback callback) {
  std::move(callback).Run(rewards_service_->GetExternalWalletProviders());
}

void RewardsPageHandler::GetAdsSettings(GetAdsSettingsCallback callback) {
  auto settings = mojom::AdsSettings::New();

  settings->new_tab_page_ads_enabled =
      prefs_->GetBoolean(ntp_background_images::prefs::
                             kNewTabPageShowSponsoredImagesBackgroundImage);
  settings->notification_ads_enabled =
      prefs_->GetBoolean(brave_ads::prefs::kOptedInToNotificationAds);
  settings->search_ads_enabled =
      prefs_->GetBoolean(brave_ads::prefs::kOptedInToSearchResultAds);
  settings->inline_content_ads_enabled =
      prefs_->GetBoolean(brave_news::prefs::kBraveNewsOptedIn) &&
      prefs_->GetBoolean(brave_news::prefs::kNewTabPageShowToday);

  settings->notification_ads_per_hour =
      ads_service_->GetMaximumNotificationAdsPerHour();

  settings->current_subdivision = prefs_->GetString(
      brave_ads::prefs::kSubdivisionTargetingUserSelectedSubdivision);
  settings->auto_detected_subdivision = prefs_->GetString(
      brave_ads::prefs::kSubdivisionTargetingAutoDetectedSubdivision);
  settings->should_allow_subdivision_targeting =
      prefs_->GetBoolean(brave_ads::prefs::kShouldAllowSubdivisionTargeting);

  auto& subdivisions = brave_ads::GetSupportedSubdivisions();
  auto iter = subdivisions.find(brave_l10n::GetCountryCode(GetLocalState()));
  if (iter != subdivisions.cend()) {
    for (auto& [code, name] : iter->second) {
      auto entry = mojom::AdsSubdivision::New();
      entry->code = code;
      entry->name = name;
      settings->available_subdivisions.push_back(std::move(entry));
    }
  }

  std::move(callback).Run(std::move(settings));
}

void RewardsPageHandler::GetAdsStatement(GetAdsStatementCallback callback) {
  ads_service_->GetStatementOfAccounts(std::move(callback));
}

void RewardsPageHandler::GetAdsHistory(GetAdsHistoryCallback callback) {
  base::Time now = base::Time::Now();
  base::Time from_time =
      now - brave_ads::kAdHistoryRetentionPeriod.Get() - base::Days(1);

  auto on_history = [](decltype(callback) callback,
                       std::optional<base::Value::List> list) {
    // The Ads service provides Ads history data as a `base::Value` (i.e. JSON).
    // Rather than sending a Mojo `base::Value` interface to the client (which
    // is awkward to use in this context), send the data to the WebUI as a JSON
    // string. The front-end will send this JSON data back when the user
    // modifies Ads history state.

    if (!list) {
      // If there is no Ads history data, send an empty JSON array.
      list = base::Value::List();
    }

    std::string json;
    if (!base::JSONWriter::Write(*list, &json)) {
      LOG(ERROR) << "Unable to convert Ads history to JSON";
    }
    std::move(callback).Run(std::move(json));
  };

  ads_service_->GetAdHistory(from_time.LocalMidnight(), now,
                             base::BindOnce(on_history, std::move(callback)));
}

void RewardsPageHandler::SetAdTypeEnabled(brave_ads::mojom::AdType ad_type,
                                          bool enabled,
                                          SetAdTypeEnabledCallback callback) {
  using AdType = brave_ads::mojom::AdType;
  switch (ad_type) {
    case AdType::kNewTabPageAd:
      prefs_->SetBoolean(ntp_background_images::prefs::
                             kNewTabPageShowSponsoredImagesBackgroundImage,
                         enabled);
      break;
    case AdType::kNotificationAd:
      prefs_->SetBoolean(brave_ads::prefs::kOptedInToNotificationAds, enabled);
      break;
    case AdType::kSearchResultAd:
      prefs_->SetBoolean(brave_ads::prefs::kOptedInToSearchResultAds, enabled);
      break;
    case AdType::kPromotedContentAd:
    case AdType::kInlineContentAd:
    case AdType::kUndefined:
      // These Ad types cannot be enabled/disabled from the Rewards page.
      break;
  }
  std::move(callback).Run();
}

void RewardsPageHandler::SetNotificationAdsPerHour(
    int32_t ads_per_hour,
    SetNotificationAdsPerHourCallback callback) {
  DCHECK_GE(ads_per_hour, 0);
  prefs_->SetInt64(brave_ads::prefs::kMaximumNotificationAdsPerHour,
                   ads_per_hour);
  std::move(callback).Run();
}

void RewardsPageHandler::SetAdsSubdivision(const std::string& subdivision,
                                           SetAdsSubdivisionCallback callback) {
  prefs_->SetString(
      brave_ads::prefs::kSubdivisionTargetingUserSelectedSubdivision,
      subdivision);
  std::move(callback).Run();
}

void RewardsPageHandler::ToggleAdLike(const std::string& history_item,
                                      ToggleAdLikeCallback callback) {
  auto dict = base::JSONReader::ReadDict(history_item);
  if (!dict) {
    std::move(callback).Run();
    return;
  }

  // TODO(tmancey): Decouple to `BuildReaction`.
  const brave_ads::AdHistoryItemInfo ad_history_item =
      brave_ads::AdHistoryItemFromValue(*dict);
  auto reaction = brave_ads::mojom::ReactionInfo::New();
  reaction->type = static_cast<brave_ads::mojom::AdType>(ad_history_item.type);
  reaction->creative_instance_id = ad_history_item.creative_instance_id;
  reaction->creative_set_id = ad_history_item.creative_set_id;
  reaction->advertiser_id = ad_history_item.advertiser_id;
  reaction->segment = ad_history_item.segment;

  ads_service_->ToggleLikeAd(std::move(reaction),
                             base::IgnoreArgs<bool>(std::move(callback)));
}

void RewardsPageHandler::ToggleAdDislike(const std::string& history_item,
                                         ToggleAdDislikeCallback callback) {
  auto dict = base::JSONReader::ReadDict(history_item);
  if (!dict) {
    std::move(callback).Run();
    return;
  }

  // TODO(tmancey): Decouple to `BuildReaction`.
  const brave_ads::AdHistoryItemInfo ad_history_item =
      brave_ads::AdHistoryItemFromValue(*dict);
  auto reaction = brave_ads::mojom::ReactionInfo::New();
  reaction->type = static_cast<brave_ads::mojom::AdType>(ad_history_item.type);
  reaction->creative_instance_id = ad_history_item.creative_instance_id;
  reaction->creative_set_id = ad_history_item.creative_set_id;
  reaction->advertiser_id = ad_history_item.advertiser_id;
  reaction->segment = ad_history_item.segment;

  ads_service_->ToggleDislikeAd(std::move(reaction),
                                base::IgnoreArgs<bool>(std::move(callback)));
}

void RewardsPageHandler::ToggleAdInappropriate(
    const std::string& history_item,
    ToggleAdInappropriateCallback callback) {
  auto dict = base::JSONReader::ReadDict(history_item);
  if (!dict) {
    std::move(callback).Run();
    return;
  }

  // TODO(tmancey): Decouple to `BuildReaction`.
  const brave_ads::AdHistoryItemInfo ad_history_item =
      brave_ads::AdHistoryItemFromValue(*dict);
  auto reaction = brave_ads::mojom::ReactionInfo::New();
  reaction->type = static_cast<brave_ads::mojom::AdType>(ad_history_item.type);
  reaction->creative_instance_id = ad_history_item.creative_instance_id;
  reaction->creative_set_id = ad_history_item.creative_set_id;
  reaction->advertiser_id = ad_history_item.advertiser_id;
  reaction->segment = ad_history_item.segment;

  ads_service_->ToggleMarkAdAsInappropriate(
      std::move(reaction), base::IgnoreArgs<bool>(std::move(callback)));
}

void RewardsPageHandler::EnableRewards(const std::string& country_code,
                                       EnableRewardsCallback callback) {
  rewards_service_->CreateRewardsWallet(country_code, std::move(callback));
}

void RewardsPageHandler::BeginExternalWalletLogin(
    const std::string& provider,
    BeginExternalWalletLoginCallback callback) {
  rewards_service_->BeginExternalWalletLogin(provider, std::move(callback));
}

void RewardsPageHandler::ConnectExternalWallet(
    const std::string& provider,
    const base::flat_map<std::string, std::string>& args,
    ConnectExternalWalletCallback callback) {
  rewards_service_->ConnectExternalWallet(provider, args, std::move(callback));
}

void RewardsPageHandler::ResetRewards(ResetRewardsCallback callback) {
  rewards_service_->CompleteReset(std::move(callback));
}

void RewardsPageHandler::OnUpdate(UpdateSource update_source) {
  page_->OnRewardsStateUpdated();
}

}  // namespace brave_rewards
