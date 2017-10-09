// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "chain.h"
#include "chainparams.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"
#include "bignum.h"

#include <math.h>

/**
 * Difficulty set by DiffMode
 * 1 = Normal difficulty adjust
 * 2 = Dr Kimoto's Gravity Well
 * 3 = Wrapper's eHRC (enhance Hash Rate Compensation)
**/
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    int nHeight = pindexLast->nHeight + 1;
    int DiffMode = 1;

    if (pindexLast->nHeight >= Params().ForkTwo())
        DiffMode = 2;

    if (nHeight >= Params().ForkThree() && nHeight < Params().ForkFour()) {
        // Difficulty reset after the switch
        if(nHeight == Params().ForkThree())
            return Params().ProofOfWorkLimit().GetCompact();

        // Use normal difficulty adjust following fork for 10 blocks
        if (nHeight <= Params().ForkThree() + 10)
            DiffMode = 1;
    } else if (nHeight >= Params().ForkFour()) {
        DiffMode = 3;
    }

    if (DiffMode == 2)
        return GetNextWorkRequired_V2(pindexLast);
    else if (DiffMode == 3)
        return GetNextWorkRequired_V3(pindexLast);

    return GetNextWorkRequired_V1(pindexLast, pblock);
}

unsigned int GetNextWorkRequired_V1(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    unsigned int nProofOfWorkLimit = Params().ProofOfWorkLimit().GetCompact();
    int64_t nTargetTimespan = Params().TargetTimespan();
    int64_t nInterval = Params().Interval();
    int nHeight = pindexLast->nHeight + 1;
    int64_t nReTargetHistoryFact = 4;

    if (nHeight >= Params().ForkOne())
    {
        nTargetTimespan = 60 * 60; // 1 hours
        nInterval = nTargetTimespan / Params().TargetSpacing();
		nReTargetHistoryFact = 2;
    }

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
    {
        if (Params().AllowMinDifficultyBlocks())
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + Params().TargetSpacing()*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % nInterval != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    int blockstogoback = nInterval - 1;
    if ((pindexLast->nHeight+1) != nInterval)
        blockstogoback = nInterval;
    if (pindexLast->nHeight > Params().CoinFix())
        blockstogoback = nReTargetHistoryFact * nInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = 0;
    if (pindexLast->nHeight > Params().CoinFix())
        nActualTimespan = (pindexLast->GetBlockTime() - pindexFirst->GetBlockTime()) / nReTargetHistoryFact;
    else
        nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();

    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;

    // Retarget
    uint256 bnNew;
    uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > Params().ProofOfWorkLimit())
        bnNew = Params().ProofOfWorkLimit();

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired_V2(const CBlockIndex* pindexLast)
{
    CBigNum bnProofOfWorkLimit;
    bnProofOfWorkLimit.SetCompact(504365055);
    int64_t nTargetTimespan = 60 * 60;
    int64_t PastSecondsMin = nTargetTimespan * 0.025;
    if(pindexLast->nHeight + 1 >= Params().ForkTwoA())
        PastSecondsMin = nTargetTimespan * 0.15;
    int64_t PastSecondsMax = nTargetTimespan * 7;
    uint64_t PastBlocksMin = PastSecondsMin / Params().TargetSpacing();
    uint64_t PastBlocksMax = PastSecondsMax / Params().TargetSpacing();
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    uint64_t PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double PastRateAdjustmentRatio = double(1);
    CBigNum PastDifficultyAverage;
    CBigNum PastDifficultyAveragePrev;
    double EventHorizonDeviation;
    double EventHorizonDeviationFast;
    double EventHorizonDeviationSlow;
        
    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin)
        return bnProofOfWorkLimit.GetCompact();

    int64_t LatestBlockTime = BlockLastSolved->GetBlockTime();
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++)
    {
        if (PastBlocksMax > 0 && i > PastBlocksMax)
            break;

        PastBlocksMass++;

        if (i == 1)
            PastDifficultyAverage.SetCompact(BlockReading->nBits);
        else
            PastDifficultyAverage = ((CBigNum().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev;

        PastDifficultyAveragePrev = PastDifficultyAverage;

        if (LatestBlockTime < BlockReading->GetBlockTime())
            LatestBlockTime = BlockReading->GetBlockTime();

        PastRateActualSeconds = LatestBlockTime - BlockReading->GetBlockTime();
        PastRateTargetSeconds = Params().TargetSpacing() * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);

        if (PastRateActualSeconds < 1)
            PastRateActualSeconds = 5;

        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);

        if(pindexLast->nHeight + 1 >= Params().ForkTwoA())
            EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass) / double(144)), -1.228));
        else
            EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass) / double(28.2)), -1.228));

        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin)
        {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast))
            {
                assert(BlockReading);
                break;
            }
        }

        if (BlockReading->pprev == NULL)
        {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    CBigNum bnNew(PastDifficultyAverage);
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
    {
        bnNew *= PastRateActualSeconds;
        bnNew /= PastRateTargetSeconds;
    }

    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    return bnNew.GetCompact();
}

/**
 * eHRC designed by Wrapper
 * Short, medium and long samples averaged together and compared against the target time span.
 * Adjust every block but limted to 9% change maximum.
*/
unsigned int GetNextWorkRequired_V3(const CBlockIndex* pindexLast)
{
    int nHeight = pindexLast->nHeight + 1;
    int nTargetTimespan = 90;
    int shortSample = 15;
    int mediumSample = 200;
    int longSample = 1000;
    int pindexFirstShortTime = 0;
    int pindexFirstMediumTime = 0;

    // Genesis block or new chain
    if (pindexLast == NULL || nHeight <= longSample)
        return Params().ProofOfWorkLimit().GetCompact();

    const CBlockIndex* pindexFirstLong = pindexLast;
    for(int i = 0; pindexFirstLong && i < longSample; i++) {
        pindexFirstLong = pindexFirstLong->pprev;
        if (i == shortSample - 1)
            pindexFirstShortTime = pindexFirstLong->GetBlockTime();

        if (i == mediumSample - 1)
            pindexFirstMediumTime = pindexFirstLong->GetBlockTime();
    }

    int nActualTimespanShort = (pindexLast->GetBlockTime() - pindexFirstShortTime) / shortSample;
    int nActualTimespanMedium = (pindexLast->GetBlockTime() - pindexFirstMediumTime)/ mediumSample;
    int nActualTimespanLong = (pindexLast->GetBlockTime() - pindexFirstLong->GetBlockTime()) / longSample;

    int nActualTimespan = (nActualTimespanShort + nActualTimespanMedium + nActualTimespanLong)/3;

    LogPrintf("RETARGET: nActualTimespanShort = %d, nActualTimespanMedium = %d, nActualTimespanLong = %d, nActualTimespan = %d\n",
    nActualTimespanShort, nActualTimespanMedium, nActualTimespanLong, nActualTimespan);

    // 9% difficulty limiter
    int nActualTimespanMax = nTargetTimespan * 494 / 453;
    int nActualTimespanMin = nTargetTimespan * 453 / 494;

    if(nActualTimespan < nActualTimespanMin)
        nActualTimespan = nActualTimespanMin;

    if(nActualTimespan > nActualTimespanMax)
        nActualTimespan = nActualTimespanMax;

    LogPrintf("RETARGET: nActualTimespan = %d after bounds\n", nActualTimespan);
    LogPrintf("RETARGET: nTargetTimespan = %d, nTargetTimespan/nActualTimespan = %.4f\n", nTargetTimespan, (float) nTargetTimespan/nActualTimespan);

    // Retarget
    uint256 bnNew;
    uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > Params().ProofOfWorkLimit())
        bnNew = Params().ProofOfWorkLimit();

    /// debug print
    LogPrintf("GetNextWorkRequired RETARGET\n");
    LogPrintf("nTargetTimespan() = %d    nActualTimespan = %d\n", nTargetTimespan, nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, bnOld.ToString());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    bool fNegative;
    bool fOverflow;
    uint256 bnTarget;

    if (Params().SkipProofOfWorkCheck())
       return true;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > Params().ProofOfWorkLimit())
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget)
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

uint256 GetBlockProof(const CBlockIndex& block)
{
    uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}
