/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_DOM_WINDOW_H_
#define BRAVE_CHROMIUM_SRC_THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_DOM_WINDOW_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

#define SetStorageKey                                                        \
  SetEphemeralStorageOrigin(const SecurityOrigin* ephemeral_storage_origin); \
  const SecurityOrigin* GetEphemeralStorageOrigin() const;                   \
  const BlinkStorageKey& GetEphemeralStorageKeyOrStorageKey() const;         \
                                                                             \
 private:                                                                    \
  absl::optional<BlinkStorageKey> ephemeral_storage_key_;                    \
                                                                             \
 public:                                                                     \
  void SetStorageKey

#define outerHeight                 \
  outerHeight_ChromiumImpl() const; \
  int outerHeight

#define outerWidth                 \
  outerWidth_ChromiumImpl() const; \
  int outerWidth

#define screenLeft              \
  screenX_ChromiumImpl() const; \
  int screenLeft

#define screenTop               \
  screenY_ChromiumImpl() const; \
  int screenTop

#include "src/third_party/blink/renderer/core/frame/local_dom_window.h"

#undef SetStorageKey
#undef outerHeight
#undef outerWidth
#undef screenLeft
#undef screenTop

#endif  // BRAVE_CHROMIUM_SRC_THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_DOM_WINDOW_H_
