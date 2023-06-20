// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import styled from 'styled-components'
import {
  FeedItemMetadata,
  Signal
} from 'gen/brave/components/brave_news/common/brave_news.mojom.m'
import Card from './card'
import * as React from 'react'
import Image from './Image'

const BigText = styled.div`
  font: var(--leo-font-heading-h3);
`
const Description = styled.p`
  max-height: 150px;
  overflow: hidden;
`

export default function HeroCard({
  article,
  signal
}: {
  article: FeedItemMetadata
  signal: Signal
}) {
  return (
    <BigText>
      <Card onClick={() => window.open(article.url.url, '_blank')}>
        {article.image.paddedImageUrl?.url && (
          <Image url={article.image.paddedImageUrl.url} />
        )}
        Hero:
        <b>{article.title}</b>
        <div>
          Publisher: {article.publisherName}
          <pre>({JSON.stringify(signal, null, 4)})</pre>
        </div>
        <Description>{article.description}</Description>
      </Card>
    </BigText>
  )
}
