// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'
import { useDispatch } from 'react-redux'
import { useHistory, useParams } from 'react-router'
import Input, { InputEventDetail } from '@brave/leo/react/input'
import Dropdown from '@brave/leo/react/dropdown'
import {
  SelectItemEventDetail //
} from '@brave/leo/types/src/components/menu/menu.svelte'

// utils
import { FILECOIN_FORMAT_DESCRIPTION_URL } from '../../../../common/constants/urls'
import { getLocale, getLocaleWithTag } from '$web-common/locale'
import { copyToClipboard } from '../../../../utils/copy-to-clipboard'

// options
import { CreateAccountOptions } from '../../../../options/create-account-options'

// types
import {
  BraveWallet,
  CreateAccountOptionsType,
  WalletRoutes,
  FilecoinNetwork,
  FilecoinNetworkTypes,
  FilecoinNetworkLocaleMapping
} from '../../../../constants/types'

// actions
import { PanelActions } from '../../../../panel/actions'

// components
import { DividerLine } from '../../../extension/divider/index'
import { PopupModal } from '../index'
import { SelectAccountType } from './select-account-type/select-account-type'

// style
import { LeoSquaredButton } from '../../../shared/style'
import {
  CreateAccountStyledWrapper,
  DisclaimerText,
  ErrorText,
  ImportButton,
  ImportRow,
  StyledWrapper,
  Alert
} from './style'

// selectors
import { UISelectors } from '../../../../common/selectors'

// hooks
import { useSafeUISelector } from '../../../../common/hooks/use-safe-selector'
import {
  useImportAccountFromJsonMutation,
  useImportAccountMutation
} from '../../../../common/slices/api.slice'

interface Params {
  accountTypeName: string
}

const reduceFileName = (address: string) => {
  const firstHalf = address.slice(0, 4)
  const secondHalf = address.slice(-4)
  const reduced = firstHalf.concat('......', secondHalf)
  return reduced
}

export const ImportAccountModal = () => {
  // refs
  const passwordInputRef = React.useRef<HTMLInputElement>(null)

  // routing
  const history = useHistory()
  const { accountTypeName } = useParams<Params>()

  // redux
  const dispatch = useDispatch()

  // mutations
  const [importAccount] = useImportAccountMutation()
  const [importAccountFromJson] = useImportAccountFromJsonMutation()

  // memos
  const createAccountOptions = React.useMemo(() => {
    return CreateAccountOptions({
      isBitcoinEnabled: false, // No bitcoin imported accounts by now.
      isZCashEnabled: false // No zcash imported accounts by now.
    })
  }, [])

  const selectedAccountType = React.useMemo(() => {
    return createAccountOptions.find((option) => {
      return option.name.toLowerCase() === accountTypeName?.toLowerCase()
    })
  }, [accountTypeName, createAccountOptions])

  const isPanel = useSafeUISelector(UISelectors.isPanel)

  // state
  const [hasImportError, setHasImportError] = React.useState(false)
  const [accountName, setAccountName] = React.useState<string>('')
  const [filecoinNetwork, setFilecoinNetwork] =
    React.useState<FilecoinNetwork>('f')
  const [importOption, setImportOption] = React.useState<string>('key')
  const [privateKey, setPrivateKey] = React.useState<string>('')
  const [file, setFile] = React.useState<HTMLInputElement['files']>()
  const [password, setPassword] = React.useState<string>('')

  // computed
  const isAccountNameTooLong =
    accountName.length > BraveWallet.ACCOUNT_NAME_MAX_CHARACTER_LENGTH
  const hasAccountNameError = accountName === '' || isAccountNameTooLong
  const hasImportTypeError = importOption === 'key' ? !privateKey : !file
  const isDisabled = hasAccountNameError || hasImportTypeError
  const modalTitle = selectedAccountType
    ? getLocale('braveWalletCreateAccountImportAccount').replace(
        '$1',
        selectedAccountType.name
      )
    : getLocale('braveWalletAddAccountImport')

  const filPrivateKeyFormatDescriptionTextParts = getLocaleWithTag(
    'braveWalletFilImportPrivateKeyFormatDescription'
  )

  // methods
  const onClickClose = React.useCallback(() => {
    setHasImportError(false)
    history.push(WalletRoutes.Accounts)
  }, [history])

  const handleAccountNameChanged = React.useCallback(
    (detail: InputEventDetail) => {
      setAccountName(detail.value)
      setHasImportError(false)
    },
    []
  )

  const onChangeFilecoinNetwork = React.useCallback(
    (detail: SelectItemEventDetail) => {
      setFilecoinNetwork(detail.value as FilecoinNetwork)
    },
    []
  )

  const onChangeImportOption = React.useCallback(
    (detail: SelectItemEventDetail) => {
      setImportOption(detail.value as string)
    },
    []
  )

  const clearClipboard = React.useCallback(() => {
    copyToClipboard('')
  }, [])

  const handlePrivateKeyChanged = React.useCallback(
    (detail: InputEventDetail) => {
      clearClipboard()
      setPrivateKey(detail.value)
      setHasImportError(false)
    },
    [clearClipboard]
  )

  const onClickFileUpload = () => {
    // To prevent panel from being closed when file chooser is open
    if (isPanel) {
      dispatch(PanelActions.setCloseOnDeactivate(false))
      // For resume close on deactive when file chooser is close(select/cancel)
      window.addEventListener('focus', onFocusFileUpload)
    }
  }

  const onFocusFileUpload = () => {
    if (isPanel) {
      dispatch(PanelActions.setCloseOnDeactivate(true))
      window.removeEventListener('focus', onFocusFileUpload)
    }
  }

  const onFileUpload = React.useCallback(
    (file: React.ChangeEvent<HTMLInputElement>) => {
      if (file.target.files) {
        setFile(file.target.files)
        setHasImportError(false)
        passwordInputRef.current?.focus()
      }
    },
    []
  )

  const handlePasswordChanged = React.useCallback(
    (detail: InputEventDetail) => {
      setPassword(detail.value)
      setHasImportError(false)
      clearClipboard()
    },
    [clearClipboard]
  )

  const onClickCreateAccount = React.useCallback(async () => {
    if (importOption === 'key') {
      if (selectedAccountType?.coin === BraveWallet.CoinType.FIL) {
        try {
          await importAccount({
            accountName,
            privateKey,
            coin: BraveWallet.CoinType.FIL,
            network: filecoinNetwork
          })
          history.push(WalletRoutes.Accounts)
        } catch (error) {
          setHasImportError(true)
        }
      } else {
        try {
          await importAccount({
            accountName,
            privateKey,
            coin: selectedAccountType?.coin || BraveWallet.CoinType.ETH
          }).unwrap()
          history.push(WalletRoutes.Accounts)
        } catch (error) {
          setHasImportError(true)
        }
      }
      return
    }

    if (file) {
      const index = file[0]
      const reader = new FileReader()
      reader.onload = async function () {
        if (reader.result) {
          try {
            await importAccountFromJson({
              accountName,
              password,
              json: reader.result.toString().trim()
            }).unwrap()
            history.push(WalletRoutes.Accounts)
          } catch (error) {
            setHasImportError(true)
          }
        }
      }

      reader.readAsText(index)
    }
  }, [
    importOption,
    file,
    selectedAccountType?.coin,
    importAccount,
    accountName,
    privateKey,
    filecoinNetwork,
    history,
    importAccountFromJson,
    password
  ])

  const handleKeyDown = React.useCallback(
    (detail: InputEventDetail) => {
      if (isDisabled) {
        return
      }
      if ((detail.innerEvent as unknown as KeyboardEvent).key === 'Enter') {
        onClickCreateAccount()
      }
    },
    [isDisabled, onClickCreateAccount]
  )

  const onSelectAccountType = React.useCallback(
    (accountType: CreateAccountOptionsType) => () => {
      history.push(
        WalletRoutes.ImportAccountModal.replace(
          ':accountTypeName?',
          accountType.name.toLowerCase()
        )
      )
    },
    [history]
  )

  // render
  return (
    <PopupModal
      title={modalTitle}
      onClose={onClickClose}
    >
      <DividerLine />

      {!selectedAccountType && (
        <SelectAccountType
          createAccountOptions={createAccountOptions}
          buttonText={getLocale('braveWalletAddAccountImport')}
          onSelectAccountType={onSelectAccountType}
        />
      )}

      {selectedAccountType && (
        <StyledWrapper>
          <Alert type='warning'>
            {getLocale('braveWalletImportAccountDisclaimer')}
          </Alert>

          {selectedAccountType?.coin === BraveWallet.CoinType.FIL && (
            <Alert type='warning'>
              {filPrivateKeyFormatDescriptionTextParts.beforeTag}
              <a
                target='_blank'
                href={FILECOIN_FORMAT_DESCRIPTION_URL}
                rel='noopener noreferrer'
              >
                {filPrivateKeyFormatDescriptionTextParts.duringTag}
              </a>
              {filPrivateKeyFormatDescriptionTextParts.afterTag}
            </Alert>
          )}

          <CreateAccountStyledWrapper>
            {selectedAccountType?.coin === BraveWallet.CoinType.FIL && (
              <Dropdown
                value={filecoinNetwork}
                onChange={onChangeFilecoinNetwork}
              >
                <div slot='label'>
                  {getLocale('braveWalletAllowAddNetworkNetworkPanelTitle')}
                </div>

                <div slot='value'>
                  {FilecoinNetworkLocaleMapping[filecoinNetwork]}
                </div>

                {FilecoinNetworkTypes.map((network, index) => {
                  const networkLocale = FilecoinNetworkLocaleMapping[network]
                  return (
                    <leo-option
                      key={index}
                      value={network}
                    >
                      {networkLocale}
                    </leo-option>
                  )
                })}
              </Dropdown>
            )}

            {selectedAccountType?.coin === BraveWallet.CoinType.ETH && (
              <Dropdown
                value={importOption}
                onChange={onChangeImportOption}
              >
                <div slot='label'>
                  {getLocale('braveWalletPrivateKeyImportType')}
                </div>

                <div slot='value'>
                  {getLocale(
                    importOption === 'key'
                      ? 'braveWalletImportAccountKey'
                      : 'braveWalletImportAccountFile'
                  )}
                </div>

                <leo-option
                  key={'key'}
                  value='key'
                >
                  {getLocale('braveWalletImportAccountKey')}
                </leo-option>

                <leo-option
                  key={'file'}
                  value='file'
                >
                  {getLocale('braveWalletImportAccountFile')}
                </leo-option>
              </Dropdown>
            )}

            {hasImportError && (
              <ErrorText>
                {getLocale('braveWalletImportAccountError')}
              </ErrorText>
            )}

            {importOption === 'key' ? (
              <Input
                placeholder={getLocale('braveWalletImportAccountPlaceholder')}
                onBlur={clearClipboard}
                type='password'
                onInput={handlePrivateKeyChanged}
                onKeyDown={handleKeyDown}
              >
                {
                  // Label
                  getLocale('braveWalletImportAccountKey')
                }
              </Input>
            ) : (
              <>
                <ImportRow>
                  <ImportButton htmlFor='recoverFile'>
                    {getLocale('braveWalletImportAccountUploadButton')}
                  </ImportButton>
                  <DisclaimerText>
                    {file
                      ? reduceFileName(file[0].name)
                      : getLocale('braveWalletImportAccountUploadPlaceholder')}
                  </DisclaimerText>
                </ImportRow>
                <input
                  type='file'
                  id='recoverFile'
                  name='recoverFile'
                  style={{ display: 'none' }}
                  onChange={onFileUpload}
                  onClick={onClickFileUpload}
                />
                <Input
                  placeholder={getLocale('braveWalletInputLabelPassword')}
                  onInput={handlePasswordChanged}
                  onKeyDown={handleKeyDown}
                  onBlur={clearClipboard}
                  type='password'
                  ref={passwordInputRef}
                >
                  {
                    // Label
                    getLocale('braveWalletEnterPasswordIfApplicable')
                  }
                </Input>
              </>
            )}

            <Input
              value={accountName}
              placeholder={getLocale('braveWalletAddAccountPlaceholder')}
              onInput={handleAccountNameChanged}
              onKeyDown={handleKeyDown}
              showErrors={hasAccountNameError}
            >
              {
                // Label
                getLocale('braveWalletAddAccountPlaceholder')
              }

              <div slot='errors'>
                <ErrorText>
                  {isAccountNameTooLong
                    ? getLocale('braveWalletAccountNameTooLongError').replace(
                        '$1',
                        BraveWallet.ACCOUNT_NAME_MAX_CHARACTER_LENGTH.toString()
                      )
                    : ''}
                </ErrorText>
              </div>
              <div slot='extra'>
                {isAccountNameTooLong ? (
                  <ErrorText>
                    {accountName.length}/
                    {BraveWallet.ACCOUNT_NAME_MAX_CHARACTER_LENGTH}
                  </ErrorText>
                ) : (
                  <span>
                    {accountName.length}/
                    {BraveWallet.ACCOUNT_NAME_MAX_CHARACTER_LENGTH}
                  </span>
                )}
              </div>
            </Input>

            <LeoSquaredButton
              onClick={onClickCreateAccount}
              isDisabled={isDisabled}
              kind='filled'
            >
              {getLocale('braveWalletAddAccountImport')}
            </LeoSquaredButton>
          </CreateAccountStyledWrapper>
        </StyledWrapper>
      )}
    </PopupModal>
  )
}

export default ImportAccountModal
