/* Copyright (c) 2019 The Brave Software Team. Distributed under the MPL2
 * license. This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/net/brave_ad_block_tp_network_delegate_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/task/thread_pool.h"
#include "brave/browser/brave_browser_process.h"
#include "brave/browser/net/url_context.h"
#include "brave/common/network_constants.h"
#include "brave/components/brave_shields/browser/ad_block_service.h"
#include "brave/test/base/testing_brave_browser_process.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using brave::ResponseCallback;

class TestingBraveComponentUpdaterDelegate : public BraveComponent::Delegate {
 public:
  TestingBraveComponentUpdaterDelegate()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}
  ~TestingBraveComponentUpdaterDelegate() override {}

  using ComponentObserver = update_client::UpdateClient::Observer;
  // brave_component_updater::BraveComponent::Delegate implementation
  void Register(const std::string& component_name,
                const std::string& component_base64_public_key,
                base::OnceClosure registered_callback,
                BraveComponent::ReadyCallback ready_callback) override {}
  bool Unregister(const std::string& component_id) override { return true; }
  void OnDemandUpdate(const std::string& component_id) override {}

  void AddObserver(ComponentObserver* observer) override {}
  void RemoveObserver(ComponentObserver* observer) override {}

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override {
    return task_runner_;
  }

  const std::string locale() const override { return "en"; }
  PrefService* local_state() override { return pref_service_.get(); }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<PrefService> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(TestingBraveComponentUpdaterDelegate);
};

class BraveAdBlockTPNetworkDelegateHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    brave_component_updater_delegate_ =
        std::make_unique<TestingBraveComponentUpdaterDelegate>();

    auto adblock_service = brave_shields::AdBlockServiceFactory(
        brave_component_updater_delegate_.get());

    TestingBraveBrowserProcess::GetGlobal()->SetAdBlockService(
        std::move(adblock_service));

    g_brave_browser_process->ad_block_service()->Start();
  }

  void ResetAdblockInstance(brave_shields::AdBlockBaseService* service,
                            std::string rules,
                            std::string resources) {
    service->ResetForTest(rules, resources);
  }

  // Serves as `next_callback` for the request handler
  void OnCompletion() {}

  // Returns true if the request handler deferred control back to the calling
  // thread before completion, or true if it completed instantly.
  bool CheckRequest(std::shared_ptr<brave::BraveRequestInfo> request_info) {
    // request_identifier must be nonzero, or else nothing will be tested
    request_info->request_identifier = 1;

    auto cb = base::Bind(&BraveAdBlockTPNetworkDelegateHelperTest::OnCompletion,
                         base::Unretained(this));

    int rc = OnBeforeURLRequest_AdBlockTPPreWork(cb, request_info);
    EXPECT_TRUE(rc == net::OK || rc == net::ERR_IO_PENDING);

    task_environment_.RunUntilIdle();

    return rc == net::ERR_IO_PENDING;
  }

  std::unique_ptr<TestingBraveComponentUpdaterDelegate>
      brave_component_updater_delegate_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BraveAdBlockTPNetworkDelegateHelperTest, NoInitiatorURL) {
  const GURL url("https://bradhatesprimes.brave.com/composite_numbers_ftw");
  auto request_info = std::make_shared<brave::BraveRequestInfo>(url);
  request_info->resource_type = blink::mojom::ResourceType::kScript;

  bool handled = CheckRequest(request_info);

  EXPECT_FALSE(handled);
  EXPECT_EQ(request_info->blocked_by, brave::kNotBlocked);
  EXPECT_TRUE(request_info->new_url_spec.empty());
}

TEST_F(BraveAdBlockTPNetworkDelegateHelperTest, EmptyRequestURL) {
  auto request_info = std::make_shared<brave::BraveRequestInfo>(GURL());
  request_info->initiator_url = GURL("https://example.com");
  request_info->resource_type = blink::mojom::ResourceType::kScript;

  bool handled = CheckRequest(request_info);

  EXPECT_FALSE(handled);
  EXPECT_EQ(request_info->blocked_by, brave::kNotBlocked);
  EXPECT_TRUE(request_info->new_url_spec.empty());
}

TEST_F(BraveAdBlockTPNetworkDelegateHelperTest, DevToolURL) {
  const GURL url("devtools://devtools/");
  auto request_info = std::make_shared<brave::BraveRequestInfo>(url);
  request_info->initiator_url =
      GURL("devtools://devtools/bundled/root/root.js");
  request_info->resource_type = blink::mojom::ResourceType::kScript;

  bool handled = CheckRequest(request_info);

  EXPECT_FALSE(handled);
  EXPECT_EQ(request_info->blocked_by, brave::kNotBlocked);
  EXPECT_TRUE(request_info->new_url_spec.empty());
}

TEST_F(BraveAdBlockTPNetworkDelegateHelperTest, SimpleBlocking) {
  ResetAdblockInstance(g_brave_browser_process->ad_block_service(),
                       "||brave.com/test.txt", "");

  const GURL url("https://brave.com/test.txt");
  auto request_info = std::make_shared<brave::BraveRequestInfo>(url);
  request_info->request_identifier = 1;
  request_info->resource_type = blink::mojom::ResourceType::kScript;
  request_info->initiator_url = GURL("https://brave.com");

  bool handled = CheckRequest(request_info);

  EXPECT_TRUE(handled);
  EXPECT_EQ(request_info->blocked_by, brave::kAdBlocked);
  EXPECT_TRUE(request_info->new_url_spec.empty());
}
