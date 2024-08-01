/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_WEB_DISCOVERY_BROWSER_CONTENT_SCRAPER_H_
#define BRAVE_COMPONENTS_WEB_DISCOVERY_BROWSER_CONTENT_SCRAPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "brave/components/web_discovery/browser/document_extractor/rs/src/lib.rs.h"
#include "brave/components/web_discovery/browser/patterns.h"
#include "brave/components/web_discovery/browser/regex_util.h"
#include "brave/components/web_discovery/browser/server_config_loader.h"
#include "brave/components/web_discovery/common/web_discovery.mojom.h"
#include "url/gurl.h"

namespace web_discovery {

struct PageScrapeResult {
  PageScrapeResult(GURL url, std::string id);
  ~PageScrapeResult();

  PageScrapeResult(const PageScrapeResult&) = delete;
  PageScrapeResult& operator=(const PageScrapeResult&) = delete;

  base::Value SerializeToValue();
  static std::unique_ptr<PageScrapeResult> FromValue(const base::Value& dict);

  GURL url;
  // A map of DOM selectors to list of scraped values embedded in a Dict.
  // Each dict contains arbitrary keys (defined in the patterns) to scraped
  // values.
  base::flat_map<std::string, std::vector<base::Value::Dict>> fields;
  std::string id;

  // Only available for non-strict scrapes with "searchQuery"/"widgetTitle"
  // scrape rules
  std::optional<std::string> query;
};

// Extracts attribute values from the page DOM for reporting purposes.
// ContentScraper utilizes the following techniques:
//
// a) Extraction within the current page in the renderer (via `ScrapePage`).
//    The `mojom::DocumentExtractor` is used to request attribute values
//    from the current DOM in the view. Typically, this is used to exact a
//    search query, and decide whether the page is worthy of investigation
//    and reporting.
// b) Parsing and extracting HTML from a double fetch. This follows
//    the extraction in a). Used to extract all other needed details
//    from the page i.e. search results. Uses a Rust library for DOM
//    operations, in respect of Rule of Two.
class ContentScraper {
 public:
  using PageScrapeResultCallback =
      base::OnceCallback<void(std::unique_ptr<PageScrapeResult>)>;

  ContentScraper(const ServerConfigLoader* server_config_loader,
                 RegexUtil* regex_util);
  ~ContentScraper();

  ContentScraper(const ContentScraper&) = delete;
  ContentScraper& operator=(const ContentScraper&) = delete;

  // For initial page scrape in renderer
  void ScrapePage(const GURL& url,
                  bool is_strict_scrape,
                  mojom::DocumentExtractor* document_extractor,
                  PageScrapeResultCallback callback);
  // For subsequent double fetches after initial scrape
  void ParseAndScrapePage(const GURL& url,
                          bool is_strict_scrape,
                          std::unique_ptr<PageScrapeResult> prev_result,
                          std::string html,
                          PageScrapeResultCallback callback);

 private:
  void ProcessStandardRule(const std::string& report_key,
                           const ScrapeRule& rule,
                           const std::string& root_selector,
                           const GURL& url,
                           PageScrapeResult* scrape_result);
  void OnScrapedElementAttributes(
      bool is_strict_scrape,
      std::unique_ptr<PageScrapeResult> scrape_result,
      PageScrapeResultCallback callback,
      std::vector<mojom::AttributeResultPtr> attribute_results);
  void OnRustElementAttributes(
      bool is_strict_scrape,
      std::unique_ptr<PageScrapeResult> scrape_result,
      PageScrapeResultCallback callback,
      rust::Vec<rust_document_extractor::AttributeResult> attribute_results);

  std::optional<std::string> ExecuteRefineFunctions(
      const RefineFunctionList& function_list,
      std::string value);
  void ProcessAttributeValue(const ScrapeRuleGroup& rule_group,
                             PageScrapeResult& scrape_result,
                             std::string key,
                             std::optional<std::string> value_str,
                             base::Value::Dict& attribute_values);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  raw_ptr<const ServerConfigLoader> server_config_loader_;
  raw_ptr<RegexUtil> regex_util_;

  base::WeakPtrFactory<ContentScraper> weak_ptr_factory_{this};
};

}  // namespace web_discovery

#endif  // BRAVE_COMPONENTS_WEB_DISCOVERY_BROWSER_CONTENT_SCRAPER_H_
