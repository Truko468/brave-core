/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_WEB_DISCOVERY_BROWSER_RSA_H_
#define BRAVE_COMPONENTS_WEB_DISCOVERY_BROWSER_RSA_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "crypto/rsa_private_key.h"

namespace web_discovery {

struct RSAKeyInfo {
  RSAKeyInfo();
  ~RSAKeyInfo();
  std::unique_ptr<crypto::RSAPrivateKey> key_pair;
  std::optional<std::string> private_key_b64;
  std::string public_key_b64;
};

std::unique_ptr<RSAKeyInfo> GenerateRSAKeyPair();

std::unique_ptr<RSAKeyInfo> ImportRSAKeyPair(
    const std::string& private_key_b64);

std::optional<std::string> RSASign(crypto::RSAPrivateKey* key,
                                   base::span<uint8_t> message);

}  // namespace web_discovery

#endif  // BRAVE_COMPONENTS_WEB_DISCOVERY_BROWSER_RSA_H_
