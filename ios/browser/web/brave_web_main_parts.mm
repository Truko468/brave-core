/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/ios/browser/web/brave_web_main_parts.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "brave/ios/browser/application_context/brave_application_context_impl.h"
#include "ios/chrome/browser/application_context/model/application_context_impl.h"
#include "ios/chrome/browser/shared/model/paths/paths.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BraveWebMainParts::BraveWebMainParts(
    const base::CommandLine& parsed_command_line)
    : IOSChromeMainParts(parsed_command_line) {}

BraveWebMainParts::~BraveWebMainParts() {}

void BraveWebMainParts::PreCreateMainMessageLoop() {
  IOSChromeMainParts::PreCreateMainMessageLoop();

  // Add Brave Resource Pack
  base::FilePath brave_pack_path;
  base::PathService::Get(base::DIR_ASSETS, &brave_pack_path);
  brave_pack_path = brave_pack_path.AppendASCII("brave_resources.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      brave_pack_path, ui::kScaleFactorNone);
}

void BraveWebMainParts::PreMainMessageLoopRun() {
  IOSChromeMainParts::PreMainMessageLoopRun();
  static_cast<BraveApplicationContextImpl*>(application_context_.get())
      ->StartBraveServices();
}
