// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_UI_COLOR_MISSING_COLOR_REF_MIXER_H_
#define BRAVE_UI_COLOR_MISSING_COLOR_REF_MIXER_H_

#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"

namespace ui {
void AddMissingRefColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key);
}

#endif  // BRAVE_UI_COLOR_MISSING_COLOR_REF_MIXER_H_
