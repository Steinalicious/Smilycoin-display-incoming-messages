// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionrecord.h"

#include "base58.h"
#include "wallet.h"

#include <stdint.h>

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    if (wtx.IsCoinBase())
    {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain())
        {
            return false;
        }
    }
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.GetTxTime();
    int64_t nCredit = wtx.GetCredit(true);
    int64_t nDebit = wtx.GetDebit();
    int64_t nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    if (nNet > 0 || wtx.IsCoinBase())
    {
        //
        // Credit
        //

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            if(wallet->IsMine(txout))
            {
                TransactionRecord sub(hash, nTime);

                CTxDestination address;
                sub.idx = parts.size(); // sequence number
                sub.credit = txout.nValue;

                for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++)
                {
                    const CTxOut& txout2 = wtx.vout[nOut];
                    std::string hexString = HexStr(txout2.scriptPubKey);
                    // If there is any data in the hexString convert it to ascii
                    if (hexString.substr(0,2) == "6a") {
                      std::string datadata = hexString.substr(4, hexString.size());
                      int len = datadata.length();
                      std::string newString;
                      for(int i=0; i<len; i+=2)
                      {
                          string byte = datadata.substr(i,2);
                          // Write a dot(.) if the hex code stands for a control command
                          if(byte == "00" || byte == "01" || byte == "02" || byte == "03" || byte == "04" || byte == "05" || byte == "06" || byte == "07" || byte == "08" || byte == "09" || byte == "0a" || byte == "0b" || byte == "0c" || byte == "0d" || byte == "0e" || byte == "0f" || byte == "10" || byte == "11" || byte == "12" || byte == "13" || byte == "14" || byte == "15" || byte == "16" || byte == "17" || byte == "18" || byte == "19" || byte == "1a" || byte == "1b" || byte == "1c" || byte == "1d" || byte == "1e" || byte == "1f" || byte == "20" || byte == "7f") {
                            byte = "2e";
                          }
                          char chr = (char) (int)strtol(byte.c_str(), NULL, 16);
                          newString.push_back(chr);
                      }
                      sub.data = newString;
                    }
                }

                if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*wallet, address))
                {
                    // Received by Bitcoin Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }
                if (wtx.IsCoinBase())
                {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }

                parts.append(sub);
            }
        }
    }
    else
    {
        bool fAllFromMe = true;
        BOOST_FOREACH(const CTxIn& txin, wtx.vin)
            fAllFromMe = fAllFromMe && wallet->IsMine(txin);

        bool fAllToMe = true;
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            fAllToMe = fAllToMe && wallet->IsMine(txout);

        if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            int64_t nChange = wtx.GetChange();

            parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                            -(nDebit - nChange), nCredit - nChange,""));
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            int64_t nTxFee = nDebit - wtx.GetValueOut();

            for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++)
            {
               const CTxOut& txout = wtx.vout[nOut];
               TransactionRecord sub(hash, nTime);
               sub.idx = parts.size();
               std::string hexString = HexStr(txout.scriptPubKey);
	       // If there is any data in the hexString convert it to ascii
               if (hexString.substr(0,2) == "6a") {
                 std::string datadata = hexString.substr(4, hexString.size());
                 int len = datadata.length();
                 std::string newString;
                 for(int i=0; i<len; i+=2)
                 {
                     string byte = datadata.substr(i,2);
                     // Write a dot(.) if the hex code stands for a control command
                     if(byte == "00" || byte == "01" || byte == "02" || byte == "03" || byte == "04" || byte == "05" || byte == "06" || byte == "07" || byte == "08" || byte == "09" || byte == "0a" || byte == "0b" || byte == "0c" || byte == "0d" || byte == "0e" || byte == "0f" || byte == "10" || byte == "11" || byte == "12" || byte == "13" || byte == "14" || byte == "15" || byte == "16" || byte == "17" || byte == "18" || byte == "19" || byte == "1a" || byte == "1b" || byte == "1c" || byte == "1d" || byte == "1e" || byte == "1f" || byte == "20" || byte == "7f") {
                        byte = "2e";
                      }
                     char chr = (char) (int)strtol(byte.c_str(), NULL, 16);
                     newString.push_back(chr);
                 }
                 sub.data = newString;
               }

                if(wallet->IsMine(txout))
                {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                CTxDestination address;
                if (ExtractDestination(txout.scriptPubKey, address))
                {
                    // Sent to Bitcoin Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
                }

                int64_t nValue = txout.nValue;
                /* Add fee to first output */
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                parts.append(sub);
            }
        }
        else
        {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0,""));
        }
    }

    return parts;
}

void TransactionRecord::updateStatus(const CWalletTx &wtx)
{
    AssertLockHeld(cs_main);
    // Determine transaction status

    // Find the block the tx is in
    CBlockIndex* pindex = NULL;
    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimeReceived,
        idx);
    status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity(chainActive.Height() - wtx.GetDepthInMainChain()) > 0);
    status.depth = wtx.GetDepthInMainChain();
    status.cur_num_blocks = chainActive.Height();

    if (!IsFinalTx(wtx, chainActive.Height() + 1))
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
        {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - chainActive.Height();
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }
    // For generated transactions, determine maturity
    else if(type == TransactionRecord::Generated)
    {
        if (wtx.GetBlocksToMaturity(status.depth) > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.IsInMainChain())
            {
                status.matures_in = wtx.GetBlocksToMaturity(status.cur_num_blocks - status.depth);

                // Check if the block was requested by anyone
                if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
                    status.status = TransactionStatus::MaturesWarning;
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
        {
            status.status = TransactionStatus::Offline;
        }
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
        }
        else if (status.depth < RecommendedNumConfirmations)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }

}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != chainActive.Height();
}

QString TransactionRecord::getTxID() const
{
    return formatSubTxId(hash, idx);
}

QString TransactionRecord::formatSubTxId(const uint256 &hash, int vout)
{
    return QString::fromStdString(hash.ToString() + strprintf("-%03d", vout));
}

