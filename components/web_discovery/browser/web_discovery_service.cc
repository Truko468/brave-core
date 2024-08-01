/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/web_discovery/browser/web_discovery_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "brave/components/constants/pref_names.h"
#include "brave/components/web_discovery/browser/content_scraper.h"
#include "brave/components/web_discovery/browser/payload_generator.h"
#include "brave/components/web_discovery/browser/pref_names.h"
#include "brave/components/web_discovery/browser/privacy_guard.h"
#include "brave/components/web_discovery/browser/server_config_loader.h"
#include "brave/components/web_discovery/common/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace web_discovery {

namespace {
constexpr base::TimeDelta kAliveCheckInterval = base::Minutes(1);
constexpr size_t kMinPageCountForAliveMessage = 2;
}  // namespace

WebDiscoveryService::WebDiscoveryService(
    PrefService* local_state,
    PrefService* profile_prefs,
    base::FilePath user_data_dir,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : local_state_(local_state),
      profile_prefs_(profile_prefs),
      user_data_dir_(user_data_dir),
      shared_url_loader_factory_(shared_url_loader_factory) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (profile_prefs_->GetBoolean(kWebDiscoveryExtensionEnabled)) {
    profile_prefs_->ClearPref(kWebDiscoveryExtensionEnabled);
    profile_prefs_->SetBoolean(kWebDiscoveryNativeEnabled, true);
  }
#endif

  pref_change_registrar_.Init(profile_prefs);
  pref_change_registrar_.Add(
      kWebDiscoveryNativeEnabled,
      base::BindRepeating(&WebDiscoveryService::OnEnabledChange,
                          base::Unretained(this)));

  if (profile_prefs_->GetBoolean(kWebDiscoveryNativeEnabled)) {
    Start();
  }
}

WebDiscoveryService::~WebDiscoveryService() = default;

void WebDiscoveryService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPatternsRetrievalTime, {});
}

void WebDiscoveryService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kWebDiscoveryNativeEnabled, false);
  registry->RegisterDictionaryPref(kAnonymousCredentialsDict);
  registry->RegisterStringPref(kCredentialRSAPrivateKey, {});
  registry->RegisterStringPref(kCredentialRSAPublicKey, {});
  registry->RegisterListPref(kScheduledDoubleFetches);
  registry->RegisterListPref(kScheduledReports);
  registry->RegisterDictionaryPref(kUsedBasenameCounts);
  registry->RegisterDictionaryPref(kPageCounts);
}

void WebDiscoveryService::SetExtensionPrefIfNativeDisabled(
    PrefService* profile_prefs) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!base::FeatureList::IsEnabled(features::kWebDiscoveryNative) &&
      profile_prefs->GetBoolean(kWebDiscoveryNativeEnabled)) {
    profile_prefs->SetBoolean(kWebDiscoveryExtensionEnabled, true);
  }
#endif
}

void WebDiscoveryService::Start() {
  if (!server_config_loader_) {
    server_config_loader_ = std::make_unique<ServerConfigLoader>(
        local_state_, user_data_dir_, shared_url_loader_factory_.get(),
        base::BindRepeating(&WebDiscoveryService::OnConfigChange,
                            base::Unretained(this)),
        base::BindRepeating(&WebDiscoveryService::OnPatternsLoaded,
                            base::Unretained(this)));
    server_config_loader_->LoadConfigs();
  }
  if (!credential_manager_) {
    credential_manager_ = std::make_unique<CredentialManager>(
        profile_prefs_, shared_url_loader_factory_.get(),
        server_config_loader_.get());
  }
}

void WebDiscoveryService::Stop() {
  alive_message_timer_.Stop();
  reporter_ = nullptr;
  double_fetcher_ = nullptr;
  content_scraper_ = nullptr;
  server_config_loader_ = nullptr;
  credential_manager_ = nullptr;

  profile_prefs_->ClearPref(kWebDiscoveryNativeEnabled);
  profile_prefs_->ClearPref(kAnonymousCredentialsDict);
  profile_prefs_->ClearPref(kCredentialRSAPrivateKey);
  profile_prefs_->ClearPref(kCredentialRSAPublicKey);
  profile_prefs_->ClearPref(kScheduledDoubleFetches);
  profile_prefs_->ClearPref(kScheduledReports);
  profile_prefs_->ClearPref(kUsedBasenameCounts);
  profile_prefs_->ClearPref(kPageCounts);
}

void WebDiscoveryService::OnEnabledChange() {
  if (profile_prefs_->GetBoolean(kWebDiscoveryNativeEnabled)) {
    Start();
  } else {
    Stop();
  }
}

void WebDiscoveryService::OnConfigChange() {
  credential_manager_->JoinGroups();
}

void WebDiscoveryService::OnPatternsLoaded() {
  if (!content_scraper_) {
    content_scraper_ = std::make_unique<ContentScraper>(
        server_config_loader_.get(), &regex_util_);
  }
  if (!double_fetcher_) {
    double_fetcher_ = std::make_unique<DoubleFetcher>(
        profile_prefs_.get(), shared_url_loader_factory_.get(),
        base::BindRepeating(&WebDiscoveryService::OnDoubleFetched,
                            base::Unretained(this)));
  }
  if (!reporter_) {
    reporter_ = std::make_unique<Reporter>(
        profile_prefs_.get(), shared_url_loader_factory_.get(),
        credential_manager_.get(), &regex_util_, server_config_loader_.get());
  }
  MaybeSendAliveMessage();
}

void WebDiscoveryService::OnDoubleFetched(
    const GURL& url,
    const base::Value& associated_data,
    std::optional<std::string> response_body) {
  if (!response_body) {
    return;
  }
  auto prev_scrape_result = PageScrapeResult::FromValue(associated_data);
  if (!prev_scrape_result) {
    return;
  }
  content_scraper_->ParseAndScrapePage(
      url, true, std::move(prev_scrape_result), *response_body,
      base::BindOnce(&WebDiscoveryService::OnContentScraped,
                     base::Unretained(this), true));
}

void WebDiscoveryService::DidFinishLoad(
    const GURL& url,
    content::RenderFrameHost* render_frame_host) {
  if (!content_scraper_) {
    return;
  }
  const auto* matching_url_details =
      server_config_loader_->GetLastPatterns().GetMatchingURLPattern(url,
                                                                     false);
  if (!matching_url_details || !matching_url_details->is_search_engine) {
    if (!current_page_count_hour_key_.empty()) {
      ScopedDictPrefUpdate page_count_update(profile_prefs_, kPageCounts);
      auto existing_count =
          page_count_update->FindInt(current_page_count_hour_key_).value_or(0);
      page_count_update->Set(current_page_count_hour_key_, existing_count + 1);
    }
  }
  if (!matching_url_details) {
    return;
  }
  VLOG(1) << "URL matched pattern " << matching_url_details->id << ": " << url;
  if (IsPrivateURLLikely(regex_util_, url, matching_url_details)) {
    return;
  }
  mojo::Remote<mojom::DocumentExtractor> remote;
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      remote.BindNewPipeAndPassReceiver());
  auto remote_id = document_extractor_remotes_.Add(std::move(remote));
  content_scraper_->ScrapePage(
      url, false, document_extractor_remotes_.Get(remote_id),
      base::BindOnce(&WebDiscoveryService::OnContentScraped,
                     base::Unretained(this), false));
}

void WebDiscoveryService::OnContentScraped(
    bool is_strict,
    std::unique_ptr<PageScrapeResult> result) {
  if (!result) {
    return;
  }
  const auto& patterns = server_config_loader_->GetLastPatterns();
  auto* original_url_details =
      patterns.GetMatchingURLPattern(result->url, is_strict);
  if (!original_url_details) {
    return;
  }
  if (!is_strict && original_url_details->is_search_engine) {
    auto* strict_url_details =
        patterns.GetMatchingURLPattern(result->url, true);
    if (strict_url_details) {
      auto url = result->url;
      if (!result->query) {
        return;
      }
      if (IsPrivateQueryLikely(regex_util_, *result->query)) {
        return;
      }
      url = GeneratePrivateSearchURL(url, *result->query, *strict_url_details);
      VLOG(1) << "Double fetching search page: " << url;
      double_fetcher_->ScheduleDoubleFetch(url, result->SerializeToValue());
    }
  }
  auto payloads = GenerateQueryPayloads(
      server_config_loader_->GetLastServerConfig(), regex_util_,
      original_url_details, std::move(result));
  for (auto& payload : payloads) {
    reporter_->ScheduleSend(std::move(payload));
  }
}

bool WebDiscoveryService::UpdatePageCountStartTime() {
  auto now = base::Time::Now();
  if (!current_page_count_start_time_.is_null() &&
      (now - current_page_count_start_time_) < base::Hours(1)) {
    return false;
  }
  base::Time::Exploded exploded;
  now.UTCExplode(&exploded);
  exploded.millisecond = 0;
  exploded.second = 0;
  exploded.minute = 0;
  if (!base::Time::FromUTCExploded(exploded, &current_page_count_start_time_)) {
    return false;
  }
  current_page_count_hour_key_ =
      base::StringPrintf("%04d%02d%02d%02d", exploded.year, exploded.month,
                         exploded.day_of_month, exploded.hour);
  return true;
}

void WebDiscoveryService::MaybeSendAliveMessage() {
  if (!alive_message_timer_.IsRunning()) {
    alive_message_timer_.Start(
        FROM_HERE, kAliveCheckInterval,
        base::BindRepeating(&WebDiscoveryService::MaybeSendAliveMessage,
                            base::Unretained(this)));
  }
  if (!UpdatePageCountStartTime()) {
    return;
  }
  ScopedDictPrefUpdate update(profile_prefs_, kPageCounts);
  for (auto it = update->begin(); it != update->end();) {
    if (it->first == current_page_count_hour_key_) {
      it++;
      continue;
    }
    if (it->second.is_int() && static_cast<size_t>(it->second.GetInt()) >=
                                   kMinPageCountForAliveMessage) {
      reporter_->ScheduleSend(GenerateAlivePayload(
          server_config_loader_->GetLastServerConfig(), it->first));
    }
    it = update->erase(it);
  }
}

}  // namespace web_discovery
