/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_SPLIT_VIEW_SPLIT_VIEW_LOCATION_BAR_H_
#define BRAVE_BROWSER_UI_VIEWS_SPLIT_VIEW_SPLIT_VIEW_LOCATION_BAR_H_

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "components/prefs/pref_member.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Label;
class ImageView;
}  // namespace views

class PrefService;
class LocationBarModel;

// A simple version of the location bar for secondary web view.
// This location bar only shows host of the current page.
//  e.g. http://foo.bar.com/1234 => foo.bar.com
// When the scheme is not https, the location bar will show an icon to indicate
// the site might not be safe.
class SplitViewLocationBar : public views::WidgetDelegateView,
                             public content::WebContentsObserver,
                             public views::ViewObserver,
                             public ChromeLocationBarModelDelegate {
  METADATA_HEADER(SplitViewLocationBar, views::WidgetDelegateView)

 public:
  ~SplitViewLocationBar() override;

  static SplitViewLocationBar* Create(PrefService* prefs,
                                      views::View* web_view);

  void SetWebContents(content::WebContents* web_contents);

  // views::WidgetDelegateView:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnPaintBorder(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidChangeVisibleSecurityState() override;
  void WebContentsDestroyed() override;

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;
  bool ShouldDisplayURL() const override;

 private:
  friend class SplitViewLocationBarUnitTest;

  FRIEND_TEST_ALL_PREFIXES(SplitViewLocationBarUnitTest, GetURLForDisplay_HTTP);
  FRIEND_TEST_ALL_PREFIXES(SplitViewLocationBarUnitTest,
                           GetURLForDisplay_HTTPS);
  FRIEND_TEST_ALL_PREFIXES(SplitViewLocationBarUnitTest,
                           UpdateURLAndIcon_CertErrorShouldShowHTTPSwithStrike);
  FRIEND_TEST_ALL_PREFIXES(SplitViewLocationBarUnitTest,
                           UpdateIcon_InsecureContentsShouldShowWarningIcon);

  explicit SplitViewLocationBar(PrefService* prefs);

  void UpdateVisibility();
  void UpdateBounds();
  void UpdateURLAndIcon();
  void UpdateIcon();
  bool IsContentsSafe() const;
  bool HasCertError() const;
  std::u16string GetURLForDisplay() const;

  SkPath GetBorderPath(bool close);

  raw_ptr<PrefService> prefs_ = nullptr;

  std::unique_ptr<LocationBarModel> location_bar_model_;

  raw_ptr<views::ImageView> safety_icon_ = nullptr;
  raw_ptr<views::Label> https_with_strike_ = nullptr;
  raw_ptr<views::Label> scheme_separator_ = nullptr;
  raw_ptr<views::Label> url_ = nullptr;

  BooleanPrefMember prevent_url_elision_;

  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

BEGIN_VIEW_BUILDER(/*no export*/,
                   SplitViewLocationBar,
                   views::WidgetDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/*no export*/, SplitViewLocationBar)

#endif  // BRAVE_BROWSER_UI_VIEWS_SPLIT_VIEW_SPLIT_VIEW_LOCATION_BAR_H_
