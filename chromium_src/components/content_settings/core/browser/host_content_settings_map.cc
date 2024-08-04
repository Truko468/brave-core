/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "components/content_settings/core/browser/host_content_settings_map.h"

#include "base/containers/contains.h"
#include "base/trace_event/trace_event.h"
#include "brave/components/content_settings/core/browser/remote_list_provider.h"
#include "brave/components/webcompat/content/browser/webcompat_exceptions_service.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"

#if !BUILDFLAG(IS_IOS)
#include "brave/components/content_settings/core/browser/brave_content_settings_pref_provider.h"
#define PrefProvider BravePrefProvider
#endif

namespace content_settings {
namespace {

bool IsMorePermissive_BraveImpl(ContentSettingsType content_type,
                                ContentSetting setting,
                                ContentSetting initial_setting) {
  // These types have additional logic for OffTheRecord profiles to always
  // return BLOCK (with a random timeout) instead of inheriting the setting.
  //
  // We must be careful to not break this, otherwise
  // ProcessIncognitoInheritanceBehavior() will return `initial_setting` which
  // is usually ASK and incorrect for OffTheRecord profiles.
  const ContentSettingsType kOffTheRecordAwareTypes[] = {
      ContentSettingsType::NOTIFICATIONS,
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
      ContentSettingsType::IDLE_DETECTION,
      ContentSettingsType::BRAVE_HTTPS_UPGRADE,
  };

  const bool is_more_permissive = IsMorePermissive(setting, initial_setting);
  if (is_more_permissive ||
      base::Contains(kOffTheRecordAwareTypes, content_type) ||
      base::FeatureList::IsEnabled(kAllowIncognitoPermissionInheritance)) {
    return is_more_permissive;
  }

  // If the type doesn't have special OffTheRecord handling, force
  // ProcessIncognitoInheritanceBehavior() to always return `initial_setting`.
  return true;
}

}  // namespace
}  // namespace content_settings

#define IsMorePermissive(a, b) IsMorePermissive_BraveImpl(content_type, a, b)

// Insert code into HostContentSettingsMap::ShutdownOnUIThread() body:
#define clear() \
  clear();      \
  webcompat::WebcompatExceptionsService::RemoveObserver(this)

#include "src/components/content_settings/core/browser/host_content_settings_map.cc"

#undef clear
#undef IsMorePermissive

void HostContentSettingsMap::RemoveRedundantWebcompatSettingsByType(
    ContentSettingsType settings_type) {
  auto* svc = webcompat::WebcompatExceptionsService::GetInstance();
  if (!svc) {
    return;
  }
  const auto& patterns = svc->GetPatterns(settings_type);
  for (const ContentSettingPatternSource& setting :
       GetSettingsForOneType(settings_type)) {
    if (setting.source == ProviderType::kPrefProvider) {
      const auto prefSettingValue =
          content_settings::ValueToContentSetting(setting.setting_value);
      bool patternExists = std::find(patterns.begin(), patterns.end(),
                                     setting.primary_pattern) != patterns.end();
      if ((prefSettingValue == CONTENT_SETTING_BLOCK && !patternExists) ||
          (prefSettingValue == CONTENT_SETTING_ALLOW && patternExists)) {
        SetContentSettingCustomScope(setting.primary_pattern,
                                     ContentSettingsPattern::Wildcard(),
                                     settings_type, CONTENT_SETTING_DEFAULT);
      }
    }
  }
}

// Removes all webcompat settings set by user in Prefs that are the same as
// those provided by the remote webcompat exceptions list.
void HostContentSettingsMap::RemoveRedundantWebcompatSettings() {
  for (auto settings_type = ContentSettingsType::BRAVE_WEBCOMPAT_NONE;
       settings_type != ContentSettingsType::BRAVE_WEBCOMPAT_ALL;
       settings_type = static_cast<ContentSettingsType>(
           static_cast<int32_t>(settings_type) + 1)) {
    RemoveRedundantWebcompatSettingsByType(settings_type);
  }
  RemoveRedundantWebcompatSettingsByType(
      ContentSettingsType::BRAVE_FINGERPRINTING_V2);
}

void HostContentSettingsMap::OnWebcompatRulesUpdated() {
  RemoveRedundantWebcompatSettings();
}

#if !BUILDFLAG(IS_IOS)
#undef PrefProvider
#undef PolicyProvider
#endif
