/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
#define BRAVE_CHROMIUM_SRC_COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_

#include "brave/components/webcompat/content/browser/webcompat_exceptions_observer.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"

#define PREF_PROVIDER PREF_PROVIDER, REMOTE_LIST_PROVIDER

#define RefcountedKeyedService \
  RefcountedKeyedService, public webcompat::WebcompatExceptionsObserver

#define FlushLossyWebsiteSettings()            \
  RemoveRedundantWebcompatSettings();          \
  void RemoveRedundantWebcompatSettingsByType( \
      ContentSettingsType settings_type);      \
  void OnWebcompatRulesUpdated() override;     \
  void FlushLossyWebsiteSettings()

#include "src/components/content_settings/core/browser/host_content_settings_map.h"  // IWYU pragma: export

#undef FlushLossyWebsiteSettings
#undef RefcountedKeyedService
#undef PREF_PROVIDER

#endif  // BRAVE_CHROMIUM_SRC_COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
