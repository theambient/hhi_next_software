/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2010-2017, ITU/ISO/IEC
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  * Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
*    be used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

/** \file     UnitTool.h
 *  \brief    defines operations for basic units
 */

#ifndef __UNITTOOLS__
#define __UNITTOOLS__

#include "Unit.h"
#include "UnitPartitioner.h"
#include "ContextModelling.h"
#include "InterPrediction.h"

// CS tools
namespace CS
{
  UInt64 getEstBits                   ( const CodingStructure &cs );
  void   initFrucMvp                  (       CodingStructure &cs );
  UnitArea getArea                    ( const CodingStructure &cs, const UnitArea &area );
  bool   isDoubleITree                ( const CodingStructure &cs );
}


// CU tools
namespace CU
{
  bool isIntra                        (const CodingUnit &cu);
  bool isInter                        (const CodingUnit &cu);
  bool isRDPCMEnabled                 (const CodingUnit &cu);
  UInt getQuadtreeTULog2MinSizeInCU   (const CodingUnit &cu);
  bool isLosslessCoded                (const CodingUnit &cu);
  UInt getIntraSizeIdx                (const CodingUnit &cu);

  bool isSameCtu                      (const CodingUnit &cu, const CodingUnit &cu2);
  bool isSameSlice                    (const CodingUnit &cu, const CodingUnit &cu2);
  bool isSameTile                     (const CodingUnit &cu, const CodingUnit &cu2);
  bool isSameSliceAndTile             (const CodingUnit &cu, const CodingUnit &cu2);
  bool isLastSubCUOfCtu               (const CodingUnit &cu);
  UInt getCtuAddr                     (const CodingUnit &cu);

  int  predictQP                      (const CodingUnit& cu, const int prevQP );
  bool isQGStart                      (const CodingUnit& cu); // check if start of a Quantization Group

  UInt getNumPUs                      (const CodingUnit& cu);
  void addPUs                         (      CodingUnit& cu);

  PartSplit getSplitAtDepth           (const CodingUnit& cu, const unsigned depth);

  bool hasNonTsCodedBlock             (const CodingUnit& cu);
  UInt getNumNonZeroCoeffNonTs        (const CodingUnit& cu);
  bool isLICFlagPresent               (const CodingUnit& cu);
  bool isObmcFlagCoded                (const CodingUnit& cu);

  PUTraverser traversePUs             (      CodingUnit& cu);
  TUTraverser traverseTUs             (      CodingUnit& cu);
  cPUTraverser traversePUs            (const CodingUnit& cu);
  cTUTraverser traverseTUs            (const CodingUnit& cu);

  bool  hasSubCUNonZeroMVd            (const CodingUnit& cu);
  int   getMaxNeighboriMVCandNum      (const CodingStructure& cs, const Position& pos);
  void  resetMVDandMV2Int             (      CodingUnit& cu, InterPrediction *interPred );
}
// PU tools
namespace PU
{
  int  getIntraMPMs                   (const PredictionUnit &pu, unsigned *mpm, const ChannelType &channelType = CHANNEL_TYPE_LUMA, const bool isChromaMDMS = false, const unsigned startIdx = 0 );
  int  getLMSymbolList                (const PredictionUnit &pu, Int *pModeList);
  int  getDMModes                     (const PredictionUnit &pu, unsigned *modeList);
  void getIntraChromaCandModes        (const PredictionUnit &pu, unsigned modeList[NUM_CHROMA_MODE]);
  UInt getFinalIntraMode              (const PredictionUnit &pu, const ChannelType &chType);

  void getInterMergeCandidates        (const PredictionUnit &pu, MergeCtx& mrgCtx, const int& mrgCandIdx = -1 );
  bool isDiffMER                      (const PredictionUnit &pu, const PredictionUnit &pu2);
  bool getColocatedMVP                (const PredictionUnit &pu, const RefPicList &eRefPicList, const Position &pos, Mv& rcMv, const int &refIdx, bool* pLICFlag = 0 );
  void fillMvpCand                    (      PredictionUnit &pu, const RefPicList &eRefPicList, const int &refIdx, AMVPInfo &amvpInfo, InterPrediction *interPred = NULL);
  void fillAffineMvpCand              (      PredictionUnit &pu, const RefPicList &eRefPicList, const int &refIdx, AffineAMVPInfo &affiAMVPInfo);
  bool addMVPCandUnscaled             (const PredictionUnit &pu, const RefPicList &eRefPicList, const int &iRefIdx, const Position &pos, const MvpDir &eDir, AMVPInfo &amvpInfo, bool affine = false);
  bool addMVPCandWithScaling          (const PredictionUnit &pu, const RefPicList &eRefPicList, const int &iRefIdx, const Position &pos, const MvpDir &eDir, AMVPInfo &amvpInfo, bool affine = false);
  bool isBipredRestriction            (const PredictionUnit &pu);
  void spanMotionInfo                 (      PredictionUnit &pu, const MergeCtx &mrgCtx = MergeCtx() );
  void spanLICFlags                   (      PredictionUnit &pu, const bool LICFlag );

  bool getInterMergeSubPuMvpCand      (const PredictionUnit &pu, MergeCtx &mrgCtx, bool& LICFlag, const int count );
  bool getInterMergeSubPuRecurCand    (const PredictionUnit &pu, MergeCtx &mrgCtx, const int count );
  void applyImv                       (      PredictionUnit &pu, MergeCtx &mrgCtx, InterPrediction *interPred = NULL );
  bool isAffineMrgFlagCoded           (const PredictionUnit &pu );
  void getAffineMergeCand             (const PredictionUnit &pu, MvField (*mvFieldNeighbours)[3], unsigned char &interDirNeighbours, int &numValidMergeCand );
  void setAllAffineMvField            (      PredictionUnit &pu, MvField *mvField, RefPicList eRefList );
  void setAllAffineMv                 (      PredictionUnit &pu, Mv affLT, Mv affRT, Mv affLB, RefPicList eRefList );
  void setAllAffineMvd                (      MotionBuf mb, const Mv& affLT, const Mv& affRT, RefPicList eRefList, Bool useQTBT );

  bool isBIOLDB                       (const PredictionUnit &pu);

  void restrictBiPredMergeCands       (const PredictionUnit &pu, MergeCtx& mrgCtx);

  bool getNeighborMotion              (      PredictionUnit &pu, MotionInfo& mi, Position off, Int iDir, Bool bSubPu );

  bool getMvPair                      (const PredictionUnit &pu, RefPicList eCurRefPicList, const MvField & rCurMvField, MvField &rMvPair);
  bool isSameMVField                  (const PredictionUnit &pu, RefPicList eListA, MvField &rMVFieldA, RefPicList eListB, MvField &rMVFieldB);
  Mv   scaleMv                        (const Mv &rColMV, Int iCurrPOC, Int iCurrRefPOC, Int iColPOC, Int iColRefPOC, Slice *slice);

  bool isLMCMode                      (                          unsigned mode);
  bool isMMLMEnabled                  (const PredictionUnit &pu);
  bool isMFLMEnabled                  (const PredictionUnit &pu);
  bool isLMCModeEnabled               (const PredictionUnit &pu, unsigned mode);
  bool isChromaIntraModeCrossCheckMode(const PredictionUnit &pu);
}

// TU tools
namespace TU
{
  UInt getNumNonZeroCoeffsNonTS       (const TransformUnit &tu);
  bool useDST                         (const TransformUnit &tu, const ComponentID &compID);
  bool isNonTransformedResidualRotated(const TransformUnit &tu, const ComponentID &compID);
  bool getCbf                         (const TransformUnit &tu, const ComponentID &compID);
  bool getCbfAtDepth                  (const TransformUnit &tu, const ComponentID &compID, const unsigned &depth);
  void setCbfAtDepth                  (      TransformUnit &tu, const ComponentID &compID, const unsigned &depth, const bool &cbf);
  bool hasTransformSkipFlag           (const CodingStructure& cs, const CompArea& area);
  UInt getGolombRiceStatisticsIndex   (const TransformUnit &tu, const ComponentID &compID);
  UInt getCoefScanIdx                 (const TransformUnit &tu, const ComponentID &compID);
  bool isProcessingAllQuadrants       (const UnitArea      &tuArea);
  bool hasCrossCompPredInfo           (const TransformUnit &tu, const ComponentID &compID);
  bool needsSqrt2Scale                (const Size& size);
}

// Other tools

UInt getCtuAddr        (const Position& pos, const PreCalcValues &pcv);

template<typename T, size_t N>
UInt updateCandList( T uiMode, Double uiCost, static_vector<T, N>& candModeList, static_vector<Double, N>& candCostList, size_t uiFastCandNum = N )
{
  UInt i;
  UInt shift = 0;

  while( shift < uiFastCandNum && uiCost < candCostList[uiFastCandNum - 1 - shift] )
  {
    shift++;
  }

  if( shift != 0 )
  {
    for( i = 1; i < shift; i++ )
    {
      candModeList[ uiFastCandNum-i ] = candModeList[ uiFastCandNum-1-i ];
      candCostList[ uiFastCandNum-i ] = candCostList[ uiFastCandNum-1-i ];
    }
    candModeList[ uiFastCandNum-shift ] = uiMode;
    candCostList[ uiFastCandNum-shift ] = uiCost;
    return 1;
  }

  return 0;
}


#endif
