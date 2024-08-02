// Copyright 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

import BraveCore
import SwiftUI

struct SendContainerView: View {

  let keyringStore: KeyringStore
  let cryptoStore: CryptoStore
  let destination: WalletActionDestination
  let completion: ((_ success: Bool) -> Void)?
  let dismissAction: () -> Void

  init(
    keyringStore: KeyringStore,
    cryptoStore: CryptoStore,
    destination: WalletActionDestination,
    completion: ((Bool) -> Void)? = nil,
    dismissAction: @escaping () -> Void
  ) {
    self.keyringStore = keyringStore
    self.cryptoStore = cryptoStore
    self.destination = destination
    self.completion = completion
    self.dismissAction = dismissAction
  }

  var body: some View {
    if BraveCore.FeatureList.kBraveWalletWebUIIOS.enabled {
      WebUISendView()
    } else {
      SendTokenView(
        keyringStore: keyringStore,
        networkStore: cryptoStore.networkStore,
        sendTokenStore: cryptoStore.openSendTokenStore(destination.initialToken),
        completion: completion,
        onDismiss: dismissAction
      )
    }
  }
}

struct WebUISendView: View {

  @State private var title: String = Strings.Wallet.send
  @Environment(\.openURL) private var openWalletURL

  var body: some View {
    NavigationStack {
      ChromeWebView(
        urlString: "brave://wallet/send",
        title: $title,
        openURL: { url in
          openWalletURL(url)
        }
      )
      .navigationTitle(title)
      .navigationBarTitleDisplayMode(.inline)
      .background(Color(braveSystemName: .containerBackground))
      .ignoresSafeArea(.keyboard, edges: .bottom)
    }
  }
}
