// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'
import { Meta, StoryObj } from '@storybook/react'

import { WalletSettingsMenu } from './wallet_settings_menu'
import WalletPanelStory from '../../../stories/wrappers/wallet-panel-story-wrapper'
import {
  SettingsButton,
  SettingsIcon
} from '../../../page/screens/shared-screen-components/tab-header/tab-header.style'

const meta: Meta<typeof WalletSettingsMenu> = {
  component: WalletSettingsMenu,
  args: {},
  render: (args) => {
    return (
      <WalletPanelStory>
        <WalletSettingsMenu
          anchor={
            <SettingsButton>
              <SettingsIcon name='more-vertical' />
            </SettingsButton>
          }
        />
      </WalletPanelStory>
    )
  }
} satisfies Meta<typeof WalletSettingsMenu>

export default meta
type Story = StoryObj<typeof meta>

export const _WalletSettingsMenu: Story = {} satisfies Story
