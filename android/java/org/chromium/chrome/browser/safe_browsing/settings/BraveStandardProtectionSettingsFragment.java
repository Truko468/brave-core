/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.chromium.chrome.browser.safe_browsing.settings;

import org.chromium.base.BraveReflectionUtil;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;

/**
 * Fragment containing standard protection settings.
 */
public class BraveStandardProtectionSettingsFragment extends StandardProtectionSettingsFragment {
    public void updateLeakDetectionAndExtendedReportingPreferences() {
        BraveReflectionUtil.InvokeMethod(StandardProtectionSettingsFragment.class, this,
                "updateLeakDetectionAndExtendedReportingPreferences");

        SafeBrowsingBridge.setSafeBrowsingExtendedReportingEnabled(false);
        getPreferenceScreen().removePreference(mExtendedReportingPreference);
        getPreferenceScreen().removePreference(mPasswordLeakDetectionPreference);
    }
}
