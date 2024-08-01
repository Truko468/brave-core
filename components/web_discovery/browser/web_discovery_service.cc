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
  content_scraper_ = nullptr;
  server_config_loader_ = nullptr;
  credential_manager_ = nullptr;

  profile_prefs_->ClearPref(kWebDiscoveryNativeEnabled);
  profile_prefs_->ClearPref(kAnonymousCredentialsDict);
  profile_prefs_->ClearPref(kCredentialRSAPrivateKey);
  profile_prefs_->ClearPref(kCredentialRSAPublicKey);
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
  auto payloads = GenerateQueryPayloads(
      server_config_loader_->GetLastServerConfig(), regex_util_,
      original_url_details, std::move(result));
}

}  // namespace web_discovery
