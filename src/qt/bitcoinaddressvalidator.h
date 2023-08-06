// Copyright (c) 2011-2020 The Sugarchain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINADDRESSVALIDATOR_H
#define BITCOIN_QT_BITCOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class SugarchainAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit SugarchainAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

/** Sugarchain address widget validator, checks for a valid sugarchain address.
 */
class SugarchainAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit SugarchainAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

#endif // BITCOIN_QT_BITCOINADDRESSVALIDATOR_H
