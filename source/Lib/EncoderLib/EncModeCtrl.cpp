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

/** \file     EncModeCtrl.cpp
    \brief    Encoder controller for trying out specific modes
*/

#include "EncModeCtrl.h"

#include "AQp.h"
#include "RateCtrl.h"

#include "CommonLib/RdCost.h"
#include "CommonLib/CodingStructure.h"
#include "CommonLib/Picture.h"
#include "CommonLib/UnitTools.h"

#include "CommonLib/dtrace_next.h"

#include <cmath>

Void EncModeCtrl::init( EncCfg *pCfg, RateCtrl *pRateCtrl, RdCost* pRdCost )
{
  m_pcEncCfg      = pCfg;
  m_pcRateCtrl    = pRateCtrl;
  m_pcRdCost      = pRdCost;
  m_fastDeltaQP   = false;
#if SHARP_LUMA_DELTA_QP
  m_lumaQPOffset  = 0;

  initLumaDeltaQpLUT();
#endif
}

void EncModeCtrl::setEarlySkipDetected()
{
  m_ComprCUCtxList.back().earlySkip = true;
}

Void EncModeCtrl::xExtractFeatures( const EncTestMode encTestmode, CodingStructure& cs )
{
  CHECK( cs.features.size() < NUM_ENC_FEATURES, "Features vector is not initialized" );

  cs.features[ENC_FT_DISTORTION     ] = Double( cs.dist              );
  cs.features[ENC_FT_FRAC_BITS      ] = Double( cs.fracBits          );
  cs.features[ENC_FT_RD_COST        ] = Double( cs.cost              );
  cs.features[ENC_FT_ENC_MODE_TYPE  ] = Double( encTestmode.type     );
  cs.features[ENC_FT_ENC_MODE_OPTS  ] = Double( encTestmode.opts     );
  cs.features[ENC_FT_ENC_MODE_PART  ] = Double( encTestmode.partSize );
}

bool EncModeCtrl::nextMode( const CodingStructure &cs, Partitioner &partitioner )
{
  m_ComprCUCtxList.back().lastTestMode = m_ComprCUCtxList.back().testModes.back();

  m_ComprCUCtxList.back().testModes.pop_back();

  while( !m_ComprCUCtxList.back().testModes.empty() && !tryMode( currTestMode(), cs, partitioner ) )
  {
    m_ComprCUCtxList.back().testModes.pop_back();
  }

  return !m_ComprCUCtxList.back().testModes.empty();
}

EncTestMode EncModeCtrl::currTestMode() const
{
  return m_ComprCUCtxList.back().testModes.back();
}

EncTestMode EncModeCtrl::lastTestMode() const
{
  return m_ComprCUCtxList.back().lastTestMode;
}

bool EncModeCtrl::anyMode() const
{
  return !m_ComprCUCtxList.back().testModes.empty();
}

void EncModeCtrl::setBest( CodingStructure& cs )
{
  m_ComprCUCtxList.back().bestCS = &cs;
  m_ComprCUCtxList.back().bestCU = cs.cus[0];
  m_ComprCUCtxList.back().bestTU = cs.cus[0]->firstTU;
 }


bool EncModeCtrl::hasOnlySplitModes() const
{
  for( const auto& mode : m_ComprCUCtxList.back().testModes )
  {
    if( !isModeSplit( mode ) )
    {
      return false;
    }
  }

  return true;
}

void EncModeCtrl::xGetMinMaxQP( int& minQP, int& maxQP, const CodingStructure& cs, const Partitioner &partitioner, const int baseQP, const SPS& sps, const PPS& pps, const bool splitMode )
{
  if( m_pcEncCfg->getUseRateCtrl() )
  {
    minQP = m_pcRateCtrl->getRCQP();
    maxQP = m_pcRateCtrl->getRCQP();
    return;
  }

  const UInt currDepth = partitioner.currDepth;

  if( !splitMode )
  {
    if( currDepth <= pps.getMaxCuDQPDepth() )
    {
      Int deltaQP = m_pcEncCfg->getMaxDeltaQP();
      minQP = Clip3( -sps.getQpBDOffset( CHANNEL_TYPE_LUMA ), MAX_QP, baseQP - deltaQP );
      maxQP = Clip3( -sps.getQpBDOffset( CHANNEL_TYPE_LUMA ), MAX_QP, baseQP + deltaQP );
    }
    else
    {
      minQP = cs.currQP;
      maxQP = cs.currQP;
    }

#if SHARP_LUMA_DELTA_QP
    if( m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled() )
    {
      if( currDepth <= pps.getMaxCuDQPDepth() )
      {
        CompArea clipedArea = clipArea( cs.area.Y(), cs.picture->Y() );
        // keep using the same m_QP_LUMA_OFFSET in the same CTU
        m_lumaQPOffset = calculateLumaDQP( cs.getOrgBuf( clipedArea ) );
      }
      minQP = baseQP - m_lumaQPOffset;
      maxQP = minQP; // force encode choose the modified QO
    }
#endif
  }
  else
  {
    if( currDepth == pps.getMaxCuDQPDepth() )
    {
      Int deltaQP = m_pcEncCfg->getMaxDeltaQP();
      minQP = Clip3( -sps.getQpBDOffset( CHANNEL_TYPE_LUMA ), MAX_QP, baseQP - deltaQP );
      maxQP = Clip3( -sps.getQpBDOffset( CHANNEL_TYPE_LUMA ), MAX_QP, baseQP + deltaQP );
    }
    else if( currDepth < pps.getMaxCuDQPDepth() )
    {
      minQP = baseQP;
      maxQP = baseQP;
    }
    else
    {
      minQP = cs.currQP;
      maxQP = cs.currQP;
    }

#if SHARP_LUMA_DELTA_QP
    if( m_pcEncCfg->getLumaLevelToDeltaQPMapping().isEnabled() )
    {
      minQP = baseQP - m_lumaQPOffset;
      maxQP = minQP;
    }
#endif
  }
}


int EncModeCtrl::xComputeDQP( const CodingStructure &cs, const Partitioner &partitioner )
{
  Picture* picture    = cs.picture;
  unsigned uiAQDepth  = std::min( partitioner.currDepth, ( UInt ) picture->aqlayer.size() - 1 );
  AQpLayer* pcAQLayer = picture->aqlayer[uiAQDepth];

  double dMaxQScale   = pow( 2.0, m_pcEncCfg->getQPAdaptationRange() / 6.0 );
  double dAvgAct      = pcAQLayer->getAvgActivity();
  double dCUAct       = pcAQLayer->getActivity( cs.area.Y().topLeft() );
  double dNormAct     = ( dMaxQScale*dCUAct + dAvgAct ) / ( dCUAct + dMaxQScale*dAvgAct );
  double dQpOffset    = log( dNormAct ) / log( 2.0 ) * 6.0;
  int    iQpOffset    = Int( floor( dQpOffset + 0.49999 ) );
  return iQpOffset;
}


#if SHARP_LUMA_DELTA_QP
Void EncModeCtrl::initLumaDeltaQpLUT()
{
  const LumaLevelToDeltaQPMapping &mapping = m_pcEncCfg->getLumaLevelToDeltaQPMapping();

  if( !mapping.isEnabled() )
  {
    return;
  }

  // map the sparse LumaLevelToDeltaQPMapping.mapping to a fully populated linear table.

  Int         lastDeltaQPValue = 0;
  std::size_t nextSparseIndex = 0;
  for( Int index = 0; index < LUMA_LEVEL_TO_DQP_LUT_MAXSIZE; index++ )
  {
    while( nextSparseIndex < mapping.mapping.size() && index >= mapping.mapping[nextSparseIndex].first )
    {
      lastDeltaQPValue = mapping.mapping[nextSparseIndex].second;
      nextSparseIndex++;
    }
    m_lumaLevelToDeltaQPLUT[index] = lastDeltaQPValue;
  }
}

Int EncModeCtrl::calculateLumaDQP( const CPelBuf& rcOrg )
{
  Double avg = 0;

  // Get QP offset derived from Luma level
  if( m_pcEncCfg->getLumaLevelToDeltaQPMapping().mode == LUMALVL_TO_DQP_AVG_METHOD )
  {
    // Use avg method
    Int sum = 0;
    for( UInt y = 0; y < rcOrg.height; y++ )
    {
      for( UInt x = 0; x < rcOrg.width; x++ )
      {
        sum += rcOrg.at( x, y );
      }
    }
    avg = ( Double ) sum / rcOrg.area();
  }
  else
  {
    // Use maximum luma value
    Int maxVal = 0;
    for( UInt y = 0; y < rcOrg.height; y++ )
    {
      for( UInt x = 0; x < rcOrg.width; x++ )
      {
        const Pel& v = rcOrg.at( x, y );
        if( v > maxVal )
        {
          maxVal = v;
        }
      }
    }
    // use a percentage of the maxVal
    avg = ( Double ) maxVal * m_pcEncCfg->getLumaLevelToDeltaQPMapping().maxMethodWeight;
  }

  Int lumaIdx = Clip3<Int>( 0, Int( LUMA_LEVEL_TO_DQP_LUT_MAXSIZE ) - 1, Int( avg + 0.5 ) );
  Int QP = m_lumaLevelToDeltaQPLUT[lumaIdx];
  return QP;
}
#endif


void CacheBlkInfoCtrl::create()
{
  const unsigned numPos = MAX_CU_SIZE >> MIN_CU_LOG2;

  m_numWidths  = gp_sizeIdxInfo->numWidths();
  m_numHeights = gp_sizeIdxInfo->numHeights();

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      m_codedCUInfo[x][y] = new CodedCUInfo**[m_numWidths];

      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( wIdx ) ) && x + gp_sizeIdxInfo->sizeFrom( wIdx ) <= MAX_CU_SIZE )
        {
          m_codedCUInfo[x][y][wIdx] = new CodedCUInfo*[gp_sizeIdxInfo->numHeights()];

          for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
          {
            if( gp_sizeIdxInfo->isCuSize( gp_sizeIdxInfo->sizeFrom( hIdx ) ) && y + gp_sizeIdxInfo->sizeFrom( hIdx ) <= MAX_CU_SIZE )
            {
              m_codedCUInfo[x][y][wIdx][hIdx] = new CodedCUInfo();
            }
            else
            {
              m_codedCUInfo[x][y][wIdx][hIdx] = nullptr;
            }
          }
        }
        else
        {
          m_codedCUInfo[x][y][wIdx] = nullptr;
        }
      }
    }
  }
}

void CacheBlkInfoCtrl::destroy()
{
  const unsigned numPos = MAX_CU_SIZE >> MIN_CU_LOG2;

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( m_codedCUInfo[x][y][wIdx] )
        {
          for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
          {
            if( m_codedCUInfo[x][y][wIdx][hIdx] )
            {
              delete m_codedCUInfo[x][y][wIdx][hIdx];
            }
          }
          delete[] m_codedCUInfo[x][y][wIdx];
        }
      }

      delete[] m_codedCUInfo[x][y];
    }
  }
}

void CacheBlkInfoCtrl::init( const Slice &slice )
{
  const unsigned numPos = MAX_CU_SIZE >> MIN_CU_LOG2;

  for( unsigned x = 0; x < numPos; x++ )
  {
    for( unsigned y = 0; y < numPos; y++ )
    {
      for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
      {
        if( m_codedCUInfo[x][y][wIdx] )
        {
          for( int hIdx = 0; hIdx < gp_sizeIdxInfo->numHeights(); hIdx++ )
          {
            if( m_codedCUInfo[x][y][wIdx][hIdx] )
            {
              m_codedCUInfo[x][y][wIdx][hIdx]->isSkip  = false;
              m_codedCUInfo[x][y][wIdx][hIdx]->isInter = false;
              m_codedCUInfo[x][y][wIdx][hIdx]->isIntra = false;

              memset( m_codedCUInfo[x][y][wIdx][hIdx]->validMv, 0, sizeof( m_codedCUInfo[x][y][wIdx][hIdx]->validMv ) );
            }
          }
        }
      }
    }
  }

  m_slice_chblk = &slice;
}

CodedCUInfo& CacheBlkInfoCtrl::getBlkInfo( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return *m_codedCUInfo[idx1][idx2][idx3][idx4];
}

bool CacheBlkInfoCtrl::isSkip( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_codedCUInfo[idx1][idx2][idx3][idx4]->isSkip;
}

void CacheBlkInfoCtrl::setMv( const UnitArea& area, const RefPicList refPicList, const int iRefIdx, const Mv& rMv )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  m_codedCUInfo[idx1][idx2][idx3][idx4]->saveMv [refPicList][iRefIdx] = rMv;
  m_codedCUInfo[idx1][idx2][idx3][idx4]->validMv[refPicList][iRefIdx] = true;
}

bool CacheBlkInfoCtrl::getMv( const UnitArea& area, const RefPicList refPicList, const int iRefIdx, Mv& rMv )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_chblk->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  rMv =  m_codedCUInfo[idx1][idx2][idx3][idx4]->saveMv [refPicList][iRefIdx];
  return m_codedCUInfo[idx1][idx2][idx3][idx4]->validMv[refPicList][iRefIdx];
}

void SaveLoadEncInfoCtrl::create()
{
  m_saveLoadInfo = new SaveLoadStruct*[gp_sizeIdxInfo->numWidths()];

  for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
  {
    m_saveLoadInfo[wIdx] = new SaveLoadStruct[gp_sizeIdxInfo->numHeights()];
  }
}

void SaveLoadEncInfoCtrl::destroy()
{
  for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
  {
    delete[] m_saveLoadInfo[wIdx];
  }

  delete[] m_saveLoadInfo;
}

void SaveLoadEncInfoCtrl::init( const Slice &slice )
{
  for( int wIdx = 0; wIdx < gp_sizeIdxInfo->numWidths(); wIdx++ )
  {
    memset( m_saveLoadInfo[wIdx], 0, gp_sizeIdxInfo->numWidths() * sizeof( SaveLoadStruct ) );
  }

  m_slice_sls = &slice;
}

SaveLoadStruct& SaveLoadEncInfoCtrl::getSaveLoadStruct( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4];
}

SaveLoadStruct& SaveLoadEncInfoCtrl::getSaveLoadStructQuad( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( Area( area.lx(), area.ly(), area.lwidth() / 2, area.lheight() / 2 ), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4];
}

SaveLoadTag SaveLoadEncInfoCtrl::getSaveLoadTag( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  unsigned PartIdx = ( ( idx1 << 8 ) | idx2 );
  SaveLoadTag uc   = ( PartIdx == m_saveLoadInfo[idx3][idx4].partIdx ) ? m_saveLoadInfo[idx3][idx4].tag : SAVE_LOAD_INIT;
  return uc;
}
unsigned SaveLoadEncInfoCtrl::getSaveLoadInterDir( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4].interDir;
}

unsigned SaveLoadEncInfoCtrl::getSaveLoadNsstIdx( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4].nsstIdx;
}

bool SaveLoadEncInfoCtrl::getSaveLoadPdpc( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4].pdpc;
}

bool SaveLoadEncInfoCtrl::getSaveLoadEmtCuFlag( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4].emtCuFlag;
}

unsigned SaveLoadEncInfoCtrl::getSaveLoadEmtTuIndex( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4].emtTuIndex;
}

unsigned SaveLoadEncInfoCtrl::getSaveLoadFrucMode( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );

  return m_saveLoadInfo[idx3][idx4].frucMode;
}

unsigned SaveLoadEncInfoCtrl::getSaveLoadAffineFlag( const UnitArea& area )
{
  unsigned idx1, idx2, idx3, idx4;
  getAreaIdx( area.Y(), *m_slice_sls->getPPS()->pcv, idx1, idx2, idx3, idx4 );
  return m_saveLoadInfo[idx3][idx4].affineFlag;
}

void EncModeCtrl::initSlice( const Slice &slice )
{
  if( m_pcEncCfg->getUseE0023FastEnc() )
  {
    m_skipThreshold = ( ( slice.getMinPictureDistance() <= PICTURE_DISTANCE_TH ) ? FAST_SKIP_DEPTH : SKIP_DEPTH );
  }
  else
  {
    m_skipThreshold = SKIP_DEPTH;
  }
}

//////////////////////////////////////////////////////////////////////////
// EncModeCtrlHEVC
//////////////////////////////////////////////////////////////////////////

Void EncModeCtrlQTwithRQT::initCTUEncoding( const Slice &slice )
{
  CHECK( !m_ComprCUCtxList.empty(), "Mode list is not empty at the beginning of a CTU" );

  m_slice = &slice;
}

void EncModeCtrlQTwithRQT::initCULevel( Partitioner &partitioner, const CodingStructure& cs )
{
  // Min/max depth
  unsigned minDepth = 0;
  unsigned maxDepth = g_aucLog2[cs.sps->getSpsNext().getCTUSize()] - g_aucLog2[cs.sps->getSpsNext().getMinQTSize( m_slice->getSliceType(), cs.chType )];
  if( m_pcEncCfg->getUseFastLCTU() )
  {
    if( auto adPartitioner = dynamic_cast<AdaptiveDepthPartitioner*>( &partitioner ) )
    {
      // LARGE CTU
      adPartitioner->setMaxMinDepth( minDepth, maxDepth, cs );
    }
  }

  m_ComprCUCtxList.push_back( ComprCUCtx( cs, minDepth, maxDepth, NUM_EXTRA_FEATURES ) );

  PartSize parentPartSize = NUMBER_OF_PART_SIZES;

  if( m_ComprCUCtxList.size() > 1 )
  {
    const ComprCUCtx& prevDepth = m_ComprCUCtxList[m_ComprCUCtxList.size() - 2];

    if( prevDepth.bestCU && CU::isInter( *prevDepth.bestCU ) )
    {
      parentPartSize = prevDepth.bestCU->partSize;
    }
  }

  m_ComprCUCtxList.back().set( PARENT_PART_SIZE, parentPartSize );

  m_ComprCUCtxList.back().set( BEST_IMV_COST,    MAX_DOUBLE * .5 );
  m_ComprCUCtxList.back().set( BEST_NO_IMV_COST, MAX_DOUBLE * .5 );

  m_ComprCUCtxList.back().set( EARLY_SKIP_INTRA, false );
  m_ComprCUCtxList.back().set( LAST_NSST_IDX,    0 );
  m_ComprCUCtxList.back().set( SKIP_OTHER_NSST,  false );

  // add modes
  // working back to front, as a stack, which is more efficient with the container

  int baseQP = cs.slice->getSliceQp();
  if( m_pcEncCfg->getUseAdaptiveQP() )
  {
    baseQP = Clip3( -cs.sps->getQpBDOffset( CHANNEL_TYPE_LUMA ), MAX_QP, baseQP + xComputeDQP( cs, partitioner ) );
  }
  int minQP = baseQP;
  int maxQP = baseQP;

  xGetMinMaxQP( minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, true );

  // add split modes
  for( int qp = maxQP; qp >= minQP; qp-- )
  {
    m_ComprCUCtxList.back().testModes.push_back( EncTestMode( ETM_SPLIT_QT, SIZE_2Nx2N, ETO_STANDARD, qp, false ) );
  }


  xGetMinMaxQP( minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, false );

  bool useLossless = false;
  int  lowestQP = minQP;
  if( cs.pps->getTransquantBypassEnabledFlag() )
  {
    useLossless = true; // mark that the first iteration is to cost TQB mode.
    minQP = minQP - 1;  // increase loop variable range by 1, to allow testing of TQB mode along with other QPs

    if( m_pcEncCfg->getCUTransquantBypassFlagForceValue() )
    {
      maxQP = minQP;
    }
  }

  // add second pass modes
  for( int qpLoop = maxQP; qpLoop >= minQP; qpLoop-- )
  {
    const int  qp       = std::max( qpLoop, lowestQP );
    const bool lossless = useLossless && qpLoop == minQP;
    // add intra modes
    m_ComprCUCtxList.back().testModes.push_back( { ETM_IPCM,                            qp, lossless } );
    if( m_pcEncCfg->getNSST() )
    {
      for( int rotIdx = 3; rotIdx >= 1; rotIdx-- )
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_NxN, EncTestModeOpts( rotIdx << ETO_NSST_SHIFT ), qp, lossless } );
      }
    }
    if( 1 & m_pcEncCfg->getIntraPDPC() )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_NxN, EncTestModeOpts( 1 << ETO_PDPC_SHIFT ), qp, lossless } );
    }
    m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_NxN,   ETO_STANDARD, qp, lossless } );
    if( m_pcEncCfg->getNSST() )
    {
      for( int rotIdx = 3; rotIdx >= 1; rotIdx-- )
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_2Nx2N, EncTestModeOpts( rotIdx << ETO_NSST_SHIFT ), qp, lossless } );
      }
    }
    if( 1 & m_pcEncCfg->getIntraPDPC() )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_2Nx2N, EncTestModeOpts( 1 << ETO_PDPC_SHIFT ), qp, lossless } );
    }
    m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );

    if( !m_slice->isIntra() )
    {
      if( m_pcEncCfg->getIMV() )
      {
        // Trigger to add int-pel candidate modes in tryMode()
        m_ComprCUCtxList.back().testModes.push_back( { ETM_TRIGGER_IMV_LIST } );
        // 4-Pel mode
        if( m_pcEncCfg->getIMV() == IMV_4PEL )
        {
          Int imv = m_pcEncCfg->getIMV4PelFast() ? 3 : 2;
          // inter with imv and illumination compensation
          if( m_slice->getUseLIC() )
          {
            m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( ( imv << ETO_IMV_SHIFT ) | ETO_LIC ), qp, lossless } );
          }
          m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( imv << ETO_IMV_SHIFT ), qp, lossless } );
        }
        // Int-Pel mode
        if( m_slice->getUseLIC() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( ( 1 << ETO_IMV_SHIFT ) | ETO_LIC ), qp, lossless } );
        }
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( 1 << ETO_IMV_SHIFT ), qp, lossless } );
      }


      // add fine inter modes

      // AMP modes with illumination compensation
      if( m_slice->getUseLIC() )
      {
#if AMP_MRG
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nRx2N, EncTestModeOpts( ETO_FORCE_MERGE | ETO_LIC ), qp, lossless } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nLx2N, EncTestModeOpts( ETO_FORCE_MERGE | ETO_LIC ), qp, lossless } );
#endif
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nRx2N, EncTestModeOpts( ETO_STANDARD    | ETO_LIC ), qp, lossless } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nLx2N, EncTestModeOpts( ETO_STANDARD    | ETO_LIC ), qp, lossless } );
#if AMP_MRG
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnD, EncTestModeOpts( ETO_FORCE_MERGE | ETO_LIC ), qp, lossless } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnU, EncTestModeOpts( ETO_FORCE_MERGE | ETO_LIC ), qp, lossless } );
#endif
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnD, EncTestModeOpts( ETO_STANDARD    | ETO_LIC ), qp, lossless } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnU, EncTestModeOpts( ETO_STANDARD    | ETO_LIC ), qp, lossless } );
      }

      // AMP modes
#if AMP_MRG
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nRx2N, ETO_FORCE_MERGE, qp, lossless } );
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nLx2N, ETO_FORCE_MERGE, qp, lossless } );
#endif
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nRx2N, ETO_STANDARD, qp, lossless } );
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_nLx2N, ETO_STANDARD, qp, lossless } );
#if AMP_MRG
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnD, ETO_FORCE_MERGE, qp, lossless } );
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnU, ETO_FORCE_MERGE, qp, lossless } );
#endif
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnD, ETO_STANDARD, qp, lossless } );
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxnU, ETO_STANDARD, qp, lossless } );

      if( m_slice->getUseLIC() )
      {
        // symmetrical modes with illumination compensation
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2NxN,  ETO_LIC,          qp, lossless } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_Nx2N,  ETO_LIC,          qp, lossless } );
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_NxN,   ETO_LIC,          qp, lossless } );
      }

      // symmetrical modes
      m_ComprCUCtxList.back().testModes.push_back(   { ETM_INTER_ME, SIZE_2NxN,  ETO_STANDARD,     qp, lossless } );
      m_ComprCUCtxList.back().testModes.push_back(   { ETM_INTER_ME, SIZE_Nx2N,  ETO_STANDARD,     qp, lossless } );
      m_ComprCUCtxList.back().testModes.push_back(   { ETM_INTER_ME, SIZE_NxN,   ETO_STANDARD,     qp, lossless } );
    }
  }

  // add first pass modes
  if( !m_slice->isIntra() )
  {
    for( int qpLoop = maxQP; qpLoop >= minQP; qpLoop-- )
    {
      const int  qp       = std::max( qpLoop, lowestQP );
      const bool lossless = useLossless && qpLoop == minQP;

      // 2Nx2N with illumination compensation
      if( m_slice->getUseLIC() )
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    SIZE_2Nx2N, ETO_LIC,      qp, lossless } );

        if( m_pcEncCfg->getUseFRUCMrgMode() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_FRUC,  SIZE_2Nx2N, ETO_LIC,      qp, lossless } );
        }
      }

      // add inter modes
      if( m_pcEncCfg->getUseEarlySkipDetection() )
      {
        if( m_pcEncCfg->getUseFRUCMrgMode() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_FRUC,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }

        m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_SKIP,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        if( cs.sps->getSpsNext().getUseAffine() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_AFFINE,      SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
      }
      else
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );

        if( m_pcEncCfg->getUseFRUCMrgMode() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_FRUC,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }

        m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_SKIP,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        if( cs.sps->getSpsNext().getUseAffine() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_AFFINE,      SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }
      }
    }
  }

  // ensure to skip unprobable modes
  if( !tryMode( m_ComprCUCtxList.back().testModes.back(), cs, partitioner ) )
  {
    nextMode( cs, partitioner );
  }

  m_ComprCUCtxList.back().lastTestMode = EncTestMode();

  if( m_pcEncCfg->getIMV() )
  {
    m_ImvCtxList.push_back( ImvCtx() );
  }
}

void EncModeCtrlQTwithRQT::finishCULevel( Partitioner &partitioner )
{
  m_ComprCUCtxList.pop_back();

  if( m_pcEncCfg->getIMV() )
  {
    m_ImvCtxList.pop_back();
  }
}

Bool EncModeCtrlQTwithRQT::tryMode( const EncTestMode& encTestmode, const CodingStructure &cs, Partitioner& partitioner )
{
  ComprCUCtx& cuECtx = m_ComprCUCtxList.back();

  // if early skip detected, skip all modes checking but the splits
  if( cuECtx.earlySkip && m_pcEncCfg->getUseEarlySkipDetection() && !isModeSplit( encTestmode ) && !( isModeInter( encTestmode ) && encTestmode.partSize == SIZE_2Nx2N ) )
  {
    return false;
  }

  const Slice&      slice               = *m_slice;
  const SPS&        sps                 = *slice.getSPS();
  const UInt        numValidComponents  = getNumberValidComponents( slice.getSPS()->getChromaFormatIdc() );
  const UInt        width               = partitioner.currArea().lumaSize().width;
  const EncTestMode bestTestMode        = cuECtx.bestCS ? getCSEncMode( *cuECtx.bestCS ) : EncTestMode();

  const bool isBoundary = partitioner.isSplitImplicit( CU_QUAD_SPLIT, cs );

  if( isBoundary )
  {
    return encTestmode.type == ETM_SPLIT_QT;
  }

  if( sps.getSpsNext().getUseLargeCTU() )
  {
    if( cuECtx.minDepth > partitioner.currQtDepth && partitioner.canSplit( CU_QUAD_SPLIT, cs ) )
    {
      // enforce QT
      return encTestmode.type == ETM_SPLIT_QT;
    }
    else if( encTestmode.type == ETM_SPLIT_QT && cuECtx.maxDepth <= partitioner.currQtDepth )
    {
      // don't check this QT depth
      return false;
    }
  }

  if( encTestmode.type == ETM_INTRA )
  {
    if( getFastDeltaQp() )
    {
      if( cs.area.lumaSize().width > cs.pcv->fastDeltaQPCuMaxSize )
      {
        return false; // only check necessary 2Nx2N Intra in fast delta-QP mode
      }
    }

    if( m_pcEncCfg->getUseFastLCTU() && partitioner.currArea().lwidth() > 64 )
    {
      return false;
    }

    if( m_pcEncCfg->getUsePbIntraFast() && !cs.slice->isIntra() && cuECtx.interHad == 0 && cuECtx.bestCU && CU::isInter( *cuECtx.bestCU ) )
    {
      return false;
    }

    // INTRA MODES
    CHECK( !slice.isIntra() && !cuECtx.bestTU, "No possible non-intra encoding for a P- or B-slice found" );

    if( !( slice.isIntra() || bestTestMode.type == ETM_INTRA ||
       ( ( !m_pcEncCfg->getDisableIntraPUsInInterSlices() ) && (
                                                  ( cuECtx.bestTU->cbf[0] != 0 ) ||
         ( ( numValidComponents > COMPONENT_Cb ) && cuECtx.bestTU->cbf[1] != 0 ) ||
         ( ( numValidComponents > COMPONENT_Cr ) && cuECtx.bestTU->cbf[2] != 0 )  // avoid very complex intra if it is unlikely
       ) ) ) )
    {
      return false;
    }

    if( encTestmode.partSize != SIZE_2Nx2N )
    {
      //if the code is uncommented and the return after the code is eliminated, then it implements the skip condition for intra SIZE_NxN when the EMTs first pass cost is much worse than the best inter cost
      if( cs.pcv->only2Nx2N || !( partitioner.currDepth == sps.getLog2DiffMaxMinCodingBlockSize() && width > ( 1 << sps.getQuadtreeTULog2MinSize() ) ) )
      {
        return false;
      }

      if( cuECtx.get<bool>( EARLY_SKIP_INTRA ) ) 
      {
        return false;
      }
    }

    if( encTestmode.partSize != lastTestMode().partSize )
    {
      cuECtx.set( SKIP_OTHER_NSST, false );
    }

    bool usePDPC        = ( encTestmode.opts & ETO_PDPC ) != 0;
    int currNsstIdx     = ( encTestmode.opts & ETO_NSST ) >> ETO_NSST_SHIFT;
    int lastNsstIdx     = cuECtx.get<int> ( LAST_NSST_IDX );
    bool skipOtherNsst  = cuECtx.get<bool>( SKIP_OTHER_NSST );

    if( usePDPC && skipOtherNsst )
    {
      return false;
    }

    if( currNsstIdx != lastNsstIdx && skipOtherNsst )
    {
      return false;
    }

    if( lastTestMode().type != ETM_INTRA && cuECtx.bestCS && cuECtx.bestCU && cuECtx.interHad != 0 )
    {
      // Get SATD threshold from best Inter-CU
      if( !cs.slice->isIntra() && m_pcEncCfg->getUsePbIntraFast() )
      {
        CodingUnit* bestCU = cuECtx.bestCU;
        if( bestCU && CU::isInter( *bestCU ) )
        {
          DistParam distParam;
          const bool useHad = !bestCU->transQuantBypass;
          m_pcRdCost->setDistParam( distParam, cs.getOrgBuf( COMPONENT_Y ), cuECtx.bestCS->getPredBuf( COMPONENT_Y ), cs.sps->getBitDepth( CHANNEL_TYPE_LUMA ), COMPONENT_Y, useHad );
          cuECtx.interHad = distParam.distFunc( distParam );
        }
      }
    }

    return true;
  }
  else if( encTestmode.type == ETM_IPCM )
  {
    if( getFastDeltaQp() )
    {
      const SPS &sps = *cs.sps;
      const UInt fastDeltaQPCuMaxPCMSize = Clip3( ( UInt ) 1 << sps.getPCMLog2MinSize(), ( UInt ) 1 << sps.getPCMLog2MaxSize(), 32u );

      if( cs.area.lumaSize().width > fastDeltaQPCuMaxPCMSize )
      {
        return false;   // only check necessary PCM in fast deltaqp mode
      }
    }

    // PCM MODES
    return sps.getUsePCM() && width <= ( 1 << sps.getPCMLog2MaxSize() ) && width >= ( 1 << sps.getPCMLog2MinSize() );
  }
  else if( encTestmode.type == ETM_TRIGGER_IMV_LIST )
  {
    ImvCtx &imvCtx = m_ImvCtxList.back();

    if( imvCtx.idx >= imvCtx.max || imvCtx.testModes.empty() ) // no more modes
    {
      return false;
    }

    EncTestMode nextTestmode = imvCtx.testModes.front();

    while( nextTestmode.type == ETM_INTER_ME && nextTestmode.partSize == SIZE_2Nx2N )
    {
      imvCtx.testModes.pop_front();
      imvCtx.testCosts.pop_front();
      imvCtx.idx++;;
      if( imvCtx.idx == imvCtx.max && imvCtx.loop == 0)
      {
        imvCtx.max++;
        imvCtx.loop = 1;  // switch the loop if we are
      }
      if( imvCtx.idx >= imvCtx.max || imvCtx.testModes.empty() ) // no more modes
      {
        return false;
      }
      nextTestmode = imvCtx.testModes.front(); // load new mode
    }

    nextTestmode.opts = EncTestModeOpts( nextTestmode.opts | ( 1 << ETO_IMV_SHIFT ) );
    m_ComprCUCtxList.back().testModes.push_back( nextTestmode );
    imvCtx.testModes.pop_front();
    imvCtx.testCosts.pop_front();
    imvCtx.idx++;
    return true;
  }
  else if( isModeInter( encTestmode ) )
  {
    if( getFastDeltaQp() )
    {
      if( encTestmode.type == ETM_MERGE_SKIP )
      {
        return false;
      }
      if( encTestmode.partSize != SIZE_2Nx2N || cs.area.lumaSize().width > cs.pcv->fastDeltaQPCuMaxSize )
      {
        return false; // only check necessary 2Nx2N Inter in fast deltaqp mode
      }
    }

    if( ( encTestmode.type == ETM_INTER_ME || encTestmode.type == ETM_MERGE_FRUC ) && encTestmode.partSize == SIZE_2Nx2N && ( encTestmode.opts & ETO_IMV ) != 0 )
    {
      ImvCtx &imvCtx = m_ImvCtxList.back();

      imvCtx.max  = std::max( 1, CU::getMaxNeighboriMVCandNum( cs, partitioner.currArea().lumaPos() ) );
      imvCtx.idx  = 0;
      imvCtx.loop = 0;
    }

    if( ( encTestmode.opts & ETO_IMV ) != 0 )
    {
      int imvOpt = ( encTestmode.opts & ETO_IMV ) >> ETO_IMV_SHIFT;

      if( imvOpt == 3 && cuECtx.get<double>( BEST_NO_IMV_COST ) * 1.06 < cuECtx.get<double>( BEST_IMV_COST ) )
      {
        return false;
      }
    }

    if( m_pcEncCfg->getUseFastLCTU() && encTestmode.type != ETM_AFFINE )
    {
      if( encTestmode.type == ETM_INTER_ME )
      {
        if( m_pcEncCfg->getUseFastLCTU() && encTestmode.partSize != SIZE_2Nx2N && partitioner.currArea().lwidth() > 64 )
        {
          return false;
        }
      }
      else
      {
        if( m_pcEncCfg->getUseFastLCTU() && cs.picture->lheight() < 2 * partitioner.currArea().lheight() )
        {
          return false;
        }
      }
    }

    // INTER MODES (ME + MERGE/SKIP)
    if( slice.isIntra() )
    {
      return false;
    }
    else if( encTestmode.partSize == SIZE_2Nx2N )
    {
      return true;
    }
    else
    {
      // ME MODES ONLY
      // fast CBF mode
      if( m_pcEncCfg->getUseCbfFastMode() && isModeInter( bestTestMode ) && bestTestMode.partSize != SIZE_NxN && ( bestTestMode.partSize != SIZE_2Nx2N || !m_pcEncCfg->getUseEarlySkipDetection() ) && !cuECtx.bestCU->rootCbf )
      {
        return false;
      }

      // only do NxN on maximal depth, but not for 8x8
      if( encTestmode.partSize == SIZE_NxN && ( width == 8 || partitioner.currDepth != sps.getLog2DiffMaxMinCodingBlockSize() ) )
      {
        return false;
      }

      if( encTestmode.type == ETM_INTER_ME && ( encTestmode.partSize == SIZE_Nx2N || encTestmode.partSize == SIZE_2NxnU ) && encTestmode.opts == ETO_STANDARD ) // first symmetric or asymmetric mode
      {
        cuECtx.set( DISABLE_LIC, cuECtx.bestCU->LICFlag == false );
      }

      if( encTestmode.opts & ETO_LIC )
      {
        if( cuECtx.get<bool>( DISABLE_LIC ) )
        {
          return false;
        }
        if( cs.sps->getSpsNext().getLICMode() == 2 ) // selective LIC
        {
          if( encTestmode.partSize != SIZE_2Nx2N )
          {
            return false;
          }
        }
      }

      // derivation of AMP modes
      switch( encTestmode.partSize )
      {
      case SIZE_2NxnU:
      case SIZE_2NxnD:
      case SIZE_nLx2N:
      case SIZE_nRx2N:
        if( !sps.getUseAMP() || partitioner.currDepth >= sps.getLog2DiffMaxMinCodingBlockSize() )
        {
          return false;
        }
        break;
      case SIZE_2Nx2N:
      case SIZE_2NxN:
      case SIZE_Nx2N:
      case SIZE_NxN:
      default:
        // all other part sizes are now OK to go
        return true;
        break;
      }

      if( ( encTestmode.opts & ETO_IMV ) != 0 )
      {
        return true;
      }

      // save this for the next AMP modes
      if( encTestmode.partSize == SIZE_2NxnU && ( encTestmode.opts & ~ETO_LIC ) == 0 )
      {
        cuECtx.set( PRE_AMP_WIDTH,     cuECtx.bestCU->lumaSize().width   );
        cuECtx.set( PRE_AMP_PART_SIZE, cuECtx.bestCU->partSize           );
        cuECtx.set( PRE_AMP_SKIP,      cuECtx.bestCU->skip               );
        cuECtx.set( PRE_AMP_MERGE,     cuECtx.bestCU->firstPU->mergeFlag );
#if AMP_MRG
        cuECtx.set( TRY_AMP_MRG_VERT,  true                              );
        cuECtx.set( TRY_AMP_MRG_HORZ,  true                              );
#endif
      }

      switch( encTestmode.partSize )
      {
      case SIZE_2NxnU:
      case SIZE_2NxnD:
      {
        bool doTest = false;

        if( ( encTestmode.opts & ETO_FORCE_MERGE ) == 0 )
        {
          if( cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2NxN )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2Nx2N && !cuECtx.get<bool>( PRE_AMP_SKIP ) && !cuECtx.get<bool>( PRE_AMP_MERGE ) )
          {
            doTest = true;
          }
#if AMP_MRG
          if( cuECtx.get<unsigned>( PRE_AMP_WIDTH ) == 64 )
          {
            doTest = false;
          }

          if( doTest )
          {
            cuECtx.set( TRY_AMP_MRG_HORZ, false );
          }
#else
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) == NUMBER_OF_PART_SIZES && cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2NxN )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) == SIZE_2Nx2N )
          {
            doTest = false;
          }
#endif
        }
#if AMP_MRG
        // AMP_MRG modes
        else
        {
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) == NUMBER_OF_PART_SIZES && cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2NxN )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) >= SIZE_2NxnU && cuECtx.get<PartSize>( PARENT_PART_SIZE ) <= SIZE_nRx2N )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2Nx2N && !cuECtx.get<bool>( PRE_AMP_SKIP ) )
          {
            doTest = true;
          }

          doTest &= cuECtx.get<bool>( TRY_AMP_MRG_HORZ );
        }
#endif

        if( !doTest )
        {
          return false;
        }
      }
      break;
      case SIZE_nLx2N:
      case SIZE_nRx2N:
      {
        bool doTest = false;

        if( ( encTestmode.opts & ETO_FORCE_MERGE ) == 0 )
        {
          if( cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_Nx2N )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2Nx2N && !cuECtx.get<bool>( PRE_AMP_SKIP ) && !cuECtx.get<bool>( PRE_AMP_MERGE ) )
          {
            doTest = true;
          }
#if AMP_MRG
          if( cuECtx.get<unsigned>( PRE_AMP_WIDTH ) == 64 )
          {
            doTest = false;
          }

          if( doTest ) cuECtx.set( TRY_AMP_MRG_VERT, false );
#else
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) == NUMBER_OF_PART_SIZES && cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2NxN )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) == SIZE_2Nx2N )
          {
            doTest = false;
          }
#endif
        }
#if AMP_MRG
        else
        {
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) == NUMBER_OF_PART_SIZES && cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_Nx2N )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PARENT_PART_SIZE ) >= SIZE_2NxnU && cuECtx.get<PartSize>( PARENT_PART_SIZE ) <= SIZE_nRx2N )
          {
            doTest = true;
          }
          if( cuECtx.get<PartSize>( PRE_AMP_PART_SIZE ) == SIZE_2Nx2N && !cuECtx.get<bool>( PRE_AMP_SKIP ) )
          {
            doTest = true;
          }

          doTest &= cuECtx.get<bool>( TRY_AMP_MRG_VERT );
        }
#endif

        if( !doTest )
        {
          return false;
        }
      }
      break;
      case SIZE_2Nx2N:
      case SIZE_2NxN:
      case SIZE_Nx2N:
      case SIZE_NxN:
      default:
        // do nothing
        break;
      }
    }

    return true;
  }
  else if( isModeSplit( encTestmode ) )
  {
    CHECK( getPartSplit( encTestmode ) != CU_QUAD_SPLIT, "HEVC only supports quad splits" );

    if( partitioner.currQtDepth >= sps.getLog2DiffMaxMinCodingBlockSize() )
    {
      return false;
    }

    return true;
  }
  else
  {
    THROW( "Unknown mode" );
  }
}


Bool EncModeCtrlQTwithRQT::useModeResult( const EncTestMode& encTestmode, CodingStructure*& tempCS, Partitioner& partitioner )
{
  xExtractFeatures( encTestmode, *tempCS );

  ComprCUCtx& cuECtx = m_ComprCUCtxList.back();

  //imv stuff starts
  if( IMV_OFF != m_pcEncCfg->getIMV() && encTestmode.type == ETM_INTER_ME )
  {
    ImvCtx &imvCtx = m_ImvCtxList.back();

    bool insertIMVCand = ( encTestmode.opts & ETO_IMV ) == 0;
    int  deleteIMVCand = -1;
    if( tempCS->cost < MAX_DOUBLE && insertIMVCand )
    {
      for( int i = 0; i < imvCtx.testModes.size(); i++ )
      {
        if( encTestmode.partSize == imvCtx.testModes[i].partSize && tempCS->cus[0]->emtFlag && ( ( imvCtx.testModes[i].opts & ETO_LIC ) != 0 ) == tempCS->cus[0]->LICFlag )
        {
          // delete existing
          deleteIMVCand = tempCS->cost < imvCtx.testCosts[i] ? i : -1;
        }
        if( ( encTestmode.partSize == imvCtx.testModes[i].partSize && tempCS->cost >= imvCtx.testCosts[i] ) || tempCS->cost == imvCtx.testCosts[i] )
        {
          insertIMVCand = false;
        }
      }

      if( insertIMVCand && tempCS->cus[0]->emtFlag && imvCtx.emt_backup_cost <= tempCS->cost )
      {
        insertIMVCand = false;
      }
    }
    else
    {
      insertIMVCand = false;
    }

    if( deleteIMVCand != -1 )
    {
      size_t last = imvCtx.testModes.size()-1;
      for( size_t i = deleteIMVCand; i < last; i++ )
      {
        imvCtx.testModes[ i ] = imvCtx.testModes[ i+1 ];
        imvCtx.testCosts[ i ] = imvCtx.testCosts[ i+1 ];
      }
      imvCtx.testModes.pop_back();
      imvCtx.testCosts.pop_back();
    }

    if( insertIMVCand && imvCtx.testModes.size() < imvCtx.testModes.capacity() )
    {
      imvCtx.testModes.push_back( EncTestMode() );
      imvCtx.testCosts.push_back( MAX_DOUBLE );

      updateCandList( encTestmode, tempCS->cost, imvCtx.testModes, imvCtx.testCosts, imvCtx.testModes.size() );
    }


    if( m_pcEncCfg->getInterEMT() && 0 == tempCS->cus[0]->emtFlag )
    {
      imvCtx.emt_backup_cost = tempCS->features[ENC_FT_RD_COST];
    }

    bool update = true;
    if( ( encTestmode.opts & ETO_IMV ) != 0 && ( encTestmode.opts & ETO_FORCE_MERGE ) == 0 && encTestmode.partSize != SIZE_2Nx2N  )
    {
      if( m_pcEncCfg->getInterEMT() )
      {
        if( 0 == tempCS->cus[0]->emtFlag )
        {
          imvCtx.emt_backup_max   = imvCtx.max;
          imvCtx.emt_backup_loop  = imvCtx.loop;
          imvCtx.emt_backup_idx   = imvCtx.idx;
        }
        else if( imvCtx.emt_backup_cost > tempCS->features[ENC_FT_RD_COST] )
        {
          imvCtx.max   = imvCtx.emt_backup_max;
          imvCtx.loop  = imvCtx.emt_backup_loop;
          imvCtx.idx   = imvCtx.emt_backup_idx;
        }
        else
        {
          update = false;
        }
      }

      if( update )
      {
        if( tempCS->cost == MAX_DOUBLE || ( m_pcEncCfg->getUseOBMC() && !tempCS->cus.empty() && tempCS->cus[0]->obmcFlag ) )
        {
          imvCtx.max++;
        }

        if( ( tempCS->features[ENC_FT_RD_COST] < cuECtx.bestCS->features[ENC_FT_RD_COST]) )
        {
          tempCS->cus[0]->imvNumCand = imvCtx.idx;

          if( imvCtx.loop == 0 )
          {
            imvCtx.max++;
            imvCtx.max = std::min( imvCtx.max, m_pcEncCfg->getIMVMaxCand() );
          }
        }
      }
    }

    if( update )
    {
      if( imvCtx.idx == imvCtx.max && imvCtx.loop == 0 )
      {
        imvCtx.loop = 1;  // switch the loop if we are
        imvCtx.max++;
      }

      if( m_pcEncCfg->getIMV4PelFast() )
      {
        int imvMode = ( encTestmode.opts & ETO_IMV ) >> ETO_IMV_SHIFT;

        if( imvMode == 1 )
        {
          if( tempCS->cost < cuECtx.get<double>( BEST_IMV_COST ) )
          {
            cuECtx.set( BEST_IMV_COST, tempCS->cost );
          }
        }
        else if( imvMode == 0 )
        {
          if( tempCS->cost < cuECtx.get<double>( BEST_NO_IMV_COST ) )
          {
            cuECtx.set( BEST_NO_IMV_COST, tempCS->cost );
          }
        }
      }
    }
  }

  if( encTestmode.type == ETM_INTRA )
  {
    const int nsstIdx      = ( encTestmode.opts & ETO_NSST ) >> ETO_NSST_SHIFT;

    if( !cuECtx.bestCS || ( tempCS->cost >= cuECtx.bestCS->cost && cuECtx.bestCS->cus.size() == 1 && CU::isIntra( *cuECtx.bestCS->cus[0] ) )
                       || ( tempCS->cost <  cuECtx.bestCS->cost && CU::isIntra( *tempCS->cus[0] ) ) )
    {
      cuECtx.set( SKIP_OTHER_NSST, !tempCS->cus[0]->rootCbf );
    }

    cuECtx.set( LAST_NSST_IDX, nsstIdx );
  }

  //imv stuff ends

  if( encTestmode.type == ETM_INTRA && encTestmode.partSize == SIZE_2Nx2N )
  {
    const CodingUnit& cu = *tempCS->getCU();

    if( !cu.emtFlag )
    {
      cuECtx.bestEmtSize2Nx2N1stPass = tempCS->cost;
    }


    if( m_pcEncCfg->getIntraEMT() && m_pcEncCfg->getFastInterEMT() )
    {
      bool bestPredModeInter = false;
      if( cuECtx.bestCS )
      {
        bestPredModeInter = cuECtx.bestCS->cus[0]->predMode != MODE_INTRA;
      }

      const double bestInterCost = getBestInterCost();
      //now we check whether the second pass of SIZE_2Nx2N and the whole Intra SIZE_NxN should be skipped or not
      if( !cu.emtFlag && bestPredModeInter && ( m_pcEncCfg->getUseSaveLoadEncInfo() ? ( bestInterCost < MAX_DOUBLE ) : true ) )
      {
        const double thEmtInterFastSkipIntra = 1.4; // Skip checking Intra if "2Nx2N using DCT2" is worse than best Inter mode
        if( cuECtx.bestEmtSize2Nx2N1stPass > thEmtInterFastSkipIntra * bestInterCost )
        {
          cuECtx.set( EARLY_SKIP_INTRA, true );
        }
      }
    }

  }

  // for now just a simple decision based on RD-cost or choose tempCS if bestCS is not yet coded
  if( !cuECtx.bestCS || tempCS->features[ENC_FT_RD_COST] < cuECtx.bestCS->features[ENC_FT_RD_COST] )
  {
    cuECtx.bestCS = tempCS;
    cuECtx.bestCU = tempCS->cus[0];
    cuECtx.bestTU = cuECtx.bestCU->firstTU;

    if( ( encTestmode.type == ETM_INTER_ME ) || ( encTestmode.type == ETM_MERGE_SKIP ) || ( encTestmode.type == ETM_MERGE_FRUC ) || ( encTestmode.type == ETM_AFFINE ) ) //if it is an inter mode
    {
      //Here we take the best cost of both inter modes. We are assuming only the inter modes (and all of them) have come before the intra modes!!!
      cuECtx.bestInterCost = cuECtx.bestCS->cost;
    }

    return true;
  }
  else
  {
    return false;
  }
}

//////////////////////////////////////////////////////////////////////////
// EncModeCtrlQTBT
//////////////////////////////////////////////////////////////////////////

EncModeCtrlMTnoRQT::EncModeCtrlMTnoRQT()
{
  CacheBlkInfoCtrl   ::create();
  SaveLoadEncInfoCtrl::create();
}

EncModeCtrlMTnoRQT::~EncModeCtrlMTnoRQT()
{
  CacheBlkInfoCtrl   ::destroy();
  SaveLoadEncInfoCtrl::destroy();
}

Void EncModeCtrlMTnoRQT::initCTUEncoding( const Slice &slice )
{
  CacheBlkInfoCtrl   ::init( slice );
  SaveLoadEncInfoCtrl::init( slice );

  CHECK( !m_ComprCUCtxList.empty(), "Mode list is not empty at the beginning of a CTU" );

  m_slice = &slice;
}

#if ENABLE_TRACING
static unsigned getHalvedIdx( unsigned idx )
{
  return gp_sizeIdxInfo->idxFrom( gp_sizeIdxInfo->sizeFrom( idx ) >> 1 );
}
#endif

void EncModeCtrlMTnoRQT::initCULevel( Partitioner &partitioner, const CodingStructure& cs )
{
  // Min/max depth
  unsigned minDepth = 0;
  unsigned maxDepth = g_aucLog2[cs.sps->getSpsNext().getCTUSize()] - g_aucLog2[cs.sps->getSpsNext().getMinQTSize( m_slice->getSliceType(), cs.chType )];
  if( m_pcEncCfg->getUseFastLCTU() )
  {
    if( auto adPartitioner = dynamic_cast<AdaptiveDepthPartitioner*>( &partitioner ) )
    {
      // LARGE CTU
      adPartitioner->setMaxMinDepth( minDepth, maxDepth, cs );
    }
  }

  m_ComprCUCtxList.push_back( ComprCUCtx( cs, minDepth, maxDepth, NUM_EXTRA_FEATURES ) );

  // set features
  ComprCUCtx &cuECtx  = m_ComprCUCtxList.back();
  cuECtx.set( BEST_NON_SPLIT_COST,  MAX_DOUBLE );
  cuECtx.set( BEST_VERT_SPLIT_COST, MAX_DOUBLE );
  cuECtx.set( BEST_HORZ_SPLIT_COST, MAX_DOUBLE );
  SaveLoadStruct &sls = getSaveLoadStruct( partitioner.currArea() );
  cuECtx.set( HISTORY_DO_SAVE,      sls.partIdx == cuECtx.partIdx && sls.tag == SAVE_ENC_INFO );
  cuECtx.set( SAVE_LOAD_TAG,        sls.partIdx == cuECtx.partIdx  ? sls.tag  : SAVE_LOAD_INIT );
  cuECtx.set( HISTORY_NEED_TO_SAVE, m_pcEncCfg->getUseSaveLoadEncInfo() && cs.area.lwidth() > ( 1 << MIN_CU_LOG2 ) && cs.area.lheight() > ( 1 << MIN_CU_LOG2 ) );
  cuECtx.set( BEST_IMV_COST,        MAX_DOUBLE * .5 );
  cuECtx.set( BEST_NO_IMV_COST,     MAX_DOUBLE * .5 );
  cuECtx.set( LAST_NSST_IDX,        0 );
  cuECtx.set( SKIP_OTHER_NSST,      false );

  DTRACE( g_trace_ctx, D_SAVE_LOAD, "SaveLoadTag at %d,%d (%dx%d): %d, Split: %d\n",
          cs.area.lx(), cs.area.ly(),
          cs.area.lwidth(), cs.area.lheight(),
          cuECtx.get<SaveLoadTag>( SAVE_LOAD_TAG ),
          sls.partIdx == cuECtx.partIdx && sls.tag == LOAD_ENC_INFO && m_pcEncCfg->getUseSaveLoadSplitDecision() ? sls.split : 0 );

  // QP
  int baseQP = cs.slice->getSliceQp();
  if( m_pcEncCfg->getUseAdaptiveQP() )
  {
    baseQP = Clip3( -cs.sps->getQpBDOffset( CHANNEL_TYPE_LUMA ), MAX_QP, baseQP + xComputeDQP( cs, partitioner ) );
  }
  int minQP = baseQP;
  int maxQP = baseQP;

  xGetMinMaxQP( minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, true );

  // Add coding modes here
  // NOTE: Working back to front, as a stack, which is more efficient with the container
  // NOTE: First added modes will be processed at the end.

  //////////////////////////////////////////////////////////////////////////
  // Add unit split modes

  for( int qp = maxQP; qp >= minQP; qp-- )
  {
    m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_QT, SIZE_2Nx2N, ETO_STANDARD, qp, false } );
  }


  if( partitioner.canSplit( CU_VERT_SPLIT, cs ) )
  {
    // add split modes
    for( int qp = maxQP; qp >= minQP; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_BT_V, SIZE_2Nx2N, ETO_STANDARD, qp, false } );
    }
    m_ComprCUCtxList.back().set( DID_VERT_SPLIT, true );
  }
  else
  {
    m_ComprCUCtxList.back().set( DID_VERT_SPLIT, false );
  }

  if( partitioner.canSplit( CU_HORZ_SPLIT, cs ) )
  {
    // add split modes
    for( int qp = maxQP; qp >= minQP; qp-- )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_SPLIT_BT_H, SIZE_2Nx2N, ETO_STANDARD, qp, false } );
    }
    m_ComprCUCtxList.back().set( DID_HORZ_SPLIT, true );
  }
  else
  {
    m_ComprCUCtxList.back().set( DID_HORZ_SPLIT, false );
  }


  xGetMinMaxQP( minQP, maxQP, cs, partitioner, baseQP, *cs.sps, *cs.pps, false );

  bool useLossless = false;
  int  lowestQP = minQP;
  if( cs.pps->getTransquantBypassEnabledFlag() )
  {
    useLossless = true; // mark that the first iteration is to cost TQB mode.
    minQP = minQP - 1;  // increase loop variable range by 1, to allow testing of TQB mode along with other QPs

    if( m_pcEncCfg->getCUTransquantBypassFlagForceValue() )
    {
      maxQP = minQP;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Add unit coding modes: Intra, InterME, InterMerge ...

  for( int qpLoop = maxQP; qpLoop >= minQP; qpLoop-- )
  {
    const int  qp       = std::max( qpLoop, lowestQP );
    const bool lossless = useLossless && qpLoop == minQP;
    // add intra modes
    m_ComprCUCtxList.back().testModes.push_back( { ETM_IPCM,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
    if( m_pcEncCfg->getNSST() )
    {
      const int maxRotIdx = CS::isDoubleITree( cs ) && cs.chType == CHANNEL_TYPE_CHROMA && ( partitioner.currArea().lwidth() < 8 || partitioner.currArea().lheight() < 8 ) ? 0 : 3;
      for( int rotIdx = maxRotIdx; rotIdx >= 1; rotIdx-- )
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_2Nx2N, EncTestModeOpts( rotIdx << ETO_NSST_SHIFT ), qp, lossless } );
      }
    }
    if( 1 & m_pcEncCfg->getIntraPDPC() )
    {
      m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_2Nx2N, EncTestModeOpts( 1 << ETO_PDPC_SHIFT ), qp, lossless } );
    }
    m_ComprCUCtxList.back().testModes.push_back( { ETM_INTRA, SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
  }

  // add first pass modes
  if( !m_slice->isIntra() )
  {
    for( int qpLoop = maxQP; qpLoop >= minQP; qpLoop-- )
    {
      const int  qp       = std::max( qpLoop, lowestQP );
      const bool lossless = useLossless && qpLoop == minQP;

      if( m_pcEncCfg->getIMV() )
      {
        if( m_pcEncCfg->getIMV() == IMV_4PEL )
        {
          Int imv = m_pcEncCfg->getIMV4PelFast() ? 3 : 2;
          // inter with imv and illumination compensation
          if( m_slice->getUseLIC() )
          {
            m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( ( imv << ETO_IMV_SHIFT ) | ETO_LIC ), qp, lossless } );
          }
          m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( imv << ETO_IMV_SHIFT ), qp, lossless } );
        }
        // inter with imv and illumination compensation
        if( m_slice->getUseLIC() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( ( 1 << ETO_IMV_SHIFT ) | ETO_LIC ), qp, lossless } );
        }
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME, SIZE_2Nx2N, EncTestModeOpts( 1 << ETO_IMV_SHIFT ), qp, lossless } );
      }

      // inter with illumination compensation
      if( m_slice->getUseLIC() )
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    SIZE_2Nx2N, ETO_LIC,      qp, lossless } );

        if( m_pcEncCfg->getUseFRUCMrgMode() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_FRUC,  SIZE_2Nx2N, ETO_LIC,      qp, lossless } );
        }
      }

      // add inter modes
      if( m_pcEncCfg->getUseEarlySkipDetection() )
      {
        if( m_pcEncCfg->getUseFRUCMrgMode() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_FRUC,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }

        m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_SKIP,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        if( cs.sps->getSpsNext().getUseAffine() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_AFFINE,      SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
      }
      else
      {
        m_ComprCUCtxList.back().testModes.push_back( { ETM_INTER_ME,    SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );

        if( m_pcEncCfg->getUseFRUCMrgMode() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_FRUC,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }

        m_ComprCUCtxList.back().testModes.push_back( { ETM_MERGE_SKIP,  SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        if( cs.sps->getSpsNext().getUseAffine() )
        {
          m_ComprCUCtxList.back().testModes.push_back( { ETM_AFFINE,      SIZE_2Nx2N, ETO_STANDARD, qp, lossless } );
        }
      }
    }
  }

  // ensure to skip unprobable modes
  if( !tryMode( m_ComprCUCtxList.back().testModes.back(), cs, partitioner ) )
  {
    nextMode( cs, partitioner );
  }

  m_ComprCUCtxList.back().lastTestMode = EncTestMode();
}

void EncModeCtrlMTnoRQT::finishCULevel( Partitioner &partitioner )
{
  ComprCUCtx &cuECtx   = m_ComprCUCtxList.back();
  const CompArea &area = partitioner.currArea().Y();
  SaveLoadStruct &sls  = getSaveLoadStruct( partitioner.currArea() );

  if( cuECtx.get<bool>( HISTORY_DO_SAVE ) && cuECtx.bestCS )
  {
    sls.partIdx = cuECtx.partIdx;
    sls.tag     = LOAD_ENC_INFO;
    cuECtx.set( HISTORY_DO_SAVE, false );
  }

  if( m_pcEncCfg->getUseSaveLoadEncInfo() && area.width > ( 1 << MIN_CU_LOG2 ) && area.height > ( 1 << MIN_CU_LOG2 ) )
  {
    SaveLoadStruct &subSls = getSaveLoadStructQuad( partitioner.currArea() );

    subSls.tag     = SAVE_LOAD_INIT;
    subSls.partIdx = 0xff;

    DTRACE( g_trace_ctx, D_SAVE_LOAD, "SaveLoad %d (%d %d) [%d %d] \n", SAVE_LOAD_INIT, -1, -1, getHalvedIdx( cuECtx.cuW ), getHalvedIdx( cuECtx.cuH ) );
    DTRACE( g_trace_ctx, D_SAVE_LOAD, "saving tag at %d,%d (%dx%d) -> %d\n", area.x, area.y, area.width >> 1, area.height >> 1, SAVE_LOAD_INIT );
  }

  if( m_pcEncCfg->getUseSaveLoadSplitDecision() && m_pcEncCfg->getUseSaveLoadEncInfo() && area.width > ( 1 << MIN_CU_LOG2 ) && area.height > ( 1 << MIN_CU_LOG2 ) && cuECtx.get<UChar>( SAVE_LOAD_TAG ) == SAVE_ENC_INFO )
  {
    UChar c = 0;
    double bestNonSplit = cuECtx.get<double>( BEST_NON_SPLIT_COST  );
    double horzCost     = cuECtx.get<double>( BEST_HORZ_SPLIT_COST );
    double vertCost     = cuECtx.get<double>( BEST_VERT_SPLIT_COST );
    double threshold    = JVET_D0077_SPLIT_DECISION_COST_SCALE * std::min( bestNonSplit, std::min( horzCost, vertCost ) );

    if( bestNonSplit > threshold )
    {
      c |= 0x01;
    }
    if( horzCost > threshold )
    {
      c |= 0x02;
    }
    if( vertCost > threshold )
    {
      c |= 0x04;
    }
    sls.split = c;

    DTRACE( g_trace_ctx, D_SAVE_LOAD, "saving split info at %d,%d (%dx%d) thres = %f, cans (%f, %f, %f) -> %d\n", area.x, area.y, area.width, area.height, threshold, bestNonSplit, horzCost, vertCost, c & 0x7 );
  }

  m_ComprCUCtxList.pop_back();
}


Bool EncModeCtrlMTnoRQT::tryMode( const EncTestMode& encTestmode, const CodingStructure &cs, Partitioner& partitioner )
{
  CHECK( encTestmode.partSize != SIZE_2Nx2N, "Only 2Nx2N supported with QTBT" );

  ComprCUCtx& cuECtx = m_ComprCUCtxList.back();

  // Fast checks, partitioning depended

  // if early skip detected, skip all modes checking but the splits
  if( cuECtx.earlySkip && m_pcEncCfg->getUseEarlySkipDetection() && !isModeSplit( encTestmode ) && !( isModeInter( encTestmode ) && encTestmode.partSize == SIZE_2Nx2N ) )
  {
    return false;
  }

  const PartSplit implicitSplit = partitioner.getImplicitSplit( cs );
  const bool isBoundary         = implicitSplit != CU_DONT_SPLIT;

  if( isBoundary && encTestmode.type != ETM_SPLIT_QT )
  {
    return false;
  }
  else if( isBoundary && encTestmode.type == ETM_SPLIT_QT )
  {
    return partitioner.canSplit( CU_QUAD_SPLIT, cs );
  }

  if( partitioner.currArea().lwidth() > m_slice->getSPS()->getMaxTrSize() && encTestmode.type != ETM_SPLIT_QT )
  {
    return false;
  }

  const Slice&           slice       = *m_slice;
  const SPS&             sps         = *slice.getSPS();
  const UInt             numComp     = getNumberValidComponents( slice.getSPS()->getChromaFormatIdc() );
  const UInt             width       = partitioner.currArea().lumaSize().width;
  const CodingStructure *bestCS      = cuECtx.bestCS;
  const CodingUnit      *bestCU      = cuECtx.bestCU;
  const EncTestMode      bestMode    = bestCS ? getCSEncMode( *bestCS ) : EncTestMode();

  UChar       saveLoadSplit          = 0;
  SaveLoadTag saveLoadTag            = cuECtx.get<SaveLoadTag>( SAVE_LOAD_TAG );
  SaveLoadStruct &sls                = getSaveLoadStruct( partitioner.currArea() );
  CodedCUInfo    &relatedCU          = getBlkInfo( partitioner.currArea() );

  // assuming that all non-split modes are done before all split modes
  if( isModeSplit( encTestmode ) && isModeNoSplit( cuECtx.lastTestMode ) && bestCS )
  {
    // when encountering first split mode, save info for non-split modes
    if( bestCU )
    {
      if( CU::isInter( *bestCU ) )
      {
        relatedCU.isInter   = true;
        relatedCU.isSkip    = bestCU->skip;
      }
      else if( CU::isIntra( *bestCU ) )
      {
        relatedCU.isIntra   = true;
      }
    }

    if( cuECtx.get<bool>( HISTORY_DO_SAVE ) && bestCS->cost != MAX_DOUBLE )
    {
      if( bestCU->predMode != MODE_INTRA )
      {
        sls.mergeFlag  = bestCU->firstPU->mergeFlag;
        sls.interDir   = bestCU->firstPU->interDir;
        sls.LICFlag    = bestCU->LICFlag;
        sls.imv        = bestCU->imv;
        sls.frucMode   = bestCU->firstPU->frucMrgMode;
        sls.affineFlag = bestCU->affine;
      }
      else
      {
        sls.nsstIdx    = bestCU->nsstIdx;
        sls.pdpc       = bestCU->pdpc;
      }

      sls.emtCuFlag  = bestCU->emtFlag;
      sls.emtTuIndex = bestCU->firstTU->emtIdx; //since this is the QTBT path, there is only one TU

      sls.tag        = LOAD_ENC_INFO;
      CHECK( sls.partIdx != cuECtx.partIdx, "partidx is not consistent" );

#if ENABLE_TRACING
      const Area& area = partitioner.currArea().Y();
      DTRACE( g_trace_ctx, D_SAVE_LOAD, "saving tag at %d,%d (%dx%d) -> %d\n", area.x, area.y, area.width, area.height, LOAD_ENC_INFO );
#endif
    }
  }

  if( cuECtx.minDepth > partitioner.currQtDepth && partitioner.canSplit( CU_QUAD_SPLIT, cs ) )
  {
    // enforce QT
    return encTestmode.type == ETM_SPLIT_QT;
  }
  else if( encTestmode.type == ETM_SPLIT_QT && cuECtx.maxDepth <= partitioner.currQtDepth )
  {
    // don't check this QT depth
    return false;
  }

  if( m_pcEncCfg->getUseSaveLoadEncInfo() && m_pcEncCfg->getUseSaveLoadSplitDecision() && saveLoadTag == LOAD_ENC_INFO )
  {
    saveLoadSplit = sls.split;
    if( ( saveLoadSplit & 0x01 ) && isModeNoSplit( encTestmode ) )
    {
      return false;
    }
  }
  if( bestCS && bestCS->cus.size() == 1 )
  {
    // update the best non-split cost
    cuECtx.set( BEST_NON_SPLIT_COST, bestCS->cost );
  }

  if( encTestmode.type == ETM_INTRA )
  {
    if( getFastDeltaQp() )
    {
      if( cs.area.lumaSize().width > cs.pcv->fastDeltaQPCuMaxSize )
      {
        return false; // only check necessary 2Nx2N Intra in fast delta-QP mode
      }
    }

    if( m_pcEncCfg->getUseFastLCTU() && partitioner.currArea().lumaSize().area() > 4096 )
    {
      return false;
    }

    if( m_pcEncCfg->getUsePbIntraFast() && !cs.slice->isIntra() && cuECtx.interHad == 0 && cuECtx.bestCU && CU::isInter( *cuECtx.bestCU ) )
    {
      return false;
    }

    // INTRA MODES
    CHECK( !m_pcEncCfg->getUseSaveLoadEncInfo() && !slice.isIntra() && !cuECtx.bestTU, "No possible non-intra encoding for a P- or B-slice found" );

    if( !( slice.isIntra() || bestMode.type == ETM_INTRA ||
         ( ( !m_pcEncCfg->getDisableIntraPUsInInterSlices() ) && !relatedCU.isInter && (
                                         ( cuECtx.bestTU->cbf[0] != 0 ) ||
           ( ( numComp > COMPONENT_Cb ) && cuECtx.bestTU->cbf[1] != 0 ) ||
           ( ( numComp > COMPONENT_Cr ) && cuECtx.bestTU->cbf[2] != 0 )  // avoid very complex intra if it is unlikely
         ) ) ) )
    {
      return false;
    }

    if( encTestmode.partSize != lastTestMode().partSize )
    {
      cuECtx.set( SKIP_OTHER_NSST, false );
    }

    bool usePDPC = sps.getSpsNext().isPlanarPDPC() ? false : ((encTestmode.opts & ETO_PDPC) != 0);
    int currNsstIdx     = ( encTestmode.opts & ETO_NSST ) >> ETO_NSST_SHIFT;
    int lastNsstIdx     = cuECtx.get<int> ( LAST_NSST_IDX );
    bool skipOtherNsst  = cuECtx.get<bool>( SKIP_OTHER_NSST );

    if( usePDPC && skipOtherNsst )
    {
      return false;
    }

    if( currNsstIdx != lastNsstIdx && skipOtherNsst )
    {
      return false;
    }

    if( LOAD_ENC_INFO == saveLoadTag && currNsstIdx && currNsstIdx != getSaveLoadNsstIdx( cs.area ) )
    {
      return false;
    }


    if(( ! sps.getSpsNext().isPlanarPDPC() ) &&  ( LOAD_ENC_INFO == saveLoadTag && usePDPC != getSaveLoadPdpc( cs.area ) ))
    {
      return false;
    }

    if( lastTestMode().type != ETM_INTRA && cuECtx.bestCS && cuECtx.bestCU && cuECtx.interHad != 0 )
    {
      // Get SATD threshold from best Inter-CU
      if( !cs.slice->isIntra() && m_pcEncCfg->getUsePbIntraFast() )
      {
        CodingUnit* bestCU = cuECtx.bestCU;
        if( bestCU && CU::isInter( *bestCU ) )
        {
          DistParam distParam;
          const bool useHad = !bestCU->transQuantBypass;
          m_pcRdCost->setDistParam( distParam, cs.getOrgBuf( COMPONENT_Y ), cuECtx.bestCS->getPredBuf( COMPONENT_Y ), cs.sps->getBitDepth( CHANNEL_TYPE_LUMA ), COMPONENT_Y, useHad );
          cuECtx.interHad = distParam.distFunc( distParam );
        }
      }
    }

    return true;
  }
  else if( encTestmode.type == ETM_IPCM )
  {
    if( getFastDeltaQp() )
    {
      const SPS &sps = *cs.sps;
      const UInt fastDeltaQPCuMaxPCMSize = Clip3( ( UInt ) 1 << sps.getPCMLog2MinSize(), ( UInt ) 1 << sps.getPCMLog2MaxSize(), 32u );

      if( cs.area.lumaSize().width > fastDeltaQPCuMaxPCMSize )
      {
        return false;   // only check necessary PCM in fast deltaqp mode
      }
    }

    // PCM MODES
    return sps.getUsePCM() && width <= ( 1 << sps.getPCMLog2MaxSize() ) && width >= ( 1 << sps.getPCMLog2MinSize() );
  }
  else if( isModeInter( encTestmode ) )
  {
    // INTER MODES (ME + MERGE/SKIP)
    CHECK( slice.isIntra(), "Inter-mode should not be in the I-Slice mode list!" );

    if( getFastDeltaQp() )
    {
      if( encTestmode.type == ETM_MERGE_SKIP )
      {
        return false;
      }
      if( encTestmode.partSize != SIZE_2Nx2N || cs.area.lumaSize().width > cs.pcv->fastDeltaQPCuMaxSize )
      {
        return false; // only check necessary 2Nx2N Inter in fast deltaqp mode
      }
    }

    // --- Check if we can quit current mode using SAVE/LOAD coding history

    if( encTestmode.type == ETM_INTER_ME )
    {
      if( encTestmode.opts == ETO_STANDARD )
      {
        // NOTE: ETO_STANDARD is always done when early SKIP mode detection is enabled
        if( !m_pcEncCfg->getUseEarlySkipDetection() )
        {
          if( ( saveLoadTag == LOAD_ENC_INFO && ( sls.mergeFlag || sls.LICFlag ) ) || relatedCU.isSkip || relatedCU.isIntra /*|| relatedCU.imv || relatedCU.LICFlag*/ )
          {
            return false;
          }
        }
      }
      else if( ( saveLoadTag == LOAD_ENC_INFO ) && sls.mergeFlag )
      {
        return false;
      }
      else if( ( saveLoadTag == LOAD_ENC_INFO && ( ( encTestmode.opts & ETO_LIC ) == ETO_LIC ) != sls.LICFlag ) )
      {
        return false;
      }
      else if( ( saveLoadTag == LOAD_ENC_INFO && ( ( encTestmode.opts & ETO_IMV ) ) && ( !sls.imv ) ) || relatedCU.isSkip || relatedCU.isIntra )
      {
        return false;
      }
      else if( ( encTestmode.opts & ETO_IMV ) != 0 )
      {
        int imvOpt = ( encTestmode.opts & ETO_IMV ) >> ETO_IMV_SHIFT;

        if( imvOpt == 3 && cuECtx.get<double>( BEST_NO_IMV_COST ) * 1.06 < cuECtx.get<double>( BEST_IMV_COST ) )
        {
          return false;
        }
      }
    }

    if( encTestmode.type == ETM_AFFINE && ( ( saveLoadTag == LOAD_ENC_INFO && !sls.affineFlag ) || relatedCU.isIntra ) )
    {
      return false;
    }
    if( encTestmode.type == ETM_MERGE_FRUC && ( ( saveLoadTag == LOAD_ENC_INFO && sls.frucMode == 0 ) || relatedCU.isIntra ) )
    {
      return false;
    }

    if( ( encTestmode.opts & ETO_LIC ) == ETO_LIC && cs.sps->getSpsNext().getLICMode() == 2 )
    {
      if( partitioner.currArea().lumaSize().area() <= 32 )
      {
        return false;
      }
    }

    return true;
  }
  else if( isModeSplit( encTestmode ) )
  {
    if( cuECtx.get<bool>( HISTORY_NEED_TO_SAVE ) )
    {
      SaveLoadStruct &slsNext = getSaveLoadStructQuad( partitioner.currArea() );

      slsNext.tag     = SAVE_ENC_INFO;
      slsNext.partIdx = cuECtx.partIdx;

      // don't save saveLoadTag at this level anymore
      cuECtx.set( HISTORY_NEED_TO_SAVE, false );

      DTRACE( g_trace_ctx, D_SAVE_LOAD, "SaveLoad %d (%d %d) [%d %d] \n", SAVE_ENC_INFO, ( cuECtx.partIdx >> 8 ), ( cuECtx.partIdx & 0xff ), getHalvedIdx( cuECtx.cuW ), getHalvedIdx( cuECtx.cuH ) );

#if ENABLE_TRACING
      const Area& area = partitioner.currArea().Y();
      DTRACE( g_trace_ctx, D_SAVE_LOAD, "saving tag at %d,%d (%dx%d) -> %d\n", area.x, area.y, area.width >> 1, area.height >> 1, SAVE_ENC_INFO );
#endif
    }

    const PartSplit split = getPartSplit( encTestmode );
    if( !partitioner.canSplit( split, cs ) )
    {
      if( split == CU_HORZ_SPLIT ) cuECtx.set( DID_HORZ_SPLIT, false );
      if( split == CU_VERT_SPLIT ) cuECtx.set( DID_VERT_SPLIT, false );

      return false;
    }

    if( m_pcEncCfg->getUseContentBasedFastQtbt() )
    {
      const CompArea& currArea = partitioner.currArea().Y();
      int cuHeight  = currArea.height;
      int cuWidth   = currArea.width;

      const bool condIntraInter = ( m_pcEncCfg->getIntraPeriod() == 1  ) ? ( partitioner.currBtDepth == 0 ) : ( cuHeight > 32 && cuWidth > 32 );
      
      if( cuWidth == cuHeight && condIntraInter && ( getPartSplit( encTestmode ) == CU_HORZ_SPLIT || getPartSplit( encTestmode ) == CU_VERT_SPLIT ) )
      {
        const CPelBuf bufCurrArea = cs.getOrgBuf( partitioner.currArea().block( COMPONENT_Y ) );

        double horVal = 0;                                                                       
        double verVal = 0;
        double dupVal = 0;
        double dowVal = 0;

        const double th = ( m_pcEncCfg->getIntraPeriod() == 1 ) ? 1.2 : 1;

        unsigned j, k;

        for( j = 0; j < cuWidth - 1; j++ )
        {
          for( k = 0; k < cuHeight - 1; k++ )
          {
            horVal += abs( bufCurrArea.at( j + 1,            k     ) - bufCurrArea.at( j,            k ) );
            verVal += abs( bufCurrArea.at( j    ,            k + 1 ) - bufCurrArea.at( j,            k ) );
            dupVal += abs( bufCurrArea.at( j + 1,            k + 1 ) - bufCurrArea.at( j,            k ) );
            dowVal += abs( bufCurrArea.at( j + 1, cuHeight - k + 1 ) - bufCurrArea.at( j, cuHeight - k ) );
          }
        }
        if( horVal > th * verVal && sqrt( 2 ) * horVal > th * dowVal && sqrt( 2 ) * horVal > th * dupVal && getPartSplit( encTestmode ) == CU_HORZ_SPLIT )
        {
          return false;
        }
        if( th * dupVal < sqrt( 2 ) * verVal && th * dowVal < sqrt( 2 ) * verVal && th * horVal < verVal && getPartSplit( encTestmode ) == CU_VERT_SPLIT )
        {
          return false;
        }
      }

      if( m_pcEncCfg->getIntraPeriod() == 1 && cuHeight <= 32 && bestCS && bestCS->tus.size() == 1 && bestCU && bestCU->depth == partitioner.currDepth && partitioner.currBtDepth > 1 && isLuma( cs.chType ) )
      {
        if( !bestCU->rootCbf )
        {
          return false;
        }
      }
    }

    if( bestCU && bestCU->skip && bestCU->btDepth >= m_skipThreshold && !isModeSplit( cuECtx.lastTestMode ) )
    {
      return false;
    }

    switch( getPartSplit( encTestmode ) )
    {
      case CU_QUAD_SPLIT:
        {
          if( cuECtx.get<bool>( DID_HORZ_SPLIT ) && bestCU && bestCU->skip && bestCU->btDepth == partitioner.currBtDepth && partitioner.currBtDepth >= SKIPHORNOVERQT_DEPTH_TH )
          {
            return false;
          }

          if( bestCU )
          {
            unsigned maxBTD        = cs.pcv->getMaxBtDepth( slice, cs.chType );
            const CodingUnit *cuBR = bestCS->getCU( partitioner.currArea().Y().bottomRight() );

            if( bestCU && ( ( bestCU->btDepth == 0 &&                               maxBTD >= ( slice.isIntra() ? 3 : 2 ) )
                         || ( bestCU->btDepth == 1 && cuBR && cuBR->btDepth == 1 && maxBTD >= ( slice.isIntra() ? 4 : 3 ) ) )
                       && cuECtx.get<bool>( DID_HORZ_SPLIT ) && cuECtx.get<bool>( DID_VERT_SPLIT ) )
            {
              return false;
            }
          }
          if( m_pcEncCfg->getUseEarlyCU() && bestCS->cost != MAX_DOUBLE && bestCU && bestCU->skip )
          {
            return false;
          }
          if( getFastDeltaQp() && width <= slice.getPPS()->pcv->fastDeltaQPCuMaxSize )
          {
            return false;
          }
        }
        break;
      case CU_HORZ_SPLIT:
        if( saveLoadSplit & 0x02 )
        {
          cuECtx.set( CU_HORZ_SPLIT, false );
          return false;
        }
        break;
      case CU_VERT_SPLIT:
        if( cuECtx.get<bool>( DID_HORZ_SPLIT ) && bestCU && bestCU->skip && bestCU->btDepth == partitioner.currBtDepth && partitioner.currBtDepth >= SKIPHORNOVERQT_DEPTH_TH )
        {
          cuECtx.set( DID_VERT_SPLIT, false );
          return false;
        }

        if( saveLoadSplit & 0x04 )
        {
          cuECtx.set( DID_VERT_SPLIT, false );
          return false;
        }
        break;
      default:
        THROW( "Only CU split modes are governed by the EncModeCtrl" );
        return false;
        break;
    }

    return true;
  }
  else
  {
    THROW( "Unknown mode" );
  }
}

Bool EncModeCtrlMTnoRQT::useModeResult( const EncTestMode& encTestmode, CodingStructure*& tempCS, Partitioner& partitioner )
{
  xExtractFeatures( encTestmode, *tempCS );

  ComprCUCtx& cuECtx = m_ComprCUCtxList.back();


  if(      encTestmode.type == ETM_SPLIT_BT_H )
  {
    cuECtx.set( BEST_HORZ_SPLIT_COST, tempCS->cost );
  }
  else if( encTestmode.type == ETM_SPLIT_BT_V )
  {
    cuECtx.set( BEST_VERT_SPLIT_COST, tempCS->cost );
  }
  else if( encTestmode.type == ETM_INTRA && encTestmode.partSize == SIZE_2Nx2N )
  {
    const CodingUnit cu = *tempCS->getCU();

    if( !cu.emtFlag )
    {
      cuECtx.bestEmtSize2Nx2N1stPass = tempCS->cost;
    }
  }

  if( m_pcEncCfg->getIMV4PelFast() && m_pcEncCfg->getIMV() && encTestmode.type == ETM_INTER_ME )
  {
    int imvMode = ( encTestmode.opts & ETO_IMV ) >> ETO_IMV_SHIFT;

    if( imvMode == 1 )
    {
      if( tempCS->cost < cuECtx.get<double>( BEST_IMV_COST ) )
      {
        cuECtx.set( BEST_IMV_COST, tempCS->cost );
      }
    }
    else if( imvMode == 0 )
    {
      if( tempCS->cost < cuECtx.get<double>( BEST_NO_IMV_COST ) )
      {
        cuECtx.set( BEST_NO_IMV_COST, tempCS->cost );
      }
    }
  }

  if( encTestmode.type == ETM_INTRA )
  {
    const int nsstIdx = ( encTestmode.opts & ETO_NSST ) >> ETO_NSST_SHIFT;

    if( !cuECtx.bestCS || ( tempCS->cost >= cuECtx.bestCS->cost && cuECtx.bestCS->cus.size() == 1 && CU::isIntra( *cuECtx.bestCS->cus[0] ) )
                       || ( tempCS->cost <  cuECtx.bestCS->cost && CU::isIntra( *tempCS->cus[0] ) ) )
    {
      cuECtx.set( SKIP_OTHER_NSST, !tempCS->cus[0]->rootCbf );
    }

    cuECtx.set( LAST_NSST_IDX, nsstIdx );
  }

  // for now just a simple decision based on RD-cost or choose tempCS if bestCS is not yet coded
  if( !cuECtx.bestCS || tempCS->features[ENC_FT_RD_COST] < cuECtx.bestCS->features[ENC_FT_RD_COST] )
  {
    cuECtx.bestCS = tempCS;
    cuECtx.bestCU = tempCS->cus[0];
    cuECtx.bestTU = cuECtx.bestCU->firstTU;

    if( ( encTestmode.type == ETM_INTER_ME ) || ( encTestmode.type == ETM_MERGE_SKIP ) || ( encTestmode.type == ETM_MERGE_FRUC ) || ( encTestmode.type == ETM_AFFINE ) ) //if it is an inter mode
    {
      //Here we take the best cost of both inter modes. We are assuming only the inter modes (and all of them) have come before the intra modes!!!
      cuECtx.bestInterCost = cuECtx.bestCS->cost;
    }

    return true;
  }
  else
  {
    return false;
  }
}



