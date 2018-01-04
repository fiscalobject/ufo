// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"

namespace Consensus {
/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Used to check majorities for block version upgrade */
    int nMajorityEnforceBlockUpgrade;
    int nMajorityRejectBlockOutdated;
    int nMajorityWindow;
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    /** Hard fork parameters */
    int nCoinFix;
    int nHardForkOne;
    int nHardForkTwo;
    int nHardForkTwoA;
    int nHardForkThree;
    int nHardForkFour;
    int nHardForkFourA;
    unsigned int nNeoScryptSwitch;
    unsigned int nNeoScryptFork;
    int CoinFix() const { return nCoinFix; }
    int ForkOne() const { return nHardForkOne; }
    int ForkTwo() const { return nHardForkTwo; }
    int ForkTwoA() const { return nHardForkTwoA; }
    int ForkThree() const { return nHardForkThree; }
    int ForkFour() const { return nHardForkFour; }
    int ForkFourA() const { return nHardForkFourA; }
    unsigned int NeoScryptSwitch() const { return nNeoScryptSwitch;}
    unsigned int NeoScryptFork() const { return nNeoScryptFork;}
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
