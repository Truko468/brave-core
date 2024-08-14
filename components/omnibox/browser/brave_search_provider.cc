/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/omnibox/browser/brave_search_provider.h"

#include "brave/components/omnibox/browser/brave_omnibox_prefs.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(IS_IOS)
// Got link error on IOS.
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#endif

namespace {

#if !BUILDFLAG(IS_IOS)
// Copied from chrome/browser/ui/omnibox/clipboard_utils.h
std::u16string GetClipboardText() {
  constexpr size_t kMaxClipboardTextLength = 500 * 1024;

  // Try text format.
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  if (clipboard->IsFormatAvailable(ui::ClipboardFormatType::PlainTextType(),
                                   ui::ClipboardBuffer::kCopyPaste,
                                   &data_dst)) {
    std::u16string text;
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &data_dst, &text);
    text = text.substr(0, kMaxClipboardTextLength);
    return OmniboxView::SanitizeTextForPaste(text);
  }

  return std::u16string();
}
#endif

}  // namespace

BraveSearchProvider::~BraveSearchProvider() = default;

void BraveSearchProvider::DoHistoryQuery(bool minimal_changes) {
  if (!client()->GetPrefs()->GetBoolean(omnibox::kHistorySuggestionsEnabled))
    return;

  SearchProvider::DoHistoryQuery(minimal_changes);
}

bool BraveSearchProvider::IsQueryPotentiallyPrivate() const {
  if (SearchProvider::IsQueryPotentiallyPrivate()) {
    return true;
  }

#if !BUILDFLAG(IS_IOS)
  if (!input_.text().empty() && GetClipboardText() == input_.text()) {
    // We don't want to send username/pwd in clipboard to suggest server
    // accidently.
    VLOG(2) << __func__
            << " : Treat input as private if it's same with clipboard text";
    return true;
  }
#endif

  return false;
}
