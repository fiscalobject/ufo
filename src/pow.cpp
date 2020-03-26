// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <bignum.h>

#include <math.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    int nHeight = pindexLast->nHeight + 1;

    int DiffMode = 1;
    if (pindexLast->nHeight >= params.nHardForkTwo)
        DiffMode = 2;

    // Difficulty reset after the switch
    if(nHeight == params.nHardForkThree)
        return UintToArith256(params.powLimit).GetCompact();

    // Use normal difficulty adjust following fork for 10 blocks
    if (nHeight >= params.nHardForkThree && nHeight <= params.nHardForkThree + 10)
        DiffMode = 1;

    if (nHeight >= params.nHardForkFour)
        DiffMode = 3;

    if (DiffMode == 2)
        return GetNextWorkRequired_V2(pindexLast, params);
    else if (DiffMode == 3)
        return GetNextWorkRequired_V3(pindexLast, params);

     return GetNextWorkRequired_V1(pindexLast, pblock, params);
}

unsigned int GetNextWorkRequired_V1(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    int nHeight = pindexLast->nHeight + 1;
    int64_t nReTargetHistoryFact = 4;
    int64_t nTargetTimespan = params.nPowTargetTimespan;
    int64_t nInterval = params.DifficultyAdjustmentInterval();
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    if (nHeight >= params.nHardForkOne) {
        nTargetTimespan = 60 * 60; // 1 hours
        nInterval = nTargetTimespan / params.nPowTargetSpacing;
        nReTargetHistoryFact = 2;
    }

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % nInterval  != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % nInterval  != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nInterval  - 1;
    if ((pindexLast->nHeight + 1) != nInterval )
        blockstogoback = nInterval ;
    if (pindexLast->nHeight > params.nCoinFix)
        blockstogoback = nReTargetHistoryFact * nInterval ;

    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = 0;
    if (pindexLast->nHeight > params.nCoinFix)
        nActualTimespan = (pindexLast->GetBlockTime() - pindexFirst->GetBlockTime()) / nReTargetHistoryFact;
    else
        nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();

    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;

    // Retarget
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired_V2(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    CBigNum bnProofOfWorkLimit;
    bnProofOfWorkLimit.SetCompact(504365055);
    int64_t nTargetTimespan = 60 * 60;
    int64_t PastSecondsMin = nTargetTimespan * 0.025;
    if(pindexLast->nHeight + 1 >= params.nHardForkTwoA)
        PastSecondsMin = nTargetTimespan * 0.15;
    int64_t PastSecondsMax = nTargetTimespan * 7;
    uint64_t PastBlocksMin = PastSecondsMin / params.nPowTargetSpacing;
    uint64_t PastBlocksMax = PastSecondsMax / params.nPowTargetSpacing;
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
        PastRateTargetSeconds = params.nPowTargetSpacing * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);

        if (PastRateActualSeconds < 1)
            PastRateActualSeconds = 5;

        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);

        if(pindexLast->nHeight + 1 >= params.nHardForkTwoA)
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
 * eHRC designed by Wrapper and Wellenreiter
 * Short, medium and long samples averaged together and compared against the target time span.
 * Adjust every block but limted to 9% change maximum.
*/
unsigned int GetNextWorkRequired_V3(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    int nHeight = pindexLast->nHeight + 1;
    int nTargetTimespan = 90;
    int shortSample = 15;
    int mediumSample = 200;
    int longSample = 1000;
    int pindexFirstShortTime = 0;
    int pindexFirstMediumTime = 0;
    int nActualTimespan = 0;
    int nActualTimespanShort = 0;
    int nActualTimespanMedium = 0;
    int nActualTimespanLong = 0;

    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Genesis block or new chain
    if (pindexLast == NULL || nHeight <= longSample + 1)
        return UintToArith256(params.powLimit).GetCompact();

    const CBlockIndex* pindexFirstLong = pindexLast;
    for(int i = 0; pindexFirstLong && i < longSample; i++) {
        pindexFirstLong = pindexFirstLong->pprev;
        if (i == shortSample - 1)
            pindexFirstShortTime = pindexFirstLong->GetBlockTime();

        if (i == mediumSample - 1)
            pindexFirstMediumTime = pindexFirstLong->GetBlockTime();
    }

    if (pindexLast->GetBlockTime() - pindexFirstShortTime != 0)
        nActualTimespanShort = (pindexLast->GetBlockTime() - pindexFirstShortTime) / shortSample;

    if (pindexLast->GetBlockTime() - pindexFirstMediumTime != 0)
        nActualTimespanMedium = (pindexLast->GetBlockTime() - pindexFirstMediumTime)/ mediumSample;

    if (pindexLast->GetBlockTime() - pindexFirstLong->GetBlockTime() != 0)
        nActualTimespanLong = (pindexLast->GetBlockTime() - pindexFirstLong->GetBlockTime()) / longSample;

    int nActualTimespanSum = nActualTimespanShort + nActualTimespanMedium + nActualTimespanLong;

    if (nActualTimespanSum != 0)
        nActualTimespan = nActualTimespanSum / 3;

    if (nHeight >= params.nHardForkFourA) {
        // Apply .25 damping
        nActualTimespan = nActualTimespan + (3 * nTargetTimespan);
        nActualTimespan /= 4;
    }

    // 9% difficulty limiter
    int nActualTimespanMax = nTargetTimespan * 494 / 453;
    int nActualTimespanMin = nTargetTimespan * 453 / 494;

    if(nActualTimespan < nActualTimespanMin)
        nActualTimespan = nActualTimespanMin;

    if(nActualTimespan > nActualTimespanMax)
        nActualTimespan = nActualTimespanMax;

    // Retarget
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > UintToArith256(params.powLimit))
        bnNew = UintToArith256(params.powLimit);

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
