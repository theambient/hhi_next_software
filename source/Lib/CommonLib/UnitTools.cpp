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

/** \file     UnitTool.cpp
 *  \brief    defines operations for basic units
 */

#include "UnitTools.h"

#include "dtrace_next.h"

#include "Unit.h"
#include "Slice.h"
#include "Picture.h"

#include <utility>
#include <algorithm>

// CS tools

UInt64 CS::getEstBits(const CodingStructure &cs)
{
  return cs.fracBits >> SCALE_BITS;
}


static void xInitFrucMvpEl( CodingStructure& cs, Int x, Int y, Int nCurPOC, Int nTargetRefIdx, Int nTargetRefPOC, Int nCurRefIdx, Int nCurRefPOC, Int nColPOC, RefPicList eRefPicList, const Picture* pColPic )
{
  const unsigned scale = ( cs.pcv->noMotComp ? 1 : 4 * std::max<Int>( 1, 4 * AMVP_DECIMATION_FACTOR / 4 ) );

  const unsigned mask = ~( scale - 1 );

  const Position pos = Position{ PosType( x & mask ), PosType( y & mask ) };

  CHECK( pos.x >= cs.picture->Y().width || pos.y >= cs.picture->Y().height, "size exceed" );

  const MotionInfo &frucMi = pColPic->cs->getMotionInfo( pos );

  if( frucMi.interDir & ( 1 << eRefPicList ) )
  {
    CHECK( frucMi.isInter == false && frucMi.interDir < 1, "invalid motion info" );

    Int nColRefPOC = pColPic->cs->slice->getRefPOC( eRefPicList, frucMi.refIdx[eRefPicList] );
    Mv mvColPic = frucMi.mv[eRefPicList];
    if( cs.sps->getSpsNext().getUseHighPrecMv() )
    {
      mvColPic.setHighPrec();
    }

    Mv mv2CurRefPic = PU::scaleMv( mvColPic, nCurPOC, nCurRefPOC, nColPOC, nColRefPOC, cs.slice );

    Int xCurPic = 0;
    Int yCurPic = 0;
    if( cs.sps->getSpsNext().getUseHighPrecMv() )
    {
      Int nOffset = 1 << ( VCEG_AZ07_MV_ADD_PRECISION_BIT_FOR_STORE + 1 );

      xCurPic = x + ( MIN_PU_SIZE >> 1 ) - ( ( mv2CurRefPic.getHor() + nOffset ) >> ( 2 + VCEG_AZ07_MV_ADD_PRECISION_BIT_FOR_STORE ) );
      yCurPic = y + ( MIN_PU_SIZE >> 1 ) - ( ( mv2CurRefPic.getVer() + nOffset ) >> ( 2 + VCEG_AZ07_MV_ADD_PRECISION_BIT_FOR_STORE ) );
    }
    else
    {
      xCurPic = x - ( ( mv2CurRefPic.getHor() + 2 ) >> 2 ) + ( MIN_PU_SIZE >> 1 );
      yCurPic = y - ( ( mv2CurRefPic.getVer() + 2 ) >> 2 ) + ( MIN_PU_SIZE >> 1 );
    }

    if( 0 <= xCurPic && xCurPic < cs.picture->Y().width && 0 <= yCurPic && yCurPic < cs.picture->Y().height )
    {
      MotionInfo &curMiFRUC = cs.getMotionInfoFRUC( Position{ xCurPic, yCurPic } );

      if( !( curMiFRUC.interDir & ( 1 << eRefPicList ) ) )
      {
        Mv     mv2TargetPic = nCurRefIdx == nTargetRefIdx ? mv2CurRefPic : PU::scaleMv( mvColPic, nCurPOC, nTargetRefPOC, nColPOC, nColRefPOC, cs.slice );
        curMiFRUC.mv    [eRefPicList] = mv2TargetPic;
        curMiFRUC.refIdx[eRefPicList] = nTargetRefIdx;
        curMiFRUC.interDir           += ( 1 << eRefPicList );
      }
    }
  }
}

void CS::initFrucMvp( CodingStructure &cs )
{
  const Int width  = cs.picture->Y().width;
  const Int height = cs.picture->Y().height;

  const Int nCurPOC = cs.slice->getPOC();

  for( Int nRefPicList = 0; nRefPicList < 2; nRefPicList++ )
  {
    RefPicList eRefPicList = ( RefPicList ) nRefPicList;
    for( Int nRefIdx = 0; nRefIdx < cs.slice->getNumRefIdx( eRefPicList ); nRefIdx++ )
    {
      const Int nTargetRefIdx = 0;
      const Int nTargetRefPOC = cs.slice->getRefPOC( eRefPicList, nTargetRefIdx );
      const Int nCurRefIdx    = nRefIdx;
      const Int nCurRefPOC    = cs.slice->getRefPOC( eRefPicList, nCurRefIdx );
      const Int nColPOC       = cs.slice->getRefPOC( eRefPicList, nRefIdx );

      const Picture* pColPic  = cs.slice->getRefPic( eRefPicList, nRefIdx );

      if( pColPic->cs->slice->isIntra() )
      {
        continue;
      }

      const Int log2ctuSize = g_aucLog2[cs.pcv->maxCUWidth];

      for( Int yCtu = 0; yCtu < height; yCtu += ( 1 << log2ctuSize ) )
      {
        for( Int xCtu = 0; xCtu < width; xCtu += ( 1 << log2ctuSize ) )
        {
          for( Int yCtuLgM1 = 0; yCtuLgM1 < ( 1 << log2ctuSize ); yCtuLgM1 += ( 1 << ( log2ctuSize - 1 ) ) )
          {
            for( Int xCtuLgM1 = 0; xCtuLgM1 < ( 1 << log2ctuSize ); xCtuLgM1 += ( 1 << ( log2ctuSize - 1 ) ) )
            {
              for( Int yCtuLgM2 = 0; yCtuLgM2 < ( 1 << ( log2ctuSize - 1 ) ); yCtuLgM2 += ( 1 << ( log2ctuSize - 2 ) ) )
              {
                for( Int xCtuLgM2 = 0; xCtuLgM2 < ( 1 << ( log2ctuSize - 1 ) ); xCtuLgM2 += ( 1 << ( log2ctuSize - 2 ) ) )
                {
                  for( Int yCtuLgM3 = 0; yCtuLgM3 < ( 1 << ( log2ctuSize - 2 ) ); yCtuLgM3 += ( 1 << ( log2ctuSize - 3 ) ) )
                  {
                    for( Int xCtuLgM3 = 0; xCtuLgM3 < ( 1 << ( log2ctuSize - 2 ) ); xCtuLgM3 += ( 1 << ( log2ctuSize - 3 ) ) )
                    {
                      for( Int yCtuLgM4 = 0; yCtuLgM4 < ( 1 << ( log2ctuSize - 3 ) ); yCtuLgM4 += ( 1 << ( log2ctuSize - 4 ) ) )
                      {
                        for( Int xCtuLgM4 = 0; xCtuLgM4 < ( 1 << ( log2ctuSize - 3 ) ); xCtuLgM4 += ( 1 << ( log2ctuSize - 4 ) ) )
                        {
                          if( log2ctuSize - 4 > MIN_CU_LOG2 )
                          {
                            for( Int yCtuLgM5 = 0; yCtuLgM5 < ( 1 << ( log2ctuSize - 4 ) ); yCtuLgM5 += ( 1 << ( log2ctuSize - 5 ) ) )
                            {
                              for( Int xCtuLgM5 = 0; xCtuLgM5 < ( 1 << ( log2ctuSize - 4 ) ); xCtuLgM5 += ( 1 << ( log2ctuSize - 5 ) ) )
                              {
                                const Int x = xCtu + xCtuLgM1 + xCtuLgM2 + xCtuLgM3 + xCtuLgM4 + xCtuLgM5;
                                const Int y = yCtu + yCtuLgM1 + yCtuLgM2 + yCtuLgM3 + yCtuLgM4 + yCtuLgM5;

                                if( x >= width || y >= height )
                                {
                                  continue;
                                }

                                xInitFrucMvpEl( cs, x, y, nCurPOC, nTargetRefIdx, nTargetRefPOC, nCurRefIdx, nCurRefPOC, nColPOC, eRefPicList, pColPic );
                              }
                            }
                          }
                          else
                          {
                            const Int x = xCtu + xCtuLgM1 + xCtuLgM2 + xCtuLgM3 + xCtuLgM4;
                            const Int y = yCtu + yCtuLgM1 + yCtuLgM2 + yCtuLgM3 + yCtuLgM4;

                            if( x >= width || y >= height )
                            {
                              continue;
                            }

                            xInitFrucMvpEl( cs, x, y, nCurPOC, nTargetRefIdx, nTargetRefPOC, nCurRefIdx, nCurRefPOC, nColPOC, eRefPicList, pColPic );
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

bool CS::isDoubleITree( const CodingStructure &cs )
{
  return cs.slice->isIntra() && !cs.pcv->ISingleTree;
}

UnitArea CS::getArea( const CodingStructure &cs, const UnitArea &area )
{
  return isDoubleITree( cs ) ? area.singleChan( cs.chType ) : area;
}


// CU tools

bool CU::isIntra(const CodingUnit &cu)
{
  return cu.predMode == MODE_INTRA;
}

bool CU::isInter(const CodingUnit &cu)
{
  return cu.predMode == MODE_INTER;
}

bool CU::isRDPCMEnabled(const CodingUnit& cu)
{
  return cu.cs->sps->getSpsRangeExtension().getRdpcmEnabledFlag(cu.predMode == MODE_INTRA ? RDPCM_SIGNAL_IMPLICIT : RDPCM_SIGNAL_EXPLICIT);
}

UInt CU::getQuadtreeTULog2MinSizeInCU(const CodingUnit& cu)
{
  const SPS &sps = *cu.cs->sps;

  const UInt log2CbSize         = g_aucLog2[cu.lumaSize().width];
  const UInt quadtreeTUMaxDepth =  isIntra(cu) ? sps.getQuadtreeTUMaxDepthIntra() : sps.getQuadtreeTUMaxDepthInter();
  const Int intraSplitFlag      = (isIntra(cu) && cu.partSize == SIZE_NxN) ? 1 : 0;
  const Int interSplitFlag      = ((quadtreeTUMaxDepth == 1) && isInter(cu) && (cu.partSize != SIZE_2Nx2N));

  UInt log2MinTUSizeInCU = 0;

  if (log2CbSize < (sps.getQuadtreeTULog2MinSize() + quadtreeTUMaxDepth - 1 + interSplitFlag + intraSplitFlag))
  {
    // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is < QuadtreeTULog2MinSize
    log2MinTUSizeInCU = sps.getQuadtreeTULog2MinSize();
  }
  else
  {
    // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still >= QuadtreeTULog2MinSize
    log2MinTUSizeInCU = log2CbSize - (quadtreeTUMaxDepth - 1 + interSplitFlag + intraSplitFlag); // stop when trafoDepth == hierarchy_depth = splitFlag

    if (log2MinTUSizeInCU > sps.getQuadtreeTULog2MaxSize())
    {
      // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still > QuadtreeTULog2MaxSize
      log2MinTUSizeInCU = sps.getQuadtreeTULog2MaxSize();
    }
  }
  return log2MinTUSizeInCU;
}

bool CU::isLosslessCoded(const CodingUnit &cu)
{
  return cu.cs->pps->getTransquantBypassEnabledFlag() && cu.transQuantBypass;
}

bool CU::isSameSlice(const CodingUnit& cu, const CodingUnit& cu2)
{
  return cu.slice->getIndependentSliceIdx() == cu2.slice->getIndependentSliceIdx();
}

bool CU::isSameTile(const CodingUnit& cu, const CodingUnit& cu2)
{
  return cu.tileIdx == cu2.tileIdx;
}

bool CU::isSameSliceAndTile(const CodingUnit& cu, const CodingUnit& cu2)
{
  return ( cu.slice->getIndependentSliceIdx() == cu2.slice->getIndependentSliceIdx() ) && ( cu.tileIdx == cu2.tileIdx );
}

bool CU::isSameCtu(const CodingUnit& cu, const CodingUnit& cu2)
{
  UInt ctuSizeBit = g_aucLog2[cu.cs->sps->getMaxCUWidth()];

  Position pos1Ctu(cu.lumaPos().x  >> ctuSizeBit, cu.lumaPos().y  >> ctuSizeBit);
  Position pos2Ctu(cu2.lumaPos().x >> ctuSizeBit, cu2.lumaPos().y >> ctuSizeBit);

  return pos1Ctu.x == pos2Ctu.x && pos1Ctu.y == pos2Ctu.y;
}

UInt CU::getIntraSizeIdx(const CodingUnit &cu)
{
  UInt uiShift  = (cu.partSize == SIZE_NxN ? 1 : 0);

  UChar uiWidth = cu.lumaSize().width >> uiShift;
  UInt  uiCnt   = 0;

  while (uiWidth)
  {
    uiCnt++;
    uiWidth >>= 1;
  }

  uiCnt -= 2;

  return uiCnt > 6 ? 6 : uiCnt;
}

bool CU::isLastSubCUOfCtu( const CodingUnit &cu )
{
  const SPS &sps      = *cu.cs->sps;
  const Area cuAreaY = CS::isDoubleITree( *cu.cs ) ? Area( recalcPosition( cu.chromaFormat, cu.cs->chType, CHANNEL_TYPE_LUMA, cu.blocks[cu.cs->chType].pos() ), recalcSize( cu.chromaFormat, cu.cs->chType, CHANNEL_TYPE_LUMA, cu.blocks[cu.cs->chType].size() ) ) : ( const Area& ) cu.Y();

  return ( ( ( ( cuAreaY.x + cuAreaY.width  ) & cu.cs->pcv->maxCUWidthMask  ) == 0 || cuAreaY.x + cuAreaY.width  == sps.getPicWidthInLumaSamples()  ) &&
           ( ( ( cuAreaY.y + cuAreaY.height ) & cu.cs->pcv->maxCUHeightMask ) == 0 || cuAreaY.y + cuAreaY.height == sps.getPicHeightInLumaSamples() ) );
}

UInt CU::getCtuAddr(const CodingUnit &cu)
{
  return getCtuAddr( cu.blocks[cu.cs->chType].lumaPos(), *cu.cs->pcv );
}

int CU::predictQP (const CodingUnit& cu, const int prevQP)
{
  const CodingStructure &cs = *cu.cs;


  // only predict within the same CTU, use HEVC's above+left prediction
  const int a = (cu.lumaPos().y & cs.pcv->maxCUHeightMask) ? (cs.getCU (cu.lumaPos().offset ( 0,-1)))->qp : prevQP;
  const int b = (cu.lumaPos().x & cs.pcv->maxCUWidthMask ) ? (cs.getCU (cu.lumaPos().offset (-1, 0)))->qp : prevQP;

  return (a + b + 1) >> 1;
}

bool CU::isQGStart( const CodingUnit& cu )
{
  const SPS &sps = *cu.cs->sps;
  const PPS &pps = *cu.cs->pps;

  return ( cu.Y().x % ( 1 << ( g_aucLog2[sps.getMaxCUWidth()]  - pps.getMaxCuDQPDepth() ) ) == 0 &&
           cu.Y().y % ( 1 << ( g_aucLog2[sps.getMaxCUHeight()] - pps.getMaxCuDQPDepth() ) ) == 0 );
}

UInt CU::getNumPUs(const CodingUnit& cu)
{
  UInt cnt = 0;
  PredictionUnit *pu = cu.firstPU;

  do
  {
    cnt++;
  } while( ( pu != cu.lastPU ) && ( pu = pu->next ) );

  return cnt;
}

void CU::addPUs( CodingUnit& cu )
{
  const auto puAreas = PartitionerImpl::getPUPartitioning( cu );

  for( const auto &puArea : puAreas )
  {
    cu.cs->addPU( CS::getArea( *cu.cs, puArea ) );
  }
}


PartSplit CU::getSplitAtDepth( const CodingUnit& cu, const unsigned depth )
{
  if( depth >= cu.depth ) return CU_DONT_SPLIT;

  const PartSplit cuSplitType = PartSplit( ( cu.splitSeries >> ( depth * SPLIT_DMULT ) ) & SPLIT_MASK );

  if     ( cuSplitType == CU_QUAD_SPLIT    ) return CU_QUAD_SPLIT;

  else if( cuSplitType == CU_HORZ_SPLIT    ) return CU_HORZ_SPLIT;

  else if( cuSplitType == CU_VERT_SPLIT    ) return CU_VERT_SPLIT;

  else   { THROW( "Unknown split mode"    ); return CU_QUAD_SPLIT; }
}

bool CU::hasNonTsCodedBlock( const CodingUnit& cu )
{
  bool hasAnyNonTSCoded = false;

  for( auto &currTU : traverseTUs( cu ) )
  {
    for( UInt i = 0; i < ::getNumberValidTBlocks( *cu.cs->pcv ); i++ )
    {
      hasAnyNonTSCoded |= ( currTU.blocks[i].valid() && !currTU.transformSkip[i] && TU::getCbf( currTU, ComponentID( i ) ) );
    }
  }

  return hasAnyNonTSCoded;
}

UInt CU::getNumNonZeroCoeffNonTs( const CodingUnit& cu )
{
  UInt count = 0;
  for( auto &currTU : traverseTUs( cu ) )
  {
    count += TU::getNumNonZeroCoeffsNonTS( currTU );
  }

  return count;
}


bool CU::isLICFlagPresent( const CodingUnit& cu )
{
  if( CU::isIntra(cu) || !cu.slice->getUseLIC() )
  {
    return false;
  }
  const SPSNext& spsNext = cu.slice->getSPS()->getSpsNext();
  if( cu.cs->pcv->only2Nx2N )
  {
    if( cu.firstPU->mergeFlag && cu.firstPU->frucMrgMode == FRUC_MERGE_OFF )
    {
      return false;
    }
  }
  else
  {
    if( cu.partSize == SIZE_2Nx2N && cu.firstPU->mergeFlag && cu.firstPU->frucMrgMode == FRUC_MERGE_OFF )
    {
      return false;
    }
  }
  if( spsNext.getLICMode() == 2 ) // selective LIC
  {
    if( cu.cs->pcv->rectCUs && cu.cs->pcv->only2Nx2N )
    {
      if( cu.lumaSize().area() <= 32 )
      {
        return false;
      }
    }
    else
    {
      if( cu.partSize != SIZE_2Nx2N )
      {
        return false;
      }
    }
  }
  if( cu.affine )
  {
    return false;
  }
  return true;
}

PUTraverser CU::traversePUs( CodingUnit& cu )
{
  return PUTraverser( cu.firstPU, cu.lastPU->next );
}

TUTraverser CU::traverseTUs( CodingUnit& cu )
{
  return TUTraverser( cu.firstTU, cu.lastTU->next );
}

cPUTraverser CU::traversePUs( const CodingUnit& cu )
{
  return cPUTraverser( cu.firstPU, cu.lastPU->next );
}

cTUTraverser CU::traverseTUs( const CodingUnit& cu )
{
  return cTUTraverser( cu.firstTU, cu.lastTU->next );
}

// PU tools
int PU::getIntraMPMs( const PredictionUnit &pu, unsigned* mpm, const ChannelType &channelType /*= CHANNEL_TYPE_LUMA*/, const bool isChromaMDMS /*= false*/, const unsigned startIdx /*= 0*/ )
{
  const unsigned numMPMs = isChromaMDMS ? NUM_DM_MODES : pu.cs->pcv->numMPMs;

  if( pu.cs->sps->getSpsNext().getUseIntra65Ang()
      || isChromaMDMS )
  {
    Int  numCand = -1;
    UInt modeIdx =  0;

    Bool includedMode[NUM_INTRA_MODE];
    memset(includedMode, false, sizeof(includedMode));
    if ( isChromaMDMS )
    {
      // mark LMChroma already included
      for ( Int i = LM_CHROMA_IDX; i < LM_CHROMA_IDX + NUM_LMC_MODE; i++ )
      {
        includedMode[ i ] = true;
      }
      // mark direct modes already included
      for ( Int i = 0; i < startIdx; i++ )
      {
        includedMode[ mpm[i] ] = true;
      }
      modeIdx = startIdx;
    }

    const CompArea& area = pu.block( getFirstComponentOfChannel( channelType ) );
    const Position posLT = area.topLeft();
    const Position posRT = area.topRight();
    const Position posLB = area.bottomLeft();

    //left
    if( modeIdx < numMPMs )
    {
      const PredictionUnit *puLeft = pu.cs->getPURestricted( posLB.offset( -1, 0 ), pu, channelType );
      if( puLeft && CU::isIntra( *puLeft->cu ) )
      {
        mpm[modeIdx] = puLeft->intraDir[channelType];
        if( !includedMode[ mpm[modeIdx] ] ) { includedMode[ mpm[modeIdx++] ] = true; }
      }
    }
    //above
    if( modeIdx < numMPMs )
    {
      const PredictionUnit *puAbove = pu.cs->getPURestricted( posRT.offset( 0, -1 ), pu, channelType );
      if( puAbove && CU::isIntra( *puAbove->cu ) )
      {
        mpm[modeIdx] = puAbove->intraDir[channelType];
        if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
      }
    }

    numCand = ( modeIdx > 1 ) ? 3 : 2;

    if ( ! isChromaMDMS )
    {
      //PLANAR
      mpm[modeIdx] = PLANAR_IDX;
      if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
      //DC
      mpm[modeIdx] = DC_IDX;
      if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
    }

    //left bottom
    if( modeIdx < numMPMs )
    {
      const PredictionUnit *puLeftBottom = pu.cs->getPURestricted( posLB.offset( -1, 1 ), pu, channelType );
      if( puLeftBottom && CU::isIntra( *puLeftBottom->cu ) )
      {
        mpm[modeIdx] = puLeftBottom->intraDir[channelType];
        if( mpm[modeIdx] < NUM_INTRA_MODE ) // exclude uninitialized PUs
        {
          if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
        }
      }
    }
    // above right
    if( modeIdx < numMPMs )
    {
      const PredictionUnit *puAboveRight = pu.cs->getPURestricted( posRT.offset( 1, -1 ), pu, channelType );
      if( puAboveRight && CU::isIntra( *puAboveRight->cu ) )
      {
        mpm[modeIdx] = puAboveRight->intraDir[channelType];
        if( mpm[modeIdx] < NUM_INTRA_MODE ) // exclude uninitialized PUs
        {
          if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
        }
      }
    }

    // above left
    if( modeIdx < numMPMs )
    {
      const PredictionUnit *puAboveLeft  = pu.cs->getPURestricted( posLT.offset( -1, -1 ), pu, channelType );
      if( puAboveLeft && CU::isIntra( *puAboveLeft->cu ) )
      {
        mpm[modeIdx] = puAboveLeft->intraDir[channelType];
        if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
      }
    }

    if ( isChromaMDMS )
    {
      //PLANAR
      if ( modeIdx < numMPMs )
      {
        mpm[modeIdx] = PLANAR_IDX;
        if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
      }
      //DC
      if ( modeIdx < numMPMs )
      {
        mpm[modeIdx] = DC_IDX;
        if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
      }
    }

    // -+1 derived angular modes
    const UInt numAddedModes = modeIdx;
    const Int  offset        = (Int)NUM_LUMA_MODE - 5;
    const Int  mod           = offset + 3;

    for( UInt idx = 0; idx < numAddedModes && modeIdx < numMPMs; idx++ )
    {
      UInt mode = mpm[idx];
      if( mode > DC_IDX )
      {
        // -1
        mpm[modeIdx] = ((mode + offset) % mod) + 2;
        if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }

        if( modeIdx == numMPMs ) { break; }

        // +1
        mpm[modeIdx] = ((mode - 1) % mod) + 2;
        if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
      }
    }

    // default modes
    UInt defaultIntraModes[] ={ PLANAR_IDX, DC_IDX, VER_IDX, HOR_IDX, 2, DIA_IDX };
    CHECK( modeIdx <= 1, "Invalid mode" );
    for( UInt idx = 2; idx < numMPMs && modeIdx < numMPMs; idx++ )
    {
      mpm[modeIdx] = defaultIntraModes[idx];
      if( !includedMode[mpm[modeIdx]] ) { includedMode[mpm[modeIdx++]] = true; }
    }

    CHECK( modeIdx != numMPMs, "Invalid mode" );
    for( UInt i = 0; i < numMPMs; i++ )
    {
      CHECK( mpm[i] >= NUM_LUMA_MODE, "Invalid mode" );
    }
    CHECK( numCand == 0 || numCand > numMPMs, "Invalid number of MPMs" );
    return numCand;
  }
  else
  {
    Int numCand = -1;
    Int leftIntraDir = DC_IDX, aboveIntraDir = DC_IDX;

    const CompArea& area = pu.block( getFirstComponentOfChannel( channelType ) );
    const Position& pos  = area.pos();

    // Get intra direction of left PU
    const PredictionUnit *puLeft = pu.cs->getPURestricted( pos.offset( -1, 0 ), pu, channelType );

    if( puLeft && CU::isIntra( *puLeft->cu ) )
    {
      leftIntraDir = puLeft->intraDir[channelType];

      if( isChroma( channelType ) && leftIntraDir == DM_CHROMA_IDX )
      {
        leftIntraDir = puLeft->intraDir[0];
      }
    }

    // Get intra direction of above PU
    const PredictionUnit* puAbove = pu.cs->getPURestricted( pos.offset( 0, -1 ), pu, channelType );

    if( puAbove && CU::isIntra( *puAbove->cu ) && CU::isSameCtu( *pu.cu, *puAbove->cu ) )
    {
      aboveIntraDir = puAbove->intraDir[channelType];

      if( isChroma( channelType ) && aboveIntraDir == DM_CHROMA_IDX )
      {
        aboveIntraDir = puAbove->intraDir[0];
      }
    }

    CHECK( 2 >= numMPMs, "Invalid number of most probable modes" );

    const Int offset = 29;

    const Int mod    = offset + 3;

    if( leftIntraDir == aboveIntraDir )
    {
      numCand = 1;

      if( leftIntraDir > DC_IDX ) // angular modes
      {
        mpm[0] =   g_intraMode65to33AngMapping[leftIntraDir];
        mpm[1] = ((g_intraMode65to33AngMapping[leftIntraDir] + offset) % mod) + 2;
        mpm[2] = ((g_intraMode65to33AngMapping[leftIntraDir] - 1)      % mod) + 2;
      }
      else //non-angular
      {
        mpm[0] = g_intraMode65to33AngMapping[PLANAR_IDX];
        mpm[1] = g_intraMode65to33AngMapping[DC_IDX];
        mpm[2] = g_intraMode65to33AngMapping[VER_IDX];
      }
    }
    else
    {
      numCand = 2;

      mpm[0] = g_intraMode65to33AngMapping[leftIntraDir];
      mpm[1] = g_intraMode65to33AngMapping[aboveIntraDir];

      if( leftIntraDir && aboveIntraDir ) //both modes are non-planar
      {
        mpm[2] = g_intraMode65to33AngMapping[PLANAR_IDX];
      }
      else
      {
        mpm[2] = g_intraMode65to33AngMapping[(leftIntraDir + aboveIntraDir) < 2 ? VER_IDX : DC_IDX];
      }
    }
    for( UInt i = 0; i < numMPMs; i++ )
    {
      mpm[i] = g_intraMode33to65AngMapping[mpm[i]];
      CHECK( mpm[i] >= NUM_LUMA_MODE, "Invalid MPM" );
    }
    CHECK( numCand == 0, "No candidates found" );
    return numCand;
  }
}

int PU::getDMModes(const PredictionUnit &pu, unsigned *modeList)
{
  int numDMs = 0;

  if ( CS::isDoubleITree( *pu.cs ) )
  {
    const Position lumaPos  = pu.blocks[pu.cs->chType].lumaPos();
    const Size chromaSize   = pu.blocks[pu.cs->chType].size();
    const UInt scaleX       = getComponentScaleX( pu.blocks[pu.cs->chType].compID, pu.chromaFormat );
    const UInt scaleY       = getComponentScaleY( pu.blocks[pu.cs->chType].compID, pu.chromaFormat );
    const Size lumaSize     = Size( chromaSize.width << scaleX, chromaSize.height << scaleY );
    const Int centerOffsetX = ( lumaSize.width  == 4 ) ? ( 0 ) : ( ( lumaSize.width  >> 1 ) - 1 );
    const Int centerOffsetY = ( lumaSize.height == 4 ) ? ( 0 ) : ( ( lumaSize.height >> 1 ) - 1 );
    unsigned candModes[ NUM_DM_MODES ];
    static_assert( 5 <= NUM_DM_MODES, "Too many chroma direkt modes" );
    // center
    const PredictionUnit *lumaC  = pu.cs->picture->cs->getPU( lumaPos.offset( centerOffsetX     , centerOffsetY       ), CHANNEL_TYPE_LUMA );
    candModes[ 0 ] = lumaC->intraDir [ CHANNEL_TYPE_LUMA ];
    // top-left
    const PredictionUnit *lumaTL = pu.cs->picture->cs->getPU( lumaPos                                                  , CHANNEL_TYPE_LUMA );
    candModes[ 1 ] = lumaTL->intraDir[ CHANNEL_TYPE_LUMA ];
    // top-right
    const PredictionUnit *lumaTR = pu.cs->picture->cs->getPU( lumaPos.offset( lumaSize.width - 1, 0                   ), CHANNEL_TYPE_LUMA );
    candModes[ 2 ] = lumaTR->intraDir[ CHANNEL_TYPE_LUMA ];
    // bottom-left
    const PredictionUnit *lumaBL = pu.cs->picture->cs->getPU( lumaPos.offset( 0                 , lumaSize.height - 1 ), CHANNEL_TYPE_LUMA );
    candModes[ 3 ] = lumaBL->intraDir[ CHANNEL_TYPE_LUMA ];
    // bottom-right
    const PredictionUnit *lumaBR = pu.cs->picture->cs->getPU( lumaPos.offset( lumaSize.width - 1, lumaSize.height - 1 ), CHANNEL_TYPE_LUMA );
    candModes[ 4 ] = lumaBR->intraDir[ CHANNEL_TYPE_LUMA ];
    // remove duplicates
    for ( Int i = 0; i < NUM_DM_MODES; i++ )
    {
      const unsigned mode = candModes[ i ];
      bool isIncluded     = false;
      for ( Int j = 0; j < numDMs; j++ )
      {
        if ( mode == modeList[ j ] )
        {
          isIncluded = true;
          break;
        }
      }
      if ( ! isIncluded )
      {
        modeList[ numDMs++ ] = mode;
      }
    }
  }
  else
  {
    const UInt lumaMode  = pu.intraDir[ CHANNEL_TYPE_LUMA ];
    modeList[ numDMs++ ] = lumaMode;
  }

  return numDMs;
}

void PU::getIntraChromaCandModes( const PredictionUnit &pu, unsigned modeList[NUM_CHROMA_MODE] )
{
  if ( pu.cs->sps->getSpsNext().getUseMDMS() )
  {
    static_assert( NUM_DM_MODES + 6 <= NUM_CHROMA_MODE, "Too many chroma MPMs" );
    modeList[ 0 ] = LM_CHROMA_IDX;
    modeList[ 1 ] = MMLM_CHROMA_IDX;
    modeList[ 2 ] = LM_CHROMA_F1_IDX;
    modeList[ 3 ] = LM_CHROMA_F2_IDX;
    modeList[ 4 ] = LM_CHROMA_F3_IDX;
    modeList[ 5 ] = LM_CHROMA_F4_IDX;
    Int numDMs = getDMModes( pu, &modeList[6] );
    if ( numDMs < NUM_DM_MODES )
    {
      PU::getIntraMPMs( pu, &modeList[ 6 ], CHANNEL_TYPE_CHROMA, true, numDMs );
    }
  }
  else
  {
    static_assert( 11 <= NUM_CHROMA_MODE, "Too many chroma MPMs" );
    modeList[  0 ] = PLANAR_IDX;
    modeList[  1 ] = VER_IDX;
    modeList[  2 ] = HOR_IDX;
    modeList[  3 ] = DC_IDX;
    modeList[  4 ] = LM_CHROMA_IDX;
    modeList[  5 ] = MMLM_CHROMA_IDX;
    modeList[  6 ] = LM_CHROMA_F1_IDX;
    modeList[  7 ] = LM_CHROMA_F2_IDX;
    modeList[  8 ] = LM_CHROMA_F3_IDX;
    modeList[  9 ] = LM_CHROMA_F4_IDX;
    modeList[ 10 ] = DM_CHROMA_IDX;

    const PredictionUnit *lumaPU = CS::isDoubleITree( *pu.cs ) ? pu.cs->picture->cs->getPU( pu.blocks[pu.cs->chType].lumaPos(), CHANNEL_TYPE_LUMA ) : &pu;
    const UInt lumaMode          = lumaPU->intraDir[ CHANNEL_TYPE_LUMA ];
    for (Int i = 0; i < 4; i++)
    {
      if( lumaMode == modeList[ i ] )
      {
        modeList[ i ] = VDIA_IDX;
        break;
      }
    }
  }
}

bool PU::isLMCMode(unsigned mode)
{
  return ( mode >= LM_CHROMA_IDX && mode <= LM_CHROMA_F4_IDX );
}

bool PU::isMMLMEnabled(const PredictionUnit &pu)
{
  if ( pu.cs->sps->getSpsNext().isELMModeMMLM() )
  {
    const Int blockSize = pu.Cb().width + pu.Cb().height;
    const Int minSize   = g_aiMMLM_MinSize[ CU::isIntra(*(pu.cu)) ? 0 : 1 ];
    return blockSize >= minSize;
  }
  return false;
}

bool PU::isMFLMEnabled(const PredictionUnit &pu)
{
  if ( pu.cs->sps->getSpsNext().isELMModeMFLM() )
  {
    const Int blockSize = pu.Cb().width + pu.Cb().height;
    const Int minSize   = g_aiMFLM_MinSize[ CU::isIntra(*(pu.cu)) ? 0 : 1 ];
    return blockSize >= minSize;
  }
  return false;
}

bool PU::isLMCModeEnabled(const PredictionUnit &pu, unsigned mode)
{
  if ( pu.cs->sps->getSpsNext().getUseLMChroma() )
  {
    if ( mode == LM_CHROMA_IDX )
    {
      return true;
    }
    else if ( ( mode == MMLM_CHROMA_IDX && PU::isMMLMEnabled( pu ) ) || ( mode >= LM_CHROMA_F1_IDX && mode <= LM_CHROMA_F4_IDX && PU::isMFLMEnabled( pu ) ) )
    {
      return true;
    }
  }
  return false;
}

bool PU::isChromaIntraModeCrossCheckMode(const PredictionUnit &pu)
{
  return ( pu.intraDir[ CHANNEL_TYPE_CHROMA ] == DM_CHROMA_IDX ) || ( pu.cs->sps->getSpsNext().getUseMDMS() && ! PU::isLMCMode( pu.intraDir[ CHANNEL_TYPE_CHROMA ] ) );
}

int PU::getLMSymbolList(const PredictionUnit &pu, Int *pModeList)
{
  const Int iNeighbors = 5;
  const PredictionUnit* neighboringPUs[ iNeighbors ];

  const CompArea& area = pu.Cb();
  const Position posLT = area.topLeft();
  const Position posRT = area.topRight();
  const Position posLB = area.bottomLeft();

  neighboringPUs[ 0 ] = pu.cs->getPURestricted( posLB.offset(-1,  0), pu, CHANNEL_TYPE_CHROMA ); //left
  neighboringPUs[ 1 ] = pu.cs->getPURestricted( posRT.offset( 0, -1), pu, CHANNEL_TYPE_CHROMA ); //above
  neighboringPUs[ 2 ] = pu.cs->getPURestricted( posRT.offset( 1, -1), pu, CHANNEL_TYPE_CHROMA ); //aboveRight
  neighboringPUs[ 3 ] = pu.cs->getPURestricted( posLB.offset(-1,  1), pu, CHANNEL_TYPE_CHROMA ); //BelowLeft
  neighboringPUs[ 4 ] = pu.cs->getPURestricted( posLT.offset(-1, -1), pu, CHANNEL_TYPE_CHROMA ); //AboveLeft

  Int iCount = 0;
  for ( Int i = 0; i < iNeighbors; i++ )
  {
    if ( neighboringPUs[i] && CU::isIntra( *(neighboringPUs[i]->cu) ) )
    {
      Int iMode = neighboringPUs[i]->intraDir[CHANNEL_TYPE_CHROMA];
      if ( ! PU::isLMCMode( iMode ) )
      {
        iCount++;
      }
    }
  }

  Bool bNonLMInsert = false;
  Int iIdx = 0;

  pModeList[ iIdx++ ] = LM_CHROMA_IDX;

  if ( iCount >= g_aiNonLMPosThrs[0] && ! bNonLMInsert )
  {
    pModeList[ iIdx++ ] = -1;
    bNonLMInsert = true;
  }

  if ( PU::isMMLMEnabled( pu ) )
  {
    pModeList[ iIdx++ ] = MMLM_CHROMA_IDX;
  }

  if ( iCount >= g_aiNonLMPosThrs[1] && ! bNonLMInsert )
  {
    pModeList[ iIdx++ ] = -1;
    bNonLMInsert = true;
  }

  if ( PU::isMFLMEnabled( pu ) )
  {
    pModeList[ iIdx++ ] = LM_CHROMA_F1_IDX;
    pModeList[ iIdx++ ] = LM_CHROMA_F2_IDX;
    if ( iCount >= g_aiNonLMPosThrs[2] && ! bNonLMInsert )
    {
      pModeList[ iIdx++ ] = -1;
      bNonLMInsert = true;
    }
    pModeList[ iIdx++ ] = LM_CHROMA_F3_IDX;
    pModeList[ iIdx++ ] = LM_CHROMA_F4_IDX;
  }

  if ( ! bNonLMInsert )
  {
    pModeList[ iIdx++ ] = -1;
    bNonLMInsert = true;
  }

  return iIdx;
}

UInt PU::getFinalIntraMode( const PredictionUnit &pu, const ChannelType &chType )
{
  UInt uiIntraMode = pu.intraDir[chType];

  if( uiIntraMode == DM_CHROMA_IDX && !isLuma( chType ) )
  {
    const PredictionUnit &lumaPU = CS::isDoubleITree( *pu.cs ) ? *pu.cs->picture->cs->getPU( pu.blocks[chType].lumaPos(), CHANNEL_TYPE_LUMA ) : *pu.cs->getPU( pu.blocks[chType].lumaPos(), CHANNEL_TYPE_LUMA );
    uiIntraMode = lumaPU.intraDir[0];
  }
  if( pu.chromaFormat == CHROMA_422 && !isLuma( chType ) )
  {
    uiIntraMode = g_chroma422IntraAngleMappingTable[uiIntraMode];
  }
  return uiIntraMode;
}


void PU::getInterMergeCandidates( const PredictionUnit &pu, MergeCtx& mrgCtx, const int& mrgCandIdx )
{
  const CodingStructure &cs  = *pu.cs;
  const Slice &slice         = *pu.cs->slice;
  const UInt maxNumMergeCand = slice.getMaxNumMergeCand();
  const bool canFastExit     = pu.cs->pps->getLog2ParallelMergeLevelMinus2() == 0;

  Bool isCandInter[MRG_MAX_NUM_CANDS];

  for (UInt ui = 0; ui < maxNumMergeCand; ++ui)
  {
    isCandInter[ui] = false;
    mrgCtx.LICFlags          [ui] = false;
    mrgCtx.interDirNeighbours[ui] = 0;
    mrgCtx.mrgTypeNeighnours [ui] = MRG_TYPE_DEFAULT_N;
    mrgCtx.mvFieldNeighbours[(ui << 1)    ].refIdx = NOT_VALID;
    mrgCtx.mvFieldNeighbours[(ui << 1) + 1].refIdx = NOT_VALID;
  }

  mrgCtx.numValidMergeCand = maxNumMergeCand;
  // compute the location of the current PU

  Int cnt = 0;

  const Position posLT = pu.Y().topLeft();
  const Position posRT = pu.Y().topRight();
  const Position posLB = pu.Y().bottomLeft();

  MotionInfo miAbove, miLeft, miAboveLeft, miAboveRight, miBelowLeft;

  //left
  const PredictionUnit* puLeft = cs.getPURestricted( posLB.offset( -1, 0 ), pu );

  const Bool isAvailableA1 = puLeft && isDiffMER( pu, *puLeft ) && ( pu.cu != puLeft->cu || pu.cu->partSize == SIZE_NxN ) && CU::isInter( *puLeft->cu );

  if( isAvailableA1 )
  {
    miLeft = puLeft->getMotionInfo( posLB.offset(-1, 0) );

    isCandInter[cnt] = true;

    // get Inter Dir
    mrgCtx.interDirNeighbours[cnt] = miLeft.interDir;
    mrgCtx.LICFlags          [cnt] = miLeft.usesLIC;

    // get Mv from Left
    mrgCtx.mvFieldNeighbours[cnt << 1].setMvField(miLeft.mv[0], miLeft.refIdx[0]);

    if (slice.isInterB())
    {
      mrgCtx.mvFieldNeighbours[(cnt << 1) + 1].setMvField(miLeft.mv[1], miLeft.refIdx[1]);
    }

    if( mrgCandIdx == cnt && canFastExit )
    {
      return;
    }

    cnt++;
  }

  // early termination
  if (cnt == maxNumMergeCand)
  {
    return;
  }


  // above
  const PredictionUnit *puAbove = cs.getPURestricted( posRT.offset( 0, -1 ), pu );

  Bool isAvailableB1 = puAbove && isDiffMER( pu, *puAbove ) && ( pu.cu != puAbove->cu || pu.cu->partSize == SIZE_NxN ) && CU::isInter( *puAbove->cu );

  if( isAvailableB1 )
  {
    miAbove = puAbove->getMotionInfo( posRT.offset( 0, -1 ) );

    if( !isAvailableA1 || ( miAbove != miLeft ) )
    {
      isCandInter[cnt] = true;

      // get Inter Dir
      mrgCtx.interDirNeighbours[cnt] = miAbove.interDir;
      mrgCtx.LICFlags          [cnt] = miAbove.usesLIC;

      // get Mv from Left
      mrgCtx.mvFieldNeighbours[cnt << 1].setMvField( miAbove.mv[0], miAbove.refIdx[0] );

      if( slice.isInterB() )
      {
        mrgCtx.mvFieldNeighbours[( cnt << 1 ) + 1].setMvField( miAbove.mv[1], miAbove.refIdx[1] );
      }

      if( mrgCandIdx == cnt && canFastExit )
      {
        return;
      }

      cnt++;
    }
  }

  // early termination
  if( cnt == maxNumMergeCand )
  {
    return;
  }

  // above right
  const PredictionUnit *puAboveRight = cs.getPURestricted( posRT.offset( 1, -1 ), pu );

  Bool isAvailableB0 = puAboveRight && isDiffMER( pu, *puAboveRight ) && CU::isInter( *puAboveRight->cu );

  if( isAvailableB0 )
  {
    miAboveRight = puAboveRight->getMotionInfo( posRT.offset( 1, -1 ) );

    if( !isAvailableB1 || ( miAbove != miAboveRight ) )
    {
      isCandInter[cnt] = true;

      // get Inter Dir
      mrgCtx.interDirNeighbours[cnt] = miAboveRight.interDir;
      mrgCtx.LICFlags          [cnt] = miAboveRight.usesLIC;

      // get Mv from Left
      mrgCtx.mvFieldNeighbours[cnt << 1].setMvField( miAboveRight.mv[0], miAboveRight.refIdx[0] );

      if( slice.isInterB() )
      {
        mrgCtx.mvFieldNeighbours[( cnt << 1 ) + 1].setMvField( miAboveRight.mv[1], miAboveRight.refIdx[1] );
      }

      if( mrgCandIdx == cnt && canFastExit )
      {
        return;
      }

      cnt++;
    }
  }
  // early termination
  if( cnt == maxNumMergeCand )
  {
    return;
  }

  //left bottom
  const PredictionUnit *puLeftBottom = cs.getPURestricted( posLB.offset( -1, 1 ), pu );

  Bool isAvailableA0 = puLeftBottom && isDiffMER( pu, *puLeftBottom ) && CU::isInter( *puLeftBottom->cu );

  if( isAvailableA0 )
  {
    miBelowLeft = puLeftBottom->getMotionInfo( posLB.offset( -1, 1 ) );

    if( !isAvailableA1 || ( miBelowLeft != miLeft ) )
    {
      isCandInter[cnt] = true;

      // get Inter Dir
      mrgCtx.interDirNeighbours[cnt] = miBelowLeft.interDir;
      mrgCtx.LICFlags          [cnt] = miBelowLeft.usesLIC;

      // get Mv from Bottom-Left
      mrgCtx.mvFieldNeighbours[cnt << 1].setMvField( miBelowLeft.mv[0], miBelowLeft.refIdx[0] );

      if( slice.isInterB() )
      {
        mrgCtx.mvFieldNeighbours[( cnt << 1 ) + 1].setMvField( miBelowLeft.mv[1], miBelowLeft.refIdx[1] );
      }

      if( mrgCandIdx == cnt && canFastExit )
      {
        return;
      }

      cnt++;
    }
  }
  // early termination
  if( cnt == maxNumMergeCand )
  {
    return;
  }

  bool enableSubPuMvp = slice.getSPS()->getSpsNext().getUseSubPuMvp();
  bool isAvailableSubPu = false;
  unsigned subPuMvpPos = 0;

  if( enableSubPuMvp )
  {
    CHECK( mrgCtx.subPuMvpMiBuf   .area() == 0 || !mrgCtx.subPuMvpMiBuf   .buf, "Buffer not initialized" );
    CHECK( mrgCtx.subPuMvpExtMiBuf.area() == 0 || !mrgCtx.subPuMvpExtMiBuf.buf, "Buffer not initialized" );

    mrgCtx.subPuMvpMiBuf   .fill( MotionInfo() );
    mrgCtx.subPuMvpExtMiBuf.fill( MotionInfo() );
  }

  if( enableSubPuMvp && slice.getEnableTMVPFlag() )
  {
    Bool bMrgIdxMatchATMVPCan = ( mrgCandIdx == cnt );
    bool tmpLICFlag           = false;

    isAvailableSubPu = getInterMergeSubPuMvpCand( pu, mrgCtx, tmpLICFlag, cnt );

    if( isAvailableSubPu )
    {
      isCandInter[cnt] = true;

      mrgCtx.mrgTypeNeighnours[cnt] = MRG_TYPE_SUBPU_ATMVP;
      mrgCtx.LICFlags         [cnt] = tmpLICFlag;

      if( bMrgIdxMatchATMVPCan )
      {
        return;
      }
      subPuMvpPos = cnt;
      cnt++;

      if( cnt == maxNumMergeCand )
      {
        return;
      }
    }

    // don't restrict bi-pred candidates for now, to be able to eliminate obsolete candidates
    bool bAtmvpExtAva = getInterMergeSubPuRecurCand( pu, mrgCtx, cnt );

    if( bAtmvpExtAva )
    {
      const MotionInfo &miLast = mrgCtx.subPuMvpExtMiBuf.at( mrgCtx.subPuMvpExtMiBuf.width - 1, mrgCtx.subPuMvpExtMiBuf.height - 1 );

      mrgCtx.mrgTypeNeighnours[  cnt           ] = MRG_TYPE_SUBPU_ATMVP_EXT;
      mrgCtx.interDirNeighbours[ cnt           ] = miLast.interDir;
      mrgCtx.mvFieldNeighbours[( cnt << 1 )    ].setMvField( miLast.mv[0], miLast.refIdx[0] );
      mrgCtx.mvFieldNeighbours[( cnt << 1 ) + 1].setMvField( miLast.mv[1], miLast.refIdx[1] );
      mrgCtx.LICFlags         [  cnt           ] = ( slice.getUseLIC() && isAvailableSubPu ? !tmpLICFlag : false );
      isCandInter             [  cnt           ] = true;

      cnt++;

      if( cnt == maxNumMergeCand )
      {
        return;
      }
    }
  }

  // above left
  if( cnt < ( enableSubPuMvp ? 6 : 4 ) )
  {
    const PredictionUnit *puAboveLeft = cs.getPURestricted( posLT.offset( -1, -1 ), pu );

    Bool isAvailableB2 = puAboveLeft && isDiffMER( pu, *puAboveLeft ) && CU::isInter( *puAboveLeft->cu );

    if( isAvailableB2 )
    {
      miAboveLeft = puAboveLeft->getMotionInfo( posLT.offset( -1, -1 ) );

      if( ( !isAvailableA1 || ( miLeft != miAboveLeft ) ) && ( !isAvailableB1 || ( miAbove != miAboveLeft ) ) )
      {
        isCandInter[cnt] = true;

        // get Inter Dir
        mrgCtx.interDirNeighbours[cnt] = miAboveLeft.interDir;
        mrgCtx.LICFlags          [cnt] = miAboveLeft.usesLIC;

        // get Mv from Above-Left
        mrgCtx.mvFieldNeighbours[cnt << 1].setMvField( miAboveLeft.mv[0], miAboveLeft.refIdx[0] );

        if( slice.isInterB() )
        {
          mrgCtx.mvFieldNeighbours[( cnt << 1 ) + 1].setMvField( miAboveLeft.mv[1], miAboveLeft.refIdx[1] );
        }

        if( mrgCandIdx == cnt && canFastExit )
        {
          return;
        }

        cnt++;
      }
    }
  }
  // early termination
  if (cnt == maxNumMergeCand)
  {
    return;
  }

  if (slice.getEnableTMVPFlag())
  {
    //>> MTK colocated-RightBottom
    // offset the pos to be sure to "point" to the same position the uiAbsPartIdx would've pointed to
    Position posRB = pu.Y().bottomRight().offset(-3, -3);

    const PreCalcValues& pcv = *cs.pcv;

    Position posC0;
    Position posC1 = pu.Y().center();
    Bool C0Avail = false;

    if (((posRB.x + pcv.minCUWidth) < pcv.lumaWidth) && ((posRB.y + pcv.minCUHeight) < pcv.lumaHeight))
    {
      if( cs.sps->getSpsNext().getUseSubPuMvp() )
      {
        // COM16_C806_GEN_MRG_IMPROVEMENT
        posC0 = posRB.offset( 4, 4 );
        C0Avail = true;
      }
      else
      {
        Position posInCtu( posRB.x & pcv.maxCUWidthMask, posRB.y & pcv.maxCUHeightMask );

        if( ( posInCtu.x + 4 < pcv.maxCUWidth ) &&           // is not at the last column of CTU
            ( posInCtu.y + 4 < pcv.maxCUHeight ) )           // is not at the last row    of CTU
        {
          posC0 = posRB.offset( 4, 4 );
          C0Avail = true;
        }
        else if( posInCtu.x + 4 < pcv.maxCUWidth )           // is not at the last column of CTU But is last row of CTU
        {
          posC0 = posRB.offset( 4, 4 );
          // in the reference the CTU address is not set - thus probably resulting in no using this C0 possibility
        }
        else if( posInCtu.y + 4 < pcv.maxCUHeight )          // is not at the last row of CTU But is last column of CTU
        {
          posC0 = posRB.offset( 4, 4 );
          C0Avail = true;
        }
        else //is the right bottom corner of CTU
        {
          posC0 = posRB.offset( 4, 4 );
          // same as for last column but not last row
        }
      }
    }

    Mv        cColMv;
    int       iRefIdx     = 0;
    bool      colLICFlag  = false;
    bool      LICFlag     = false;
    int       dir         = 0;
    unsigned  uiArrayAddr = cnt;
    bool      bExistMV    = ( C0Avail && getColocatedMVP(pu, REF_PIC_LIST_0, posC0, cColMv, iRefIdx, &colLICFlag ) )
                                      || getColocatedMVP(pu, REF_PIC_LIST_0, posC1, cColMv, iRefIdx, &colLICFlag );

    if (bExistMV)
    {
      dir     |= 1;
      LICFlag |= colLICFlag;
      mrgCtx.mvFieldNeighbours[2 * uiArrayAddr].setMvField(cColMv, iRefIdx);
    }

    if (slice.isInterB())
    {
      bExistMV = ( C0Avail && getColocatedMVP(pu, REF_PIC_LIST_1, posC0, cColMv, iRefIdx, &colLICFlag ) )
                           || getColocatedMVP(pu, REF_PIC_LIST_1, posC1, cColMv, iRefIdx, &colLICFlag );

      if (bExistMV)
      {
        dir     |= 2;
        LICFlag |= colLICFlag;
        mrgCtx.mvFieldNeighbours[2 * uiArrayAddr + 1].setMvField(cColMv, iRefIdx);
      }
    }

    if( dir != 0 )
    {
      bool addTMvp = !( cs.sps->getSpsNext().getUseSubPuMvp() && isAvailableSubPu );
      if( !addTMvp )
      {
        if( dir != mrgCtx.interDirNeighbours[subPuMvpPos] || LICFlag != mrgCtx.LICFlags[subPuMvpPos] )
        {
          addTMvp = true;
        }
        else
        {
          for( unsigned refList = 0; refList < NUM_REF_PIC_LIST_01; refList++ )
          {
            if( dir & ( 1 << refList ) )
            {
              if( mrgCtx.mvFieldNeighbours[( cnt << 1 ) + refList] != mrgCtx.mvFieldNeighbours[(subPuMvpPos << 1) + refList] )
              {
                addTMvp = true;
                break;
              }
            }
          }
        }
      }
      if( addTMvp )
      {
        mrgCtx.interDirNeighbours[uiArrayAddr] = dir;
        mrgCtx.LICFlags          [uiArrayAddr] = LICFlag;
        isCandInter              [uiArrayAddr] = true;

        if( mrgCandIdx == cnt && canFastExit )
        {
          return;
        }

        cnt++;
      }
    }
  }

  // early termination
  if (cnt == maxNumMergeCand)
  {
    return;
  }

  UInt uiArrayAddr = cnt;
  UInt uiCutoff    = std::min( uiArrayAddr, 4u );

  if (slice.isInterB())
  {
    static const UInt NUM_PRIORITY_LIST = 12;
    static const UInt uiPriorityList0[NUM_PRIORITY_LIST] = { 0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3 };
    static const UInt uiPriorityList1[NUM_PRIORITY_LIST] = { 1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2 };

    for (Int idx = 0; idx < uiCutoff * (uiCutoff - 1) && uiArrayAddr != maxNumMergeCand; idx++)
    {
      CHECK( idx >= NUM_PRIORITY_LIST, "Invalid priority list number" );
      Int i = uiPriorityList0[idx];
      Int j = uiPriorityList1[idx];
      if (isCandInter[i] && isCandInter[j] && (mrgCtx.interDirNeighbours[i] & 0x1) && (mrgCtx.interDirNeighbours[j] & 0x2))
      {
        isCandInter[uiArrayAddr] = true;
        mrgCtx.interDirNeighbours[uiArrayAddr] = 3;
        mrgCtx.LICFlags          [uiArrayAddr] = ( mrgCtx.LICFlags[i] || mrgCtx.LICFlags[j] );

        // get Mv from cand[i] and cand[j]
        mrgCtx.mvFieldNeighbours[ uiArrayAddr << 1     ].setMvField(mrgCtx.mvFieldNeighbours[ i << 1     ].mv, mrgCtx.mvFieldNeighbours[ i << 1     ].refIdx);
        mrgCtx.mvFieldNeighbours[(uiArrayAddr << 1) + 1].setMvField(mrgCtx.mvFieldNeighbours[(j << 1) + 1].mv, mrgCtx.mvFieldNeighbours[(j << 1) + 1].refIdx);

        Int iRefPOCL0 = slice.getRefPOC(REF_PIC_LIST_0, mrgCtx.mvFieldNeighbours[(uiArrayAddr << 1)    ].refIdx);
        Int iRefPOCL1 = slice.getRefPOC(REF_PIC_LIST_1, mrgCtx.mvFieldNeighbours[(uiArrayAddr << 1) + 1].refIdx);

        if( iRefPOCL0 == iRefPOCL1 && mrgCtx.mvFieldNeighbours[( uiArrayAddr << 1 )].mv == mrgCtx.mvFieldNeighbours[( uiArrayAddr << 1 ) + 1].mv )
        {
          isCandInter[uiArrayAddr] = false;
        }
        else
        {
          uiArrayAddr++;
        }
      }
    }
  }

  // early termination
  if (uiArrayAddr == maxNumMergeCand)
  {
    return;
  }

  Int iNumRefIdx = slice.isInterB() ? std::min(slice.getNumRefIdx(REF_PIC_LIST_0), slice.getNumRefIdx(REF_PIC_LIST_1)) : slice.getNumRefIdx(REF_PIC_LIST_0);

  Int r = 0;
  Int refcnt = 0;
  while (uiArrayAddr < maxNumMergeCand)
  {
    isCandInter               [uiArrayAddr     ] = true;
    mrgCtx.interDirNeighbours [uiArrayAddr     ] = 1;
    mrgCtx.LICFlags           [uiArrayAddr     ] = false;
    mrgCtx.mvFieldNeighbours  [uiArrayAddr << 1].setMvField(Mv(0, 0), r);

    if (slice.isInterB())
    {
      mrgCtx.interDirNeighbours [ uiArrayAddr          ] = 3;
      mrgCtx.mvFieldNeighbours  [(uiArrayAddr << 1) + 1].setMvField(Mv(0, 0), r);
    }

    uiArrayAddr++;

    if (refcnt == iNumRefIdx - 1)
    {
      r = 0;
    }
    else
    {
      ++r;
      ++refcnt;
    }
  }
  mrgCtx.numValidMergeCand = uiArrayAddr;
}


static int xGetDistScaleFactor(const int &iCurrPOC, const int &iCurrRefPOC, const int &iColPOC, const int &iColRefPOC)
{
  int iDiffPocD = iColPOC - iColRefPOC;
  int iDiffPocB = iCurrPOC - iCurrRefPOC;

  if (iDiffPocD == iDiffPocB)
  {
    return 4096;
  }
  else
  {
    int iTDB = Clip3(-128, 127, iDiffPocB);
    int iTDD = Clip3(-128, 127, iDiffPocD);
    int iX = (0x4000 + abs(iTDD / 2)) / iTDD;
    int iScale = Clip3(-4096, 4095, (iTDB * iX + 32) >> 6);
    return iScale;
  }
}

bool PU::getColocatedMVP(const PredictionUnit &pu, const RefPicList &eRefPicList, const Position &_pos, Mv& rcMv, const int &refIdx, bool* LICFlag /*=0*/ )
{
  // don't perform MV compression when generally disabled or subPuMvp is used
  const unsigned scale = ( pu.cs->pcv->noMotComp ? 1 : 4 * std::max<Int>(1, 4 * AMVP_DECIMATION_FACTOR / 4) );
  const unsigned mask  = ~( scale - 1 );

  const Position pos = Position{ PosType( _pos.x & mask ), PosType( _pos.y & mask ) };

  const Slice &slice = *pu.cs->slice;

  // use coldir.
  const Picture* const pColPic = slice.getRefPic(RefPicList(slice.isInterB() ? 1 - slice.getColFromL0Flag() : 0), slice.getColRefIdx());

  if( !pColPic )
  {
    return false;
  }

  RefPicList eColRefPicList = slice.getCheckLDC() ? eRefPicList : RefPicList(slice.getColFromL0Flag());

  const MotionInfo& mi = pColPic->cs->getMotionInfo( pos );

  if( !mi.isInter )
  {
    return false;
  }

  int iColRefIdx = mi.refIdx[eColRefPicList];

  if (iColRefIdx < 0)
  {
    eColRefPicList = RefPicList(1 - eColRefPicList);
    iColRefIdx = mi.refIdx[eColRefPicList];

    if (iColRefIdx < 0)
    {
      return false;
    }
  }

  const Slice *pColSlice = nullptr;

  for( const auto s : pColPic->slices )
  {
    if( s->getIndependentSliceIdx() == mi.sliceIdx )
    {
      pColSlice = s;
      break;
    }
  }

  CHECK( pColSlice == nullptr, "Slice segment not found" );

  const Slice &colSlice = *pColSlice;

  const bool bIsCurrRefLongTerm = slice.getRefPic(eRefPicList, refIdx)->longTerm;
  const bool bIsColRefLongTerm  = colSlice.getIsUsedAsLongTerm(eColRefPicList, iColRefIdx);

  if (bIsCurrRefLongTerm != bIsColRefLongTerm)
  {
    return false;
  }

  if( LICFlag )
  {
    *LICFlag = mi.usesLIC;
  }

  // Scale the vector.
  Mv cColMv = mi.mv[eColRefPicList];

  if (bIsCurrRefLongTerm /*|| bIsColRefLongTerm*/)
  {
    rcMv = cColMv;
  }
  else
  {
    const Int currPOC    = slice.getPOC();
    const Int colPOC     = colSlice.getPOC();
    const Int colRefPOC  = colSlice.getRefPOC(eColRefPicList, iColRefIdx);
    const Int currRefPOC = slice.getRefPic(eRefPicList, refIdx)->getPOC();
    const Int distscale  = xGetDistScaleFactor(currPOC, currRefPOC, colPOC, colRefPOC);

    if (distscale == 4096)
    {
      rcMv = cColMv;
    }
    else
    {
      if( pu.cs->sps->getSpsNext().getUseHighPrecMv() )
      {
        // allow extended precision for temporal scaling
        cColMv.setHighPrec();
      }

      rcMv = cColMv.scaleMv(distscale);
    }
  }

  return true;
}

bool PU::isDiffMER(const PredictionUnit &pu1, const PredictionUnit &pu2)
{
  const unsigned xN = pu1.lumaPos().x;
  const unsigned yN = pu1.lumaPos().y;
  const unsigned xP = pu2.lumaPos().x;
  const unsigned yP = pu2.lumaPos().y;

  unsigned plevel = pu1.cs->pps->getLog2ParallelMergeLevelMinus2() + 2;

  if ((xN >> plevel) != (xP >> plevel))
  {
    return true;
  }

  if ((yN >> plevel) != (yP >> plevel))
  {
    return true;
  }

  return false;
}

/** Constructs a list of candidates for AMVP (See specification, section "Derivation process for motion vector predictor candidates")
* \param uiPartIdx
* \param uiPartAddr
* \param eRefPicList
* \param iRefIdx
* \param pInfo
*/
void PU::fillMvpCand(PredictionUnit &pu, const RefPicList &eRefPicList, const int &refIdx, AMVPInfo &amvpInfo, InterPrediction *interPred)
{
  CodingStructure &cs = *pu.cs;

  AMVPInfo *pInfo = &amvpInfo;

  pInfo->numCand = 0;

  if (refIdx < 0)
  {
    return;
  }

  //-- Get Spatial MV
  Position posLT = pu.Y().topLeft();
  Position posRT = pu.Y().topRight();
  Position posLB = pu.Y().bottomLeft();

  Bool isScaledFlagLX = false; /// variable name from specification; true when the PUs below left or left are available (availableA0 || availableA1).

  {
    const PredictionUnit* tmpPU = cs.getPURestricted( posLB.offset( -1, 1 ), pu ); // getPUBelowLeft(idx, partIdxLB);
    isScaledFlagLX = tmpPU != NULL && CU::isInter( *tmpPU->cu );

    if( !isScaledFlagLX )
    {
      tmpPU = cs.getPURestricted( posLB.offset( -1, 0 ), pu );
      isScaledFlagLX = tmpPU != NULL && CU::isInter( *tmpPU->cu );
    }
  }

  // Left predictor search
  if( isScaledFlagLX )
  {
    Bool bAdded = addMVPCandUnscaled( pu, eRefPicList, refIdx, posLB, MD_BELOW_LEFT, *pInfo );

    if( !bAdded )
    {
      bAdded = addMVPCandUnscaled( pu, eRefPicList, refIdx, posLB, MD_LEFT, *pInfo );

      if( !bAdded )
      {
        bAdded = addMVPCandWithScaling( pu, eRefPicList, refIdx, posLB, MD_BELOW_LEFT, *pInfo );

        if( !bAdded )
        {
          addMVPCandWithScaling( pu, eRefPicList, refIdx, posLB, MD_LEFT, *pInfo );
        }
      }
    }
  }

  // Above predictor search
  {
    Bool bAdded = addMVPCandUnscaled( pu, eRefPicList, refIdx, posRT, MD_ABOVE_RIGHT, *pInfo );

    if( !bAdded )
    {
      bAdded = addMVPCandUnscaled( pu, eRefPicList, refIdx, posRT, MD_ABOVE, *pInfo );

      if( !bAdded )
      {
        addMVPCandUnscaled( pu, eRefPicList, refIdx, posLT, MD_ABOVE_LEFT, *pInfo );
      }
    }
  }

  if( !isScaledFlagLX )
  {
    Bool bAdded = addMVPCandWithScaling( pu, eRefPicList, refIdx, posRT, MD_ABOVE_RIGHT, *pInfo );

    if( !bAdded )
    {
      bAdded = addMVPCandWithScaling( pu, eRefPicList, refIdx, posRT, MD_ABOVE, *pInfo );

      if( !bAdded )
      {
        addMVPCandWithScaling( pu, eRefPicList, refIdx, posLT, MD_ABOVE_LEFT, *pInfo );
      }
    }
  }

  if( pu.cu->imv != 0)
  {
    unsigned imvShift = pu.cu->imv << 1;
    for( Int i = 0; i < pInfo->numCand; i++ )
    {
      roundMV( pInfo->mvCand[i], imvShift );
    }
  }

  if( pInfo->numCand == 2 )
  {
    if( pInfo->mvCand[0] == pInfo->mvCand[1] )
    {
      pInfo->numCand = 1;
    }
  }

  if( cs.slice->getEnableTMVPFlag() )
  {
    // Get Temporal Motion Predictor
    const int refIdx_Col = refIdx;

    Position posRB = pu.Y().bottomRight().offset(-3, -3);

    const PreCalcValues& pcv = *cs.pcv;

    Position posC0;
    bool C0Avail = false;
    Position posC1 = pu.Y().center();

    Mv cColMv;

    if( ( ( posRB.x + pcv.minCUWidth ) < pcv.lumaWidth ) && ( ( posRB.y + pcv.minCUHeight ) < pcv.lumaHeight ) )
    {
      Position posInCtu( posRB.x & pcv.maxCUWidthMask, posRB.y & pcv.maxCUHeightMask );

      if ((posInCtu.x + 4 < pcv.maxCUWidth) &&           // is not at the last column of CTU
          (posInCtu.y + 4 < pcv.maxCUHeight))             // is not at the last row    of CTU
      {
        posC0 = posRB.offset(4, 4);
        C0Avail = true;
      }
      else if (posInCtu.x + 4 < pcv.maxCUWidth)           // is not at the last column of CTU But is last row of CTU
      {
        // in the reference the CTU address is not set - thus probably resulting in no using this C0 possibility
        posC0 = posRB.offset(4, 4);
      }
      else if (posInCtu.y + 4 < pcv.maxCUHeight)          // is not at the last row of CTU But is last column of CTU
      {
        posC0 = posRB.offset(4, 4);
        C0Avail = true;
      }
      else //is the right bottom corner of CTU
      {
        // same as for last column but not last row
        posC0 = posRB.offset(4, 4);
      }
    }

    if ((C0Avail && getColocatedMVP(pu, eRefPicList, posC0, cColMv, refIdx_Col)) || getColocatedMVP(pu, eRefPicList, posC1, cColMv, refIdx_Col))
    {
      pInfo->mvCand[pInfo->numCand++] = cColMv;
    }
  }

  if( cs.slice->getSPS()->getSpsNext().getUseFRUCMrgMode() )
  {
    if( interPred != NULL && interPred->frucFindBlkMv4Pred( pu, eRefPicList, refIdx, pInfo ) )
    {
      const Mv &mv = pu.mv[eRefPicList];
      if( pInfo->numCand == 0 )
      {
        pInfo->mvCand[0] = mv;
        pInfo->numCand++;
      }
      else if( pInfo->mvCand[0] != mv )
      {
        for( Int n = std::min( (int)pInfo->numCand, AMVP_MAX_NUM_CANDS - 1 ); n > 0; n-- )
        {
          pInfo->mvCand[n] = pInfo->mvCand[n-1];
        }
        pInfo->mvCand[0] = mv;
        pInfo->numCand = std::min( (int)pInfo->numCand + 1, AMVP_MAX_NUM_CANDS );
      }
    }
  }

  if (pInfo->numCand > AMVP_MAX_NUM_CANDS)
  {
    pInfo->numCand = AMVP_MAX_NUM_CANDS;
  }

  while (pInfo->numCand < AMVP_MAX_NUM_CANDS)
  {
    const Bool prec = pInfo->mvCand[pInfo->numCand].highPrec;
    pInfo->mvCand[pInfo->numCand] = Mv( 0, 0, prec );
    pInfo->numCand++;
  }

  if( pu.cs->sps->getSpsNext().getUseHighPrecMv() )
  {
    for( Mv &mv : pInfo->mvCand )
    {
      if( mv.highPrec ) mv.setLowPrec();
    }
  }

  if (pu.cu->imv != 0)
  {
    unsigned imvShift = pu.cu->imv << 1;
    for (Int i = 0; i < pInfo->numCand; i++)
    {
      roundMV(pInfo->mvCand[i], imvShift);
    }
  }
}

Bool isValidAffineCandidate( const PredictionUnit &pu, Mv cMv0, Mv cMv1, Mv cMv2, Int& riDV )
{
  Mv zeroMv(0, 0);
  Mv deltaHor = cMv1 - cMv0;
  Mv deltaVer = cMv2 - cMv0;

  // same motion vector, translation model
  if ( deltaHor == zeroMv )
    return false;

  deltaHor.setHighPrec();
  deltaVer.setHighPrec();

  // S/8, but the Mv is 4 precision, so change to S/2
  Int width = pu.Y().width;
  Int height = pu.Y().height;
  Int iDiffHor = width>>1;
  Int iDiffVer = height>>1;

  if ( deltaHor.getAbsHor() > iDiffHor || deltaHor.getAbsVer() > iDiffVer || deltaVer.getAbsHor() > iDiffHor || deltaVer.getAbsVer() > iDiffVer )
  {
    return false;
  }
  // Calculate DV
  riDV = abs( deltaHor.getHor() * height - deltaVer.getVer() * width ) + abs( deltaHor.getVer() * height + deltaVer.getHor() * width );
  return true;
}

void PU::fillAffineMvpCand(PredictionUnit &pu, const RefPicList &eRefPicList, const int &refIdx, AffineAMVPInfo &affiAMVPInfo)
{
  affiAMVPInfo.numCand = 0;

  if (refIdx < 0)
  {
    return;
  }

  //-- Get Spatial MV
  Position posLT = pu.Y().topLeft();
  Position posRT = pu.Y().topRight();
  Position posLB = pu.Y().bottomLeft();


  //-------------------  V0 (START) -------------------//
  AMVPInfo amvpInfo0;
  amvpInfo0.numCand = 0;

  // A->C: Above Left, Above, Left
  addMVPCandUnscaled( pu, eRefPicList, refIdx, posLT, MD_ABOVE_LEFT, amvpInfo0, true );
  addMVPCandUnscaled( pu, eRefPicList, refIdx, posLT, MD_ABOVE,      amvpInfo0, true );
  addMVPCandUnscaled( pu, eRefPicList, refIdx, posLT, MD_LEFT,       amvpInfo0, true );

  if( amvpInfo0.numCand < AFFINE_MAX_NUM_V0 )
  {
    addMVPCandWithScaling( pu, eRefPicList, refIdx, posLT, MD_ABOVE_LEFT, amvpInfo0, true );
    if ( amvpInfo0.numCand < AFFINE_MAX_NUM_V0 )
    {
      addMVPCandWithScaling( pu, eRefPicList, refIdx, posLT, MD_ABOVE, amvpInfo0, true );
      if ( amvpInfo0.numCand < AFFINE_MAX_NUM_V0 )
      {
        addMVPCandWithScaling( pu, eRefPicList, refIdx, posLT, MD_LEFT, amvpInfo0, true );
      }
    }
  }

  //-------------------  V1 (START) -------------------//
  AMVPInfo amvpInfo1;
  amvpInfo1.numCand = 0;

  // D->E: Above, Above Right
  addMVPCandUnscaled( pu, eRefPicList, refIdx, posRT, MD_ABOVE,       amvpInfo1, true );
  addMVPCandUnscaled( pu, eRefPicList, refIdx, posRT, MD_ABOVE_RIGHT, amvpInfo1, true );
  if( amvpInfo1.numCand < AFFINE_MAX_NUM_V1 )
  {
    addMVPCandWithScaling( pu, eRefPicList, refIdx, posRT, MD_ABOVE, amvpInfo1, true );
    if( amvpInfo1.numCand < AFFINE_MAX_NUM_V1 )
    {
      addMVPCandWithScaling( pu, eRefPicList, refIdx, posRT, MD_ABOVE_RIGHT, amvpInfo1, true );
    }
  }

  //-------------------  V2 (START) -------------------//
  AMVPInfo amvpInfo2;
  amvpInfo2.numCand = 0;

  // F->G: Left, Below Left
  addMVPCandUnscaled( pu, eRefPicList, refIdx, posLB, MD_LEFT,       amvpInfo2, true );
  addMVPCandUnscaled( pu, eRefPicList, refIdx, posLB, MD_BELOW_LEFT, amvpInfo2, true );
  if( amvpInfo2.numCand < AFFINE_MAX_NUM_V2 )
  {
    addMVPCandWithScaling( pu, eRefPicList, refIdx, posLB, MD_LEFT, amvpInfo2, true );
    if( amvpInfo2.numCand < AFFINE_MAX_NUM_V2 )
    {
      addMVPCandWithScaling( pu, eRefPicList, refIdx, posLB, MD_BELOW_LEFT, amvpInfo2, true );
    }
  }

  for ( Int i = 0; i < amvpInfo0.numCand; i++ )
  {
    amvpInfo0.mvCand[i].setHighPrec();
  }
  for ( Int i = 0; i < amvpInfo1.numCand; i++ )
  {
    amvpInfo1.mvCand[i].setHighPrec();
  }
  for ( Int i = 0; i < amvpInfo2.numCand; i++ )
  {
    amvpInfo2.mvCand[i].setHighPrec();
  }

  // Check Valid Candidates and Sort through DV
  Int   iRecord[AFFINE_MAX_NUM_COMB][3];
  Int   iDV[AFFINE_MAX_NUM_COMB];
  Int   iTempDV;
  Int   iCount = 0;
  for ( Int i = 0; i < amvpInfo0.numCand; i++ )
  {
    for ( Int j = 0; j < amvpInfo1.numCand; j++ )
    {
      for ( Int k = 0; k < amvpInfo2.numCand; k++ )
      {
        Bool bValid = isValidAffineCandidate( pu, amvpInfo0.mvCand[i], amvpInfo1.mvCand[j], amvpInfo2.mvCand[k], iDV[iCount] );
        if ( bValid )
        {
          // Sort
          if ( iCount==0 || iDV[iCount]>=iDV[iCount-1] )
          {
            iRecord[iCount][0] = i;
            iRecord[iCount][1] = j;
            iRecord[iCount][2] = k;
          }
          else
          {
            // save last element
            iTempDV = iDV[iCount];
            // find position and move back record
            Int m = 0;
            for ( m = iCount - 1; m >= 0 && iTempDV < iDV[m]; m-- )
            {
              iDV[m+1] = iDV[m];
              memcpy( iRecord[m+1], iRecord[m], sizeof(Int) * 3 );
            }
            // insert
            iDV[m+1] = iTempDV;
            iRecord[m+1][0] = i;
            iRecord[m+1][1] = j;
            iRecord[m+1][2] = k;
          }
          iCount++;
        }
      }
    }
  }

  affiAMVPInfo.numCand = std::min<int>(iCount, AMVP_MAX_NUM_CANDS);

  Int iWidth = pu.Y().width;
  Int iHeight = pu.Y().height;

  for ( Int i = 0; i < affiAMVPInfo.numCand; i++ )
  {
    affiAMVPInfo.mvCandLT[i] = amvpInfo0.mvCand[ iRecord[i][0] ];
    affiAMVPInfo.mvCandRT[i] = amvpInfo1.mvCand[ iRecord[i][1] ];
    affiAMVPInfo.mvCandLB[i] = amvpInfo2.mvCand[ iRecord[i][2] ];

    affiAMVPInfo.mvCandLT[i].roundMV2SignalPrecision();
    affiAMVPInfo.mvCandRT[i].roundMV2SignalPrecision();

    clipMv( affiAMVPInfo.mvCandLT[i], pu.cu->lumaPos(), *pu.cs->sps );
    clipMv( affiAMVPInfo.mvCandRT[i], pu.cu->lumaPos(), *pu.cs->sps );

    Int vx2 =  - ( affiAMVPInfo.mvCandRT[i].getVer() - affiAMVPInfo.mvCandLT[i].getVer() ) * iHeight / iWidth + affiAMVPInfo.mvCandLT[i].getHor();
    Int vy2 =    ( affiAMVPInfo.mvCandRT[i].getHor() - affiAMVPInfo.mvCandLT[i].getHor() ) * iHeight / iWidth + affiAMVPInfo.mvCandLT[i].getVer();

    affiAMVPInfo.mvCandLB[i] = Mv( vx2, vy2, true );
    if( !pu.cu->cs->pcv->only2Nx2N )
    {
      affiAMVPInfo.mvCandLB[i].roundMV2SignalPrecision();
    }

    clipMv( affiAMVPInfo.mvCandLB[i], pu.cu->lumaPos(), *pu.cs->sps );
  }

  if ( affiAMVPInfo.numCand < 2 )
  {
    AMVPInfo amvpInfo;
    PU::fillMvpCand( pu, eRefPicList, refIdx, amvpInfo );

    Int iAdd = amvpInfo.numCand - affiAMVPInfo.numCand;
    for ( Int i = 0; i < iAdd; i++ )
    {
      amvpInfo.mvCand[i].setHighPrec();
      clipMv( amvpInfo.mvCand[i], pu.cu->lumaPos(), *pu.cs->sps );
      affiAMVPInfo.mvCandLT[affiAMVPInfo.numCand] = amvpInfo.mvCand[i];
      affiAMVPInfo.mvCandRT[affiAMVPInfo.numCand] = amvpInfo.mvCand[i];
      affiAMVPInfo.mvCandLB[affiAMVPInfo.numCand] = amvpInfo.mvCand[i];
      affiAMVPInfo.numCand++;
    }
  }
}

bool PU::addMVPCandUnscaled( const PredictionUnit &pu, const RefPicList &eRefPicList, const int &iRefIdx, const Position &pos, const MvpDir &eDir, AMVPInfo &info, bool affine )
{
        CodingStructure &cs    = *pu.cs;
  const PredictionUnit *neibPU = NULL;
        Position neibPos;

  switch (eDir)
  {
  case MD_LEFT:
    neibPos = pos.offset( -1,  0 );
    break;
  case MD_ABOVE:
    neibPos = pos.offset(  0, -1 );
    break;
  case MD_ABOVE_RIGHT:
    neibPos = pos.offset(  1, -1 );
    break;
  case MD_BELOW_LEFT:
    neibPos = pos.offset( -1,  1 );
    break;
  case MD_ABOVE_LEFT:
    neibPos = pos.offset( -1, -1 );
    break;
  default:
    break;
  }

  neibPU = cs.getPURestricted( neibPos, pu );

  if( neibPU == NULL || !CU::isInter( *neibPU->cu ) )
  {
    return false;
  }

  const MotionInfo& neibMi        = neibPU->getMotionInfo( neibPos );

  const Int        currRefPOC     = cs.slice->getRefPic( eRefPicList, iRefIdx )->getPOC();
  const RefPicList eRefPicList2nd = ( eRefPicList == REF_PIC_LIST_0 ) ? REF_PIC_LIST_1 : REF_PIC_LIST_0;

  for( Int predictorSource = 0; predictorSource < 2; predictorSource++ ) // examine the indicated reference picture list, then if not available, examine the other list.
  {
    const RefPicList eRefPicListIndex = ( predictorSource == 0 ) ? eRefPicList : eRefPicList2nd;
    const Int        neibRefIdx       = neibMi.refIdx[eRefPicListIndex];

    if( neibRefIdx >= 0 && currRefPOC == cs.slice->getRefPOC( eRefPicListIndex, neibRefIdx ) )
    {
      if( affine )
      {
        Int i = 0;
        for( i = 0; i < info.numCand; i++ )
        {
          if( info.mvCand[i] == neibMi.mv[eRefPicListIndex] )
          {
            break;
          }
        }
        if( i == info.numCand )
        {
          info.mvCand[info.numCand++] = neibMi.mv[eRefPicListIndex];
          Mv cMvHigh = neibMi.mv[eRefPicListIndex];
          cMvHigh.setHighPrec();
//          CHECK( !neibMi.mv[eRefPicListIndex].highPrec, "Unexpected low precision mv.");
          return true;
        }
      }
      else
      {
        info.mvCand[info.numCand++] = neibMi.mv[eRefPicListIndex];
        return true;
      }
    }
  }

  return false;
}

/**
* \param pInfo
* \param eRefPicList
* \param iRefIdx
* \param uiPartUnitIdx
* \param eDir
* \returns Bool
*/
bool PU::addMVPCandWithScaling( const PredictionUnit &pu, const RefPicList &eRefPicList, const int &iRefIdx, const Position &pos, const MvpDir &eDir, AMVPInfo &info, bool affine )
{
        CodingStructure &cs    = *pu.cs;
  const Slice &slice           = *cs.slice;
  const PredictionUnit *neibPU = NULL;
        Position neibPos;

  switch( eDir )
  {
  case MD_LEFT:
    neibPos = pos.offset( -1,  0 );
    break;
  case MD_ABOVE:
    neibPos = pos.offset(  0, -1 );
    break;
  case MD_ABOVE_RIGHT:
    neibPos = pos.offset(  1, -1 );
    break;
  case MD_BELOW_LEFT:
    neibPos = pos.offset( -1,  1 );
    break;
  case MD_ABOVE_LEFT:
    neibPos = pos.offset( -1, -1 );
    break;
  default:
    break;
  }

  neibPU = cs.getPURestricted( neibPos, pu );

  if( neibPU == NULL || !CU::isInter( *neibPU->cu ) )
  {
    return false;
  }

  const MotionInfo& neibMi        = neibPU->getMotionInfo( neibPos );

  const RefPicList eRefPicList2nd = ( eRefPicList == REF_PIC_LIST_0 ) ? REF_PIC_LIST_1 : REF_PIC_LIST_0;

  const int  currPOC            = slice.getPOC();
  const int  currRefPOC         = slice.getRefPic( eRefPicList, iRefIdx )->poc;
  const bool bIsCurrRefLongTerm = slice.getRefPic( eRefPicList, iRefIdx )->longTerm;
  const int  neibPOC            = currPOC;

  for( int predictorSource = 0; predictorSource < 2; predictorSource++ ) // examine the indicated reference picture list, then if not available, examine the other list.
  {
    const RefPicList eRefPicListIndex = (predictorSource == 0) ? eRefPicList : eRefPicList2nd;
    const int        neibRefIdx       = neibMi.refIdx[eRefPicListIndex];
    if( neibRefIdx >= 0 )
    {
      const bool bIsNeibRefLongTerm = slice.getRefPic(eRefPicListIndex, neibRefIdx)->longTerm;

      if (bIsCurrRefLongTerm == bIsNeibRefLongTerm)
      {
        Mv cMv = neibMi.mv[eRefPicListIndex];

        if( !( bIsCurrRefLongTerm /* || bIsNeibRefLongTerm*/) )
        {
          const int neibRefPOC = slice.getRefPOC( eRefPicListIndex, neibRefIdx );
          const int scale      = xGetDistScaleFactor( currPOC, currRefPOC, neibPOC, neibRefPOC );

          if( scale != 4096 )
          {
            if( slice.getSPS()->getSpsNext().getUseHighPrecMv() )
            {
              cMv.setHighPrec();
            }
            cMv = cMv.scaleMv( scale );
          }
        }

        if( affine )
        {
          Int i;
          for( i = 0; i < info.numCand; i++ )
          {
            if( info.mvCand[i] == cMv )
            {
              break;
            }
          }
          if( i == info.numCand )
          {
            info.mvCand[info.numCand++] = cMv;
//            CHECK( !cMv.highPrec, "Unexpected low precision mv.");
            return true;
          }
        }
        else
        {
          info.mvCand[info.numCand++] = cMv;
          return true;
        }
      }
    }
  }
  return false;
}

bool PU::isBipredRestriction(const PredictionUnit &pu)
{
  const SPSNext &spsNext = pu.cs->sps->getSpsNext();
  if( !pu.cs->pcv->only2Nx2N && !spsNext.getUseSubPuMvp() && pu.cu->lumaSize().width == 8 && ( pu.lumaSize().width < 8 || pu.lumaSize().height < 8 ) )
  {
    return true;
  }
  return false;
}

static bool deriveScaledMotionTemporal( const Slice&      slice,
                                        const Position&   colPos,
                                        const Picture*    pColPic,
                                        const RefPicList  eCurrRefPicList,
                                              Mv&         cColMv,
                                              bool&       LICFlag,
                                        const RefPicList  eFetchRefPicList )
{
  const MotionInfo &mi    = pColPic->cs->getMotionInfo( colPos );
  const Slice *pColSlice  = nullptr;

  for( const auto &pSlice : pColPic->slices )
  {
    if( pSlice->getIndependentSliceIdx() == mi.sliceIdx )
    {
      pColSlice = pSlice;
      break;
    }
  }

  CHECK( pColSlice == nullptr, "Couldn't find the colocated slice" );

  int iColPOC, iColRefPOC, iCurrPOC, iCurrRefPOC, iScale;
  bool bAllowMirrorMV = true;
  RefPicList eColRefPicList = slice.getCheckLDC() ? eCurrRefPicList : RefPicList( 1 - eFetchRefPicList );
  if( pColPic == slice.getRefPic( RefPicList( slice.isInterB() ? 1 - slice.getColFromL0Flag() : 0 ), slice.getColRefIdx() ) )
  {
    eColRefPicList = eCurrRefPicList;   //67 -> disable, 64 -> enable
    bAllowMirrorMV = false;
  }

  if( slice.getSPS()->getSpsNext().getUseFRUCMrgMode() )
  {
    eColRefPicList = eCurrRefPicList;
  }

  // Although it might make sense to keep the unavailable motion field per direction still be unavailable, I made the MV prediction the same way as in TMVP
  // So there is an interaction between MV0 and MV1 of the corresponding blocks identified by TV.

  // Grab motion and do necessary scaling.{{
  iCurrPOC = slice.getPOC();

  int iColRefIdx = mi.refIdx[eColRefPicList];

  if( iColRefIdx < 0 && ( slice.getCheckLDC() || bAllowMirrorMV ) && !slice.getSPS()->getSpsNext().getUseFRUCMrgMode() )
  {
    eColRefPicList = RefPicList( 1 - eColRefPicList );
    iColRefIdx = mi.refIdx[eColRefPicList];

    if( iColRefIdx < 0 )
    {
      return false;
    }
  }

  if( iColRefIdx >= 0 && slice.getNumRefIdx( eCurrRefPicList ) > 0 )
  {
    iColPOC    = pColSlice->getPOC();
    iColRefPOC = pColSlice->getRefPOC( eColRefPicList, iColRefIdx );
    ///////////////////////////////////////////////////////////////
    // Set the target reference index to 0, may be changed later //
    ///////////////////////////////////////////////////////////////
    iCurrRefPOC = slice.getRefPic( eCurrRefPicList, 0 )->getPOC();
    // Scale the vector.
    cColMv      = mi.mv[eColRefPicList];
    //pcMvFieldSP[2*iPartition + eCurrRefPicList].getMv();
    // Assume always short-term for now
    iScale      = xGetDistScaleFactor( iCurrPOC, iCurrRefPOC, iColPOC, iColRefPOC );

    if( iScale != 4096 )
    {
      if( slice.getSPS()->getSpsNext().getUseHighPrecMv() )
      {
        cColMv.setHighPrec();
      }

      cColMv    = cColMv.scaleMv( iScale );
    }

    LICFlag = mi.usesLIC;

    return true;
  }
  return false;
}

bool PU::getInterMergeSubPuMvpCand( const PredictionUnit &pu, MergeCtx& mrgCtx, bool& LICFlag, const int count )
{
  const Slice   &slice   = *pu.cs->slice;
  const SPSNext &spsNext =  pu.cs->sps->getSpsNext();

  const Picture *pColPic = slice.getRefPic( RefPicList( slice.isInterB() ? 1 - slice.getColFromL0Flag() : 0 ), slice.getColRefIdx() );
  int iPocColPic         = pColPic->getPOC();
  Mv cTMv;

  RefPicList eFetchRefPicList = RefPicList( slice.isInterB() ? 1 - slice.getColFromL0Flag() : 0 );
  if( count )
  {
    unsigned uiN = 0;
    for( unsigned uiCurrRefListId = 0; uiCurrRefListId < ( slice.getSliceType() == B_SLICE ? 2 : 1 ); uiCurrRefListId++ )
    {
      RefPicList  eCurrRefPicList = RefPicList( RefPicList( slice.isInterB() ? ( slice.getColFromL0Flag() ? uiCurrRefListId : 1 - uiCurrRefListId ) : uiCurrRefListId ) );
      if( mrgCtx.interDirNeighbours[uiN] & ( 1 << eCurrRefPicList ) )
      {
        pColPic           = slice.getRefPic( eCurrRefPicList, mrgCtx.mvFieldNeighbours[uiN * 2 + eCurrRefPicList].refIdx );
        iPocColPic        = pColPic->poc;
        cTMv              = mrgCtx.mvFieldNeighbours[uiN * 2 + eCurrRefPicList].mv;
        eFetchRefPicList  = eCurrRefPicList;
        break;
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////
  ////////          GET Initial Temporal Vector                  ////////
  ///////////////////////////////////////////////////////////////////////
  int mvPrec = 2;
  if( pu.cs->sps->getSpsNext().getUseHighPrecMv() )
  {
    cTMv.setHighPrec();
    mvPrec += VCEG_AZ07_MV_ADD_PRECISION_BIT_FOR_STORE;
  }
  int mvRndOffs = ( 1 << mvPrec ) >> 1;

  Mv cTempVector    = cTMv;
  bool  tempLICFlag = false;

  // compute the location of the current PU
  Position puPos    = pu.lumaPos();
  Size puSize       = pu.lumaSize();
  int iNumPartLine  = std::max( puSize.width  >> spsNext.getSubPuMvpLog2Size(), 1u );
  int iNumPartCol   = std::max( puSize.height >> spsNext.getSubPuMvpLog2Size(), 1u );
  int iPUHeight     = iNumPartCol  == 1 ? puSize.height : 1 << spsNext.getSubPuMvpLog2Size();
  int iPUWidth      = iNumPartLine == 1 ? puSize.width  : 1 << spsNext.getSubPuMvpLog2Size();

  Mv cColMv;
  // use coldir.
  bool     bBSlice  = slice.isInterB();
  unsigned bColL0   = slice.getColFromL0Flag();

  Position centerPos;

  bool found = false;
  bool bInit = false;
  for( unsigned uiLX = 0; uiLX < ( bBSlice ? 2 : 1 ) && !found; uiLX++ )
  {
    RefPicList eListY = RefPicList( bBSlice ? ( bColL0 ? uiLX : 1 - uiLX ) : uiLX );

    for( int refIdxY = ( bInit ? 0 : -1 ); refIdxY < slice.getNumRefIdx( eListY ) && !found; refIdxY++ )
    {
      if( !bInit )
      {
        bInit = true;
      }
      else
      {
        pColPic          = slice.getRefPic( eListY, refIdxY );
        eFetchRefPicList = eListY;
      }
      int iNewColPicPOC = pColPic->getPOC();
      if( iNewColPicPOC != iPocColPic )
      {
        //////////////// POC based scaling of the temporal vector /////////////
        int iScale = xGetDistScaleFactor( slice.getPOC(), iNewColPicPOC, slice.getPOC(), iPocColPic );
        if( iScale != 4096 )
        {
          cTempVector = cTMv.scaleMv( iScale );
        }
      }
      else
      {
        cTempVector = cTMv;
      }

      if( puSize.width == iPUWidth && puSize.height == iPUHeight )
      {
        centerPos.x = puPos.x + ( puSize.width  >> 1 ) + ( ( cTempVector.getHor() + mvRndOffs ) >> mvPrec );
        centerPos.y = puPos.y + ( puSize.height >> 1 ) + ( ( cTempVector.getVer() + mvRndOffs ) >> mvPrec );
      }
      else
      {
        centerPos.x = puPos.x + ( ( puSize.width  / iPUWidth  ) >> 1 ) * iPUWidth  + ( iPUWidth  >> 1 ) + ( ( cTempVector.getHor() + mvRndOffs ) >> mvPrec );
        centerPos.y = puPos.y + ( ( puSize.height / iPUHeight ) >> 1 ) * iPUHeight + ( iPUHeight >> 1 ) + ( ( cTempVector.getVer() + mvRndOffs ) >> mvPrec );
      }

      centerPos.x = Clip3( 0, ( int ) pColPic->lwidth()  - 1, centerPos.x );
      centerPos.y = Clip3( 0, ( int ) pColPic->lheight() - 1, centerPos.y );

      // derivation of center motion parameters from the collocated CU
      const MotionInfo &mi = pColPic->cs->getMotionInfo( centerPos );

      if( mi.isInter )
      {
        for( UInt uiCurrRefListId = 0; uiCurrRefListId < ( bBSlice ? 2 : 1 ); uiCurrRefListId++ )
        {
          RefPicList  eCurrRefPicList = RefPicList( uiCurrRefListId );

          if( deriveScaledMotionTemporal( slice, centerPos, pColPic, eCurrRefPicList, cColMv, tempLICFlag, eFetchRefPicList ) )
          {
            // set as default, for further motion vector field spanning
            mrgCtx.mvFieldNeighbours[( count << 1 ) + uiCurrRefListId].setMvField( cColMv, 0 );
            mrgCtx.interDirNeighbours[ count ] |= ( 1 << uiCurrRefListId );
            LICFlag = tempLICFlag;
            found = true;
          }
          else
          {
            mrgCtx.mvFieldNeighbours[( count << 1 ) + uiCurrRefListId].setMvField( Mv(), NOT_VALID );
            mrgCtx.interDirNeighbours[ count ] &= ~( 1 << uiCurrRefListId );
          }
        }
      }
    }
  }

  if( !found )
  {
    return false;
  }

  int xOff = iPUWidth  / 2;
  int yOff = iPUHeight / 2;

  // compute the location of the current PU
  xOff += ( ( cTempVector.getHor() + mvRndOffs ) >> mvPrec );
  yOff += ( ( cTempVector.getVer() + mvRndOffs ) >> mvPrec );

  int iPicWidth  = pColPic->lwidth()  - 1;
  int iPicHeight = pColPic->lheight() - 1;

  MotionBuf& mb = mrgCtx.subPuMvpMiBuf;

  const bool isBiPred = isBipredRestriction( pu );

  for( int y = puPos.y; y < puPos.y + puSize.height; y += iPUHeight )
  {
    for( int x = puPos.x; x < puPos.x + puSize.width; x += iPUWidth )
    {
      Position colPos{ x + xOff, y + yOff };

      colPos.x = Clip3( 0, iPicWidth, colPos.x );
      colPos.y = Clip3( 0, iPicHeight, colPos.y );

      const MotionInfo &colMi = pColPic->cs->getMotionInfo( colPos );

      MotionInfo mi;

      mi.isInter  = true;
      mi.sliceIdx = slice.getIndependentSliceIdx();

      if( colMi.isInter )
      {
        for( UInt uiCurrRefListId = 0; uiCurrRefListId < ( bBSlice ? 2 : 1 ); uiCurrRefListId++ )
        {
          RefPicList eCurrRefPicList = RefPicList( uiCurrRefListId );
          if( deriveScaledMotionTemporal( slice, colPos, pColPic, eCurrRefPicList, cColMv, tempLICFlag, eFetchRefPicList ) )
          {
            mi.refIdx[uiCurrRefListId] = 0;
            mi.mv    [uiCurrRefListId] = cColMv;
          }
        }
      }
      else
      {
        // intra coded, in this case, no motion vector is available for list 0 or list 1, so use default
        mi.mv    [0] = mrgCtx.mvFieldNeighbours[( count << 1 ) + 0].mv;
        mi.mv    [1] = mrgCtx.mvFieldNeighbours[( count << 1 ) + 1].mv;
        mi.refIdx[0] = mrgCtx.mvFieldNeighbours[( count << 1 ) + 0].refIdx;
        mi.refIdx[1] = mrgCtx.mvFieldNeighbours[( count << 1 ) + 1].refIdx;
      }

      mi.interDir = ( mi.refIdx[0] != -1 ? 1 : 0 ) + ( mi.refIdx[1] != -1 ? 2 : 0 );

      if( isBiPred && mi.interDir == 3 )
      {
        mi.interDir  = 1;
        mi.mv    [1] = Mv();
        mi.refIdx[1] = NOT_VALID;
      }

      mb.subBuf( g_miScaling.scale( Position{ x, y } - pu.lumaPos() ), g_miScaling.scale( Size( iPUWidth, iPUHeight ) ) ).fill( mi );
    }
  }

  return true;
}

static int xGetDistScaleFactorFRUC(const int &iCurrPOC, const int &iCurrRefPOC, const int &iColPOC, const int &iColRefPOC, Slice* slice)
{
  int iDiffPocD = iColPOC - iColRefPOC;
  int iDiffPocB = iCurrPOC - iCurrRefPOC;

  if (iDiffPocD == iDiffPocB)
  {
    return 4096;
  }
  else
  {
    int iTDB = Clip3(-128, 127, iDiffPocB);
    int iTDD = Clip3(-128, 127, iDiffPocD);
    int iScale = slice->getScaleFactor( iTDB , iTDD );

    //    int iX = (0x4000 + abs(iTDD / 2)) / iTDD;
    //    int iScale = Clip3(-4096, 4095, (iTDB * iX + 32) >> 6);
    return iScale;
  }
}

bool PU::getMvPair( const PredictionUnit &pu, RefPicList eCurRefPicList, const MvField & rCurMvField, MvField & rMvPair )
{
  Slice &slice      = *pu.cs->slice;

  Int nTargetRefIdx = slice.getRefIdx4MVPair( eCurRefPicList , rCurMvField.refIdx );
  if( nTargetRefIdx < 0 )
    return false;

  RefPicList eTarRefPicList = ( RefPicList )( 1 - ( Int ) eCurRefPicList );
  Int nCurPOC               = slice.getPOC();
  Int nRefPOC               = slice.getRefPOC( eCurRefPicList , rCurMvField.refIdx );
  Int nTargetPOC            = slice.getRefPOC( eTarRefPicList , nTargetRefIdx );
  Int nScale                = xGetDistScaleFactorFRUC( nCurPOC , nTargetPOC , nCurPOC , nRefPOC, pu.cs->slice );
  rMvPair.mv                = rCurMvField.mv.scaleMv( nScale );
  rMvPair.refIdx            = nTargetRefIdx;

  return true;
}

bool PU::isSameMVField( const PredictionUnit &pu, RefPicList eListA, MvField &rMVFieldA, RefPicList eListB, MvField &rMVFieldB )
{
  if( rMVFieldA.refIdx >= 0 && rMVFieldB.refIdx >= 0 )
  {
    return( rMVFieldA.mv == rMVFieldB.mv && pu.cs->slice->getRefPOC( eListA , rMVFieldA.refIdx ) == pu.cs->slice->getRefPOC( eListB , rMVFieldB.refIdx ) );
  }
  else
  {
    return false;
  }
}

Mv PU::scaleMv( const Mv &rColMV, Int iCurrPOC, Int iCurrRefPOC, Int iColPOC, Int iColRefPOC, Slice *slice )
{
  Mv mv = rColMV;
  Int iScale = xGetDistScaleFactorFRUC( iCurrPOC, iCurrRefPOC, iColPOC, iColRefPOC, slice );
  if ( iScale != 4096 )
  {
    mv = rColMV.scaleMv( iScale );
  }
  return mv;
}

static void getNeighboringMvField( const Slice& slice, const MotionInfo &miNB, MvField *cMvField, unsigned *pucInterDir )
{
  int iRefPOCSrc, iRefPOCMirror;
  RefPicList eRefPicListSrc /*, eRefPicListMirror*/;
  unsigned uiMvIdxSrc, uiMvIdxMirror;

  if( miNB.interDir == 3 )
  {
    *pucInterDir = 3;
    for( uiMvIdxSrc = 0; uiMvIdxSrc < 2; uiMvIdxSrc++ )
    {
      eRefPicListSrc = ( RefPicList ) uiMvIdxSrc;
      if( miNB.refIdx[eRefPicListSrc] == 0 )
      {
        cMvField[uiMvIdxSrc].setMvField( miNB.mv[eRefPicListSrc], miNB.refIdx[eRefPicListSrc] );
      }
      else
      {
        iRefPOCSrc    = slice.getRefPOC( eRefPicListSrc, miNB.refIdx[eRefPicListSrc] );
        iRefPOCMirror = slice.getRefPOC( eRefPicListSrc, 0 );

        Int iScale = xGetDistScaleFactor( slice.getPOC(), iRefPOCMirror, slice.getPOC(), iRefPOCSrc );
        if( iScale == 4096 )
        {
          cMvField[uiMvIdxSrc].setMvField( miNB.mv[eRefPicListSrc], 0 );
        }
        else
        {
          Mv mvNB = miNB.mv[eRefPicListSrc];

          if( slice.getSPS()->getSpsNext().getUseHighPrecMv() )
          {
            mvNB.setHighPrec();
          }

          cMvField[uiMvIdxSrc].setMvField( mvNB.scaleMv( iScale ), 0 );
        }
      }
    }
  }
  else
  {
    if( miNB.interDir == 1 )
    {
      eRefPicListSrc = REF_PIC_LIST_0;
      //eRefPicListMirror = REF_PIC_LIST_1;
      uiMvIdxSrc = 0;
    }
    else
    {
      eRefPicListSrc = REF_PIC_LIST_1;
      //eRefPicListMirror = REF_PIC_LIST_0;
      uiMvIdxSrc = 1;
    }

    *pucInterDir = uiMvIdxSrc + 1;
    uiMvIdxMirror = 1 - uiMvIdxSrc;

    iRefPOCSrc = slice.getRefPOC( eRefPicListSrc, miNB.refIdx[eRefPicListSrc] );

    if( miNB.refIdx[eRefPicListSrc] == 0 )
    {
      cMvField[uiMvIdxSrc].setMvField( miNB.mv[eRefPicListSrc], miNB.refIdx[eRefPicListSrc] );
    }
    else
    {
      iRefPOCMirror = slice.getRefPOC( eRefPicListSrc, 0 );
      Int iScale = xGetDistScaleFactor( slice.getPOC(), iRefPOCMirror, slice.getPOC(), iRefPOCSrc );
      if( iScale == 4096 )
      {
        cMvField[uiMvIdxSrc].setMvField( miNB.mv[eRefPicListSrc], 0 );
      }
      else
      {
        Mv mvNB = miNB.mv[eRefPicListSrc];

        if( slice.getSPS()->getSpsNext().getUseHighPrecMv() )
        {
          mvNB.setHighPrec();
        }

        cMvField[uiMvIdxSrc].setMvField( mvNB.scaleMv( iScale ), 0 );
      }
    }

    Mv cZeroMv;
    cZeroMv.setZero();
    cMvField[uiMvIdxMirror].setMvField( cZeroMv, -1 );
  }
}

static void generateMvField( const CodingStructure& cs, const MvField *mvField, unsigned* interDir, unsigned uiMvNum, MvField* cMvFieldMedian, unsigned &ucInterDirMedian )
{
  unsigned disable = uiMvNum;
  Mv cMv;
  ucInterDirMedian = 0;

  if( uiMvNum == 0 )
  {
    if( cs.slice->getSliceType() == P_SLICE )
    {
      ucInterDirMedian = 1;
      cMv.setZero();
      cMvFieldMedian[0].setMvField( cMv,  0 );
      cMvFieldMedian[1].setMvField( cMv, -1 );
    }
    else
    {
      ucInterDirMedian = 3;
      cMv.setZero();
      cMvFieldMedian[0].setMvField( cMv, 0 );
      cMvFieldMedian[1].setMvField( cMv, 0 );
    }
    return;
  }
  for( unsigned j = 0; j < 2; j++ )
  {
    int iExistMvNum = 0;
    int cMvX = 0, cMvY = 0;

    for( unsigned i = 0; i < uiMvNum; i++ )
    {
      if( interDir[i] & ( j + 1 ) && disable != i )
      {
        cMvX += mvField[( i << 1 ) + j].mv.hor;
        cMvY += mvField[( i << 1 ) + j].mv.ver;
        cMv.highPrec |= mvField[( i << 1 ) + j].mv.highPrec;
        iExistMvNum++;

        CHECK( cMv.highPrec != mvField[( i << 1 ) + j].mv.highPrec, "Mixed high and low precision vectors provided" );
      }
    }
    if( iExistMvNum )
    {
      ucInterDirMedian |= ( j + 1 );

      if( iExistMvNum == 3 )
      {
        cMv.set( ( int ) ( cMvX * 43 / 128 ), ( int ) ( cMvY * 43 / 128 ) );
      }
      else if( iExistMvNum == 2 )
      {
        cMv.set( ( int ) ( cMvX / 2 ), ( int ) ( cMvY / 2 ) );
      }
      else
      {
        cMv.set( ( int ) ( cMvX ), ( int ) ( cMvY ) );
      }

      cMvFieldMedian[j].setMvField( cMv, 0 );
    }
    else
    {
      cMv.setZero();
      cMvFieldMedian[j].setMvField( cMv, -1 );
    }
  }
}

bool PU::getInterMergeSubPuRecurCand( const PredictionUnit &pu, MergeCtx& mrgCtx, const int count )
{
  const Slice&   slice   = *pu.cs->slice;
  const SPSNext& spsNext =  pu.cs->sps->getSpsNext();

  // compute the location of the current PU
  Position puPos    = pu.lumaPos();
  Size puSize       = pu.lumaSize();
  int iNumPartLine  = std::max( puSize.width  >> spsNext.getSubPuMvpLog2Size(), 1u );
  int iNumPartCol   = std::max( puSize.height >> spsNext.getSubPuMvpLog2Size(), 1u );
  int iPUHeight     = iNumPartCol  == 1 ? puSize.height : 1 << spsNext.getSubPuMvpLog2Size();
  int iPUWidth      = iNumPartLine == 1 ? puSize.width  : 1 << spsNext.getSubPuMvpLog2Size();
  int iNumPart      = iNumPartCol * iNumPartLine;

  unsigned uiSameCount      = 0;

  MotionBuf &mb = mrgCtx.subPuMvpExtMiBuf;

  MotionInfo mi1stSubPart;

  for( Int y = 0; y < puSize.height; y += iPUHeight )
  {
    for( Int x = 0; x < puSize.width; x += iPUWidth )
    {
      MvField cMvField[6];
      MvField cMvFieldMedian[2];

      unsigned ucInterDir[3];
      unsigned ucInterDirMedian = 0;
      unsigned uiMVCount=0;

      //get left
      if( x == 0 )
      {
        for( unsigned uiCurAddrY = y / iPUHeight; uiCurAddrY < puSize.height / iPUHeight; uiCurAddrY++ )
        {
          const Position        posLeft = puPos.offset( -1, uiCurAddrY * iPUHeight );
          const PredictionUnit* puLeft  = pu.cs->getPURestricted( posLeft, pu );

          if ( puLeft && !CU::isIntra( *puLeft->cu ) )
          {
            getNeighboringMvField( slice, puLeft->getMotionInfo( posLeft ), cMvField, ucInterDir );
            uiMVCount++;
            break;
          }
        }
      }
      else
      {
        const MotionInfo &miLeft = mb.at( g_miScaling.scale( Position{ x - 1, y } ) );

        ucInterDir[0] = miLeft.interDir;
        cMvField  [0].setMvField( miLeft.mv[0], miLeft.refIdx[0] );
        cMvField  [1].setMvField( miLeft.mv[1], miLeft.refIdx[1] );
        uiMVCount++;
      }
      //get above
      if ( y == 0 )
      {
        for( unsigned uiCurAddrX = x / iPUWidth; uiCurAddrX < iNumPartLine; uiCurAddrX++ )
        {
          const Position        posAbove = puPos.offset( uiCurAddrX * iPUWidth, -1 );
          const PredictionUnit *puAbove  = pu.cs->getPURestricted( posAbove, pu );

          if( puAbove && !CU::isIntra( *puAbove->cu ) )
          {
            getNeighboringMvField( slice, puAbove->getMotionInfo( posAbove ), cMvField + ( uiMVCount << 1 ), ucInterDir + uiMVCount );
            uiMVCount++;
            break;
          }
        }
      }
      else
      {
        const MotionInfo &miAbove = mb.at( g_miScaling.scale( Position{ x, y - 1 } ) );

        ucInterDir[  uiMVCount           ] = miAbove.interDir;
        cMvField  [( uiMVCount << 1 )    ].setMvField( miAbove.mv[0], miAbove.refIdx[0] );
        cMvField  [( uiMVCount << 1 ) + 1].setMvField( miAbove.mv[1], miAbove.refIdx[1] );
        uiMVCount++;
      }

      {
        ucInterDir[uiMVCount] = 0;

        if( slice.getEnableTMVPFlag() )
        {
          Position posRB = puPos.offset( x + iPUWidth - 3, y + iPUHeight - 3 );

          const PreCalcValues& pcv = *pu.cs->pcv;

          bool bExistMV = false;
          Position posC0;
          Position posC1 = puPos.offset( x + iPUWidth / 2, y + iPUHeight / 2 );
          bool C0Avail = false;

          Mv cColMv;
          int iRefIdx = 0;

          if( ( ( posRB.x + pcv.minCUWidth ) < pcv.lumaWidth ) && ( ( posRB.y + pcv.minCUHeight ) < pcv.lumaHeight ) )
          {
            posC0   = posRB.offset( 4, 4 );
            C0Avail = true;
          }

          bExistMV = ( C0Avail && getColocatedMVP( pu, REF_PIC_LIST_0, posC0, cColMv, iRefIdx ) ) || getColocatedMVP( pu, REF_PIC_LIST_0, posC1, cColMv, iRefIdx );

          if( bExistMV )
          {
            ucInterDir[uiMVCount     ] |= 1;
            cMvField  [uiMVCount << 1].setMvField( cColMv, iRefIdx );
          }

          if( slice.isInterB() )
          {
            bExistMV = ( C0Avail && getColocatedMVP( pu, REF_PIC_LIST_1, posC0, cColMv, iRefIdx ) ) || getColocatedMVP( pu, REF_PIC_LIST_1, posC1, cColMv, iRefIdx );

            if( bExistMV )
            {
              ucInterDir[ uiMVCount          ] |= 2;
              cMvField  [(uiMVCount << 1) + 1].setMvField( cColMv, iRefIdx );
            }
          }

          if( ucInterDir[uiMVCount] > 0 )
          {
            uiMVCount++;
          }
        }
      }

      if( pu.cs->sps->getSpsNext().getUseHighPrecMv() )
      {
        for( MvField &f : cMvField )
        {
          // set all vectors as high precision
          f.mv.setHighPrec();
        }
      }

      generateMvField( *pu.cs, cMvField, ucInterDir, uiMVCount, cMvFieldMedian, ucInterDirMedian );

      MotionInfo mi;
      mi.isInter    = true;
      mi.sliceIdx   = pu.cs->slice->getIndependentSliceIdx();
      mi.interDir   = ucInterDirMedian;
      mi.mv    [0]  = cMvFieldMedian[0].mv;
      mi.refIdx[0]  = cMvFieldMedian[0].refIdx;
      mi.mv    [1]  = cMvFieldMedian[1].mv;
      mi.refIdx[1]  = cMvFieldMedian[1].refIdx;

      if( x == 0 && y == 0 )
      {
        mi1stSubPart = mi;
      }

      if( mi == mi1stSubPart )
      {
        uiSameCount++;
      }

      mb.subBuf( g_miScaling.scale( Position{ x, y } ), g_miScaling.scale( Size( iPUWidth, iPUHeight ) ) ).fill( mi );
    }
  }

  bool bAtmvpExtAva = true;

  if( uiSameCount == iNumPart )
  {
    for( unsigned uiIdx = 0; uiIdx < count; uiIdx++ )
    {
      if( mrgCtx.mrgTypeNeighnours[uiIdx] != MRG_TYPE_SUBPU_ATMVP )
      {
        if( spsNext.getUseHighPrecMv() )
        {
          mrgCtx.mvFieldNeighbours[( uiIdx << 1 )    ].mv.setHighPrec();
          mrgCtx.mvFieldNeighbours[( uiIdx << 1 ) + 1].mv.setHighPrec();
          mi1stSubPart.mv[0].setHighPrec();
          mi1stSubPart.mv[1].setHighPrec();
        }

        if( mrgCtx.interDirNeighbours[ uiIdx           ] == mi1stSubPart.interDir &&
            mrgCtx.mvFieldNeighbours[( uiIdx << 1 )    ] == MvField( mi1stSubPart.mv[0], mi1stSubPart.refIdx[0] ) &&
            mrgCtx.mvFieldNeighbours[( uiIdx << 1 ) + 1] == MvField( mi1stSubPart.mv[1], mi1stSubPart.refIdx[1] ) )
        {
          bAtmvpExtAva = false;
          break;
        }
      }
    }
  }

  if( bAtmvpExtAva && false ) // subPU fix: make it JEM like
  {
    for( unsigned idx = 0; idx < count; idx++ )
    {
      if( mrgCtx.mrgTypeNeighnours[idx] == MRG_TYPE_SUBPU_ATMVP )
      {
        bool isSame = true;
        for( int y = 0; y < mb.height; y++ )
        {
          for( int x = 0; x < mb.width; x++ )
          {
            if( mb.at( x, y ) != mrgCtx.subPuMvpMiBuf.at( x, y ) )
            {
              isSame = false;
              break;
            }
          }
        }
        bAtmvpExtAva = !isSame;
        break;
      }
    }
  }

  return bAtmvpExtAva;
}

const PredictionUnit* getFirstAvailableAffineNeighbour( const PredictionUnit &pu )
{
  const Position posLT = pu.Y().topLeft();
  const Position posRT = pu.Y().topRight();
  const Position posLB = pu.Y().bottomLeft();

  const PredictionUnit* puLeft = pu.cs->getPURestricted( posLB.offset( -1, 0 ), pu );
  if( puLeft && puLeft->cu->affine )
  {
    return puLeft;
  }
  const PredictionUnit* puAbove = pu.cs->getPURestricted( posRT.offset( 0, -1 ), pu );
  if( puAbove && puAbove->cu->affine )
  {
    return puAbove;
  }
  const PredictionUnit* puAboveRight = pu.cs->getPURestricted( posRT.offset( 1, -1 ), pu );
  if( puAboveRight && puAboveRight->cu->affine )
  {
    return puAboveRight;
  }
  const PredictionUnit *puLeftBottom = pu.cs->getPURestricted( posLB.offset( -1, 1 ), pu );
  if( puLeftBottom && puLeftBottom->cu->affine )
  {
    return puLeftBottom;
  }
  const PredictionUnit *puAboveLeft = pu.cs->getPURestricted( posLT.offset( -1, -1 ), pu );
  if( puAboveLeft && puAboveLeft->cu->affine )
  {
    return puAboveLeft;
  }
  return nullptr;
}

bool PU::isAffineMrgFlagCoded( const PredictionUnit &pu )
{
  if( ( pu.cs->sps->getSpsNext().getUseQTBT() ) && pu.cu->lumaSize().area() < 64 )
  {
    return false;
  }
  return getFirstAvailableAffineNeighbour( pu ) != nullptr;
}

Void PU::getAffineMergeCand( const PredictionUnit &pu, MvField (*mvFieldNeighbours)[3], unsigned char &interDirNeighbours, int &numValidMergeCand )
{
  for ( Int mvNum = 0; mvNum < 3; mvNum++ )
  {
    mvFieldNeighbours[0][mvNum].setMvField( Mv(), -1 );
    mvFieldNeighbours[1][mvNum].setMvField( Mv(), -1 );
  }

  const PredictionUnit* puFirstNeighbour = getFirstAvailableAffineNeighbour( pu );
  if( puFirstNeighbour == nullptr )
  {
    numValidMergeCand = -1;
    return;
  }
  else
  {
    numValidMergeCand = 1;
  }

  Int width  = puFirstNeighbour->Y().width;
  Int height = puFirstNeighbour->Y().height;

  // list0
  MvField affineMvField[2][3];
  const Position posLT = puFirstNeighbour->Y().topLeft();
  const Position posRT = puFirstNeighbour->Y().topRight();
  const Position posLB = puFirstNeighbour->Y().bottomLeft();

  MotionInfo miLT = puFirstNeighbour->getMotionInfo( posLT );
  MotionInfo miRT = puFirstNeighbour->getMotionInfo( posRT );
  MotionInfo miLB = puFirstNeighbour->getMotionInfo( posLB );

  miLT.mv[0].setHighPrec();
  miRT.mv[0].setHighPrec();
  miLB.mv[0].setHighPrec();

  miLT.mv[1].setHighPrec();
  miRT.mv[1].setHighPrec();
  miLB.mv[1].setHighPrec();

  affineMvField[0][0].setMvField( miLT.mv[0], miLT.refIdx[0] );
  affineMvField[0][1].setMvField( miRT.mv[0], miRT.refIdx[0] );
  affineMvField[0][2].setMvField( miLB.mv[0], miLB.refIdx[0] );

  // list1
  affineMvField[1][0].setMvField( miLT.mv[1], miLT.refIdx[1] );
  affineMvField[1][1].setMvField( miRT.mv[1], miRT.refIdx[1] );
  affineMvField[1][2].setMvField( miLB.mv[1], miLB.refIdx[1] );

  Int pixelOrgX    = puFirstNeighbour->Y().pos().x;
  Int pixelCurrX   = pu.Y().pos().x;
  Int pixelOrgY    = puFirstNeighbour->Y().pos().y;
  Int pixelCurrY   = pu.Y().pos().y;


  Int vx0 = Int( affineMvField[0][0].mv.getHor() + 1.0 * ( affineMvField[0][2].mv.getHor() - affineMvField[0][0].mv.getHor() ) * ( pixelCurrY - pixelOrgY ) / height + 1.0 * ( affineMvField[0][1].mv.getHor() - affineMvField[0][0].mv.getHor() ) * ( pixelCurrX - pixelOrgX ) / width );
  Int vy0 = Int( affineMvField[0][0].mv.getVer() + 1.0 * ( affineMvField[0][2].mv.getVer() - affineMvField[0][0].mv.getVer() ) * ( pixelCurrY - pixelOrgY ) / height + 1.0 * ( affineMvField[0][1].mv.getVer() - affineMvField[0][0].mv.getVer() ) * ( pixelCurrX - pixelOrgX ) / width );
  mvFieldNeighbours[0][0].setMvField( Mv(vx0, vy0, true), affineMvField[0][0].refIdx );
  if( pu.cs->slice->isInterB() )
  {
    vx0 = Int( affineMvField[1][0].mv.getHor() + 1.0 * ( affineMvField[1][2].mv.getHor() - affineMvField[1][0].mv.getHor() ) * ( pixelCurrY - pixelOrgY ) / height + 1.0 * ( affineMvField[1][1].mv.getHor() - affineMvField[1][0].mv.getHor() ) * ( pixelCurrX - pixelOrgX ) / width );
    vy0 = Int( affineMvField[1][0].mv.getVer() + 1.0 * ( affineMvField[1][2].mv.getVer() - affineMvField[1][0].mv.getVer() ) * ( pixelCurrY - pixelOrgY ) / height + 1.0 * ( affineMvField[1][1].mv.getVer() - affineMvField[1][0].mv.getVer() ) * ( pixelCurrX - pixelOrgX ) / width );
    mvFieldNeighbours[1][0].setMvField( Mv(vx0, vy0, true), affineMvField[1][0].refIdx );
  }

  const Int curWidth  = pu.Y().width;
  const Int curHeight = pu.Y().height;
  Int vx1 = Int( 1.0 * ( affineMvField[0][1].mv.getHor() - affineMvField[0][0].mv.getHor() ) * curWidth / width + mvFieldNeighbours[0][0].mv.getHor() );
  Int vy1 = Int( 1.0 * ( affineMvField[0][1].mv.getVer() - affineMvField[0][0].mv.getVer() ) * curWidth / width + mvFieldNeighbours[0][0].mv.getVer() );
  mvFieldNeighbours[0][1].setMvField( Mv(vx1, vy1, true), affineMvField[0][0].refIdx );
  if( pu.cs->slice->isInterB() )
  {
    vx1 = Int( 1.0 * ( affineMvField[1][1].mv.getHor() - affineMvField[1][0].mv.getHor() ) * curWidth / width + mvFieldNeighbours[1][0].mv.getHor() );
    vy1 = Int( 1.0 * ( affineMvField[1][1].mv.getVer() - affineMvField[1][0].mv.getVer() ) * curWidth / width + mvFieldNeighbours[1][0].mv.getVer() );
    mvFieldNeighbours[1][1].setMvField( Mv(vx1, vy1, true), affineMvField[1][0].refIdx );
  }

  Int vx2 = Int( 1.0 * ( affineMvField[0][2].mv.getHor() - affineMvField[0][0].mv.getHor() ) * curHeight / height + mvFieldNeighbours[0][0].mv.getHor() );
  Int vy2 = Int( 1.0 * ( affineMvField[0][2].mv.getVer() - affineMvField[0][0].mv.getVer() ) * curHeight / height + mvFieldNeighbours[0][0].mv.getVer() );
  mvFieldNeighbours[0][2].setMvField( Mv(vx2, vy2, true), affineMvField[0][0].refIdx );
  if( pu.cs->slice->isInterB() )
  {
    vx2 = Int( 1.0 * ( affineMvField[1][2].mv.getHor() - affineMvField[1][0].mv.getHor() ) * curHeight / height + mvFieldNeighbours[1][0].mv.getHor() );
    vy2 = Int( 1.0 * ( affineMvField[1][2].mv.getVer() - affineMvField[1][0].mv.getVer() ) * curHeight / height + mvFieldNeighbours[1][0].mv.getVer() );
    mvFieldNeighbours[1][2].setMvField( Mv(vx2, vy2, true), affineMvField[1][0].refIdx );
  }
  interDirNeighbours = puFirstNeighbour->getMotionInfo().interDir;
}

Void PU::setAllAffineMvField( PredictionUnit &pu, MvField *mvField, RefPicList eRefList )
{
  // Set Mv
  Mv mv[3];
  for ( Int i = 0; i < 3; i++ )
  {
    mv[i] = mvField[i].mv;
  }
  setAllAffineMv( pu, mv[0], mv[1], mv[2], eRefList );

  // Set RefIdx
  CHECK( mvField[0].refIdx != mvField[1].refIdx || mvField[0].refIdx != mvField[2].refIdx, "Affine mv corners don't have the same refIdx." );
  pu.refIdx[eRefList] = mvField[0].refIdx;
}

Void PU::setAllAffineMv( PredictionUnit& pu, Mv affLT, Mv affRT, Mv affLB, RefPicList eRefList )
{
  Int iWidth  = pu.Y().width;

  const PreCalcValues& pcv = *pu.cs->pcv;

  Int iBit = 6;
  // convert to 2^(2+iBit) precision

  Int bitMinW = pcv.minCUWidthLog2;
  Int bitMinH = pcv.minCUHeightLog2;

  affLT.setHighPrec();
  affRT.setHighPrec();
  affLB.setHighPrec();

  Int iDMvHorX = ( (affRT - affLT).getHor() << (iBit + bitMinW) ) / iWidth;
  Int iDMvHorY = ( (affRT - affLT).getVer() << (iBit + bitMinH) ) / iWidth;
  Int iDMvVerX = -iDMvHorY;
  Int iDMvVerY =  iDMvHorX;

  Int iMvScaleHor = affLT.getHor() << iBit;
  Int iMvScaleVer = affLT .getVer() << iBit;
  Int iMvYHor = iMvScaleHor;
  Int iMvYVer = iMvScaleVer;

  MotionBuf mb = pu.getMotionBuf();
  // Calculate Mv for 4x4 Part
  Int iMvScaleTmpHor, iMvScaleTmpVer;
  for ( Int y = 0; y < mb.height; y++ )
  {
    for ( Int x = 0; x < mb.width; x++ )
    {
      iMvScaleTmpHor = iMvScaleHor + ( iDMvHorX >> 1 ) + ( iDMvVerX >> 1 );
      iMvScaleTmpVer = iMvScaleVer + ( iDMvHorY >> 1 ) + ( iDMvVerY >> 1 );

      // get the MV in hevc define precision
      Int xHevc, yHevc;
      xHevc  = iMvScaleTmpHor >> iBit;
      yHevc  = iMvScaleTmpVer >> iBit;
      Mv mvHevc(xHevc, yHevc, true);

      mb.at(x,y).mv[eRefList] = mvHevc;

      iMvScaleHor += iDMvHorX;  // switch from x to x+AffineBlockSize, add deltaMvHor
      iMvScaleVer += iDMvHorY;
    }
    iMvYHor += iDMvVerX;        // switch from y to y+AffineBlockSize, add deltaMvVer
    iMvYVer += iDMvVerY;

    iMvScaleHor = iMvYHor;
    iMvScaleVer = iMvYVer;
  }

  // Set AffineMvField for affine motion compensation LT, RT, LB and RB
  Mv mv = affRT + affLB - affLT;
  mb.at(            0,             0 ).mv[eRefList] = affLT;
  mb.at( mb.width - 1,             0 ).mv[eRefList] = affRT;
  mb.at(            0, mb.height - 1 ).mv[eRefList] = affLB;
  mb.at( mb.width - 1, mb.height - 1 ).mv[eRefList] = mv;
}

Void PU::setAllAffineMvd( MotionBuf mb, const Mv& affLT, const Mv& affRT, RefPicList eRefList, Bool rectCUs )
{
  // Set all partitions (possibly unnecessarily set)
  Mv mvd;
  if( rectCUs )
  {
    mvd = affLT + affRT;
  }
  else
  {
    mvd = affRT;
  }
  mvd.setHighPrec();
  mvd >>= 1;
  for (UInt y = 0; y < mb.height; y++)
  {
    for (UInt x = 0; x < mb.width; x++)
    {
      mb.at(x,y).mvdAffi[eRefList] = mvd;
    }
  }

  // Overwrite corner partitions
  mb.at(          0, 0 ).mvdAffi[eRefList] = affLT;
  mb.at( mb.width-1, 0 ).mvdAffi[eRefList] = affRT;
  if( !rectCUs )
  {
    mb.at(          0, mb.height-1 ).mvdAffi[eRefList] = Mv( 0, 0 );
    mb.at( mb.width-1, mb.height-1 ).mvdAffi[eRefList] = affRT - affLT;
  }
}

void PU::spanMotionInfo( PredictionUnit &pu, const MergeCtx &mrgCtx )
{
  MotionBuf mb = pu.getMotionBuf();

  if( !pu.mergeFlag || pu.mergeType == MRG_TYPE_DEFAULT_N || pu.mergeType == MRG_TYPE_FRUC )
  {
    MotionInfo mi;

    mi.isInter  = CU::isInter( *pu.cu );
    mi.sliceIdx = pu.cu->slice->getIndependentSliceIdx();

    if( mi.isInter )
    {
      mi.interDir = pu.interDir;

      for( int i = 0; i < NUM_REF_PIC_LIST_01; i++ )
      {
        mi.mv[i]     = pu.mv[i];
        mi.refIdx[i] = pu.refIdx[i];
      }
    }

    if( pu.cu->affine )
    {
      for( int y = 0; y < mb.height; y++ )
      {
        for( int x = 0; x < mb.width; x++ )
        {
          MotionInfo &dest = mb.at( x, y );
          dest.isInter  = mi.isInter;
          dest.interDir = mi.interDir;
          dest.sliceIdx = mi.sliceIdx;
          for( int i = 0; i < NUM_REF_PIC_LIST_01; i++ )
          {
            if( mi.refIdx[i] == -1 )
            {
              dest.mv[i] = Mv();
            }
            dest.refIdx[i] = mi.refIdx[i];
          }
        }
      }
    }
    else
    {
      mb.fill( mi );
    }
  }
  else if( pu.mergeType == MRG_TYPE_SUBPU_ATMVP )
  {
    CHECK( mrgCtx.subPuMvpMiBuf.area() == 0 || !mrgCtx.subPuMvpMiBuf.buf, "Buffer not initialized" );
    mb.copyFrom( mrgCtx.subPuMvpMiBuf );
  }
  else if( pu.mergeFlag && pu.mergeType == MRG_TYPE_FRUC_SET )
  {
    CHECK( mrgCtx.subPuFrucMiBuf.area() == 0 || !mrgCtx.subPuFrucMiBuf.buf, "Buffer not initialized" );
    mb.copyFrom( mrgCtx.subPuFrucMiBuf );
  }
  else
  {
    CHECK( mrgCtx.subPuMvpExtMiBuf.area() == 0 || !mrgCtx.subPuMvpExtMiBuf.buf, "Buffer not initialized" );

    mb.copyFrom( mrgCtx.subPuMvpExtMiBuf );

    if( isBipredRestriction( pu ) )
    {
      for( int y = 0; y < mb.height; y++ )
      {
        for( int x = 0; x < mb.width; x++ )
        {
          MotionInfo &mi = mb.at( x, y );
          if( mi.interDir == 3 )
          {
            mi.interDir  = 1;
            mi.mv    [1] = Mv();
            mi.refIdx[1] = NOT_VALID;
          }
        }
      }
    }
  }

  spanLICFlags( pu, pu.cu->LICFlag );
}

void PU::spanLICFlags( PredictionUnit &pu, const bool LICFlag )
{
  MotionBuf mb = pu.getMotionBuf();

  for( int y = 0; y < mb.height; y++ )
  {
    for( int x = 0; x < mb.width; x++ )
    {
      MotionInfo& mi = mb.at( x, y );
      mi.usesLIC = LICFlag;
    }
  }
}

Void PU::applyImv( PredictionUnit& pu, MergeCtx &mrgCtx, InterPrediction *interPred )
{
  if( !pu.mergeFlag )
  {
    unsigned imvShift = pu.cu->imv << 1;
    if( pu.interDir != 2 /* PRED_L1 */ )
    {
      if (pu.cu->imv)
      {
        CHECK( pu.mvd[0].highPrec, "Motion vector difference should never be high precision" );
        pu.mvd[0] = Mv( pu.mvd[0].hor << imvShift, pu.mvd[0].ver << imvShift );
      }
      unsigned mvp_idx = pu.mvpIdx[0];
      AMVPInfo amvpInfo;
      PU::fillMvpCand( pu, REF_PIC_LIST_0, pu.refIdx[0], amvpInfo, interPred );
      pu.mvpNum[0] = amvpInfo.numCand;
      pu.mvpIdx[0] = mvp_idx;
      pu.mv    [0] = amvpInfo.mvCand[mvp_idx] + pu.mvd[0];
    }

    if (pu.interDir != 1 /* PRED_L0 */)
    {
      if( !( pu.cu->cs->slice->getMvdL1ZeroFlag() && pu.interDir == 3 ) && pu.cu->imv )/* PRED_BI */
      {
        CHECK( pu.mvd[1].highPrec, "Motion vector difference should never be high precision" );
        pu.mvd[1] = Mv( pu.mvd[1].hor << imvShift, pu.mvd[1].ver << imvShift );
      }
      unsigned mvp_idx = pu.mvpIdx[1];
      AMVPInfo amvpInfo;
      PU::fillMvpCand( pu, REF_PIC_LIST_1, pu.refIdx[1], amvpInfo, interPred );
      pu.mvpNum[1] = amvpInfo.numCand;
      pu.mvpIdx[1] = mvp_idx;
      pu.mv    [1] = amvpInfo.mvCand[mvp_idx] + pu.mvd[1];
    }
  }
  else
  {
    PU::getInterMergeCandidates ( pu, mrgCtx );
    PU::restrictBiPredMergeCands( pu, mrgCtx );

    mrgCtx.setMergeInfo( pu, pu.mergeIdx );
  }

  PU::spanMotionInfo( pu, mrgCtx );
}


bool PU::isBIOLDB( const PredictionUnit& pu )
{
  const Slice&  slice   = *pu.cs->slice;
  bool          BIOLDB  = false;
  if( !slice.getBioLDBPossible() )
  {
    return( BIOLDB );
  }
  if( slice.getCheckLDC() && pu.refIdx[0] >= 0 && pu.refIdx[1] >= 0 )
  {
    const int pocCur  = slice.getPOC    ();
    const int poc0    = slice.getRefPOC ( REF_PIC_LIST_0, pu.refIdx[0] );
    const int poc1    = slice.getRefPOC ( REF_PIC_LIST_1, pu.refIdx[1] );
    if( poc0 != poc1 && ( poc0 - pocCur ) * ( poc1 - pocCur ) > 0 )
    {
      const int   dT0       = poc0 - pocCur;
      const int   dT1       = poc1 - pocCur;
      const bool  zeroMv0   = ( ( pu.mv[0].getAbsHor() + pu.mv[0].getAbsVer() ) == 0 );
      const bool  zeroMv1   = ( ( pu.mv[1].getAbsHor() + pu.mv[1].getAbsVer() ) == 0 );
      if( !zeroMv0 && !zeroMv1 )
      {
        Mv mv0 = pu.mv[0]; mv0.setHighPrec();
        Mv mv1 = pu.mv[1]; mv1.setHighPrec();
        BIOLDB = (   dT0 * mv1.getHor() == dT1 * mv0.getHor()
                  && dT0 * mv1.getVer() == dT1 * mv0.getVer() );
      }
    }
  }
  return BIOLDB;
}


void PU::restrictBiPredMergeCands( const PredictionUnit &pu, MergeCtx& mergeCtx )
{
  if( PU::isBipredRestriction( pu ) )
  {
    for( UInt mergeCand = 0; mergeCand < mergeCtx.numValidMergeCand; ++mergeCand )
    {
      if( mergeCtx.interDirNeighbours[ mergeCand ] == 3 )
      {
        mergeCtx.interDirNeighbours[ mergeCand ] = 1;
        mergeCtx.mvFieldNeighbours[( mergeCand << 1 ) + 1].setMvField( Mv( 0, 0 ), -1 );
      }
    }
  }
}


void CU::resetMVDandMV2Int( CodingUnit& cu, InterPrediction *interPred )
{
  for( auto &pu : CU::traversePUs( cu ) )
  {
    MergeCtx mrgCtx;

    if( !pu.mergeFlag )
    {
      unsigned imvShift = cu.imv << 1;
      if( pu.interDir != 2 /* PRED_L1 */ )
      {
        Mv mv        = pu.mv[0];
        Mv mvPred;
        AMVPInfo amvpInfo;
        PU::fillMvpCand( pu, REF_PIC_LIST_0, pu.refIdx[0], amvpInfo, interPred );
        pu.mvpNum[0] = amvpInfo.numCand;

        mvPred       = amvpInfo.mvCand[pu.mvpIdx[0]];
        roundMV      ( mv, imvShift );
        pu.mv[0]     = mv;
        Mv mvDiff    = mv - mvPred;
        pu.mvd[0]    = mvDiff;
      }
      if( pu.interDir != 1 /* PRED_L0 */ )
      {
        Mv mv        = pu.mv[1];
        Mv mvPred;
        AMVPInfo amvpInfo;
        PU::fillMvpCand( pu, REF_PIC_LIST_1, pu.refIdx[1], amvpInfo, interPred );
        pu.mvpNum[1] = amvpInfo.numCand;

        mvPred       = amvpInfo.mvCand[pu.mvpIdx[1]];
        roundMV      ( mv, imvShift );
        Mv mvDiff    = mv - mvPred;

        if( pu.cu->cs->slice->getMvdL1ZeroFlag() && pu.interDir == 3 /* PRED_BI */ )
        {
          pu.mvd[1] = Mv();
          mv = mvPred;
        }
        else
        {
          pu.mvd[1] = mvDiff;
        }
        pu.mv[1] = mv;
      }
    }
    else
    {
      if( pu.frucMrgMode )
      {
        Bool avail = interPred->deriveFRUCMV( pu );
        CHECK( !avail, "FRUC not available" );
        continue;
      }
      else
      {
        if( cu.cs->sps->getSpsNext().getUseSubPuMvp() )
        {
          // 2 MotionInfo buffers storing number of elements in maximal CU-size with 4x4 sub-sampling
          const Size bufSize = g_miScaling.scale( pu.lumaSize() );

          mrgCtx.subPuMvpMiBuf    = MotionBuf( ( MotionInfo* ) alloca( bufSize.area() * sizeof( MotionInfo ) ), bufSize );
          mrgCtx.subPuMvpExtMiBuf = MotionBuf( ( MotionInfo* ) alloca( bufSize.area() * sizeof( MotionInfo ) ), bufSize );
        }

        PU::getInterMergeCandidates ( pu, mrgCtx );
        PU::restrictBiPredMergeCands( pu, mrgCtx );

        mrgCtx.setMergeInfo( pu, pu.mergeIdx );
      }
    }

    PU::spanMotionInfo( pu, mrgCtx );
  }
}

bool CU::hasSubCUNonZeroMVd( const CodingUnit& cu )
{
  bool bNonZeroMvd = false;

  for( const auto &pu : CU::traversePUs( cu ) )
  {
    if( ( !pu.mergeFlag ) && ( !cu.skip ) )
    {
      if( pu.interDir != 2 /* PRED_L1 */ )
      {
        bNonZeroMvd |= pu.mvd[REF_PIC_LIST_0].getHor() != 0;
        bNonZeroMvd |= pu.mvd[REF_PIC_LIST_0].getVer() != 0;
      }
      if( pu.interDir != 1 /* PRED_L0 */ )
      {
        if( !pu.cu->cs->slice->getMvdL1ZeroFlag() || pu.interDir != 3 /* PRED_BI */ )
        {
          bNonZeroMvd |= pu.mvd[REF_PIC_LIST_1].getHor() != 0;
          bNonZeroMvd |= pu.mvd[REF_PIC_LIST_1].getVer() != 0;
        }
      }
    }
  }

  return bNonZeroMvd;
}

int CU::getMaxNeighboriMVCandNum( const CodingStructure& cs, const Position& pos )
{
  const int  numDefault     = 0;
  int        maxImvNumCand  = 0;

  // Get BCBP of left PU
  const CodingUnit *cuLeft  = cs.getCURestricted( pos.offset( -1, 0 ), cs.slice->getIndependentSliceIdx(), cs.picture->tileMap->getTileIdxMap( pos ) );
  maxImvNumCand = ( cuLeft ) ? cuLeft->imvNumCand : numDefault;

  // Get BCBP of above PU
  const CodingUnit *cuAbove = cs.getCURestricted( pos.offset( 0, -1 ), cs.slice->getIndependentSliceIdx(), cs.picture->tileMap->getTileIdxMap( pos ) );
  maxImvNumCand = std::max( maxImvNumCand, ( cuAbove ) ? cuAbove->imvNumCand : numDefault );

  return maxImvNumCand;
}

bool CU::isObmcFlagCoded ( const CodingUnit &cu )
{
  int iMaxObmcSize = 16;

  if( cu.cs->pcv->rectCUs && cu.cs->pcv->only2Nx2N )
  {
    if( cu.predMode == MODE_INTRA || cu.firstPU->mergeFlag || ( cu.lwidth() * cu.lheight() > iMaxObmcSize * iMaxObmcSize ) )
    {
      return false;
    }
    else
    {
      return true;
    }
  }
  else
  {
    if( cu.predMode == MODE_INTRA || ( cu.firstPU->mergeFlag  && cu.partSize == SIZE_2Nx2N ) || ( cu.lwidth() > iMaxObmcSize ) )
    {
      return false;
    }
    else
    {
      return true;
    }
  }
}

bool PU::getNeighborMotion( PredictionUnit &pu, MotionInfo& mi, Position off, Int iDir, Bool bSubPu )
{
  PredictionUnit* tmpPu      = nullptr;
  Position posNeighborMotion = Position( 0, 0 );

  const Int iBlkSize         = pu.cs->sps->getSpsNext().getOBMCBlkSize();
  const Position posSubBlock ( pu.lumaPos().offset( off ) );


  if ( iDir == 0 ) //above
  {
    if( bSubPu && off.y > 0 )
    {
      tmpPu = &pu;
    }
    else
    {
      tmpPu = pu.cs->getPU( posSubBlock.offset( 0, -1 ) );
    }
    posNeighborMotion = posSubBlock.offset( 0, -1 );
  }
  else if( iDir == 1 ) //left
  {
    if( bSubPu && off.x > 0 )
    {
      tmpPu = &pu;
    }
    else
    {
      tmpPu = pu.cs->getPU( posSubBlock.offset( -1, 0 ) );
    }
    posNeighborMotion = posSubBlock.offset( -1, 0 );
  }
  else if( iDir == 2 ) //below
  {
    if( bSubPu && ( off.y + iBlkSize ) < pu.lheight() )
    {
      tmpPu = &pu;
    }
    else
    {
      tmpPu = pu.cs->getPU( pu.Y().bottomLeft().offset( 0, 1 ) );
    }
    posNeighborMotion = posSubBlock.offset( 0, iBlkSize );

    CHECK( pu.cu != tmpPu->cu, "Got a PU from a different CU when fetching a below PU" );
  }
  else if( iDir == 3 ) //right
  {
    if( bSubPu && ( off.x + iBlkSize ) < pu.lwidth() )
    {
      tmpPu = &pu;
    }
    else
    {
      tmpPu = pu.cs->getPU( pu.Y().topRight().offset( 1, 0 ) );
    }
    posNeighborMotion = posSubBlock.offset( iBlkSize, 0 );

    CHECK( pu.cu != tmpPu->cu, "Got a PU from a different CU when fetching a right PU" );
  }

  const bool bNoAdjacentMotion = !tmpPu || CU::isIntra( *tmpPu->cu );

  if( bNoAdjacentMotion )
  {
    return false;
  }

  mi                          = tmpPu->getMotionInfo( posNeighborMotion );
  const MotionInfo currMotion =     pu.getMotionInfo( posSubBlock       );

  if( mi.interDir )
  {
    if( mi.interDir != currMotion.interDir )
    {
      return true;
    }
    else
    {
      for( UInt iRefList = 0; iRefList < 2; iRefList++ )
      {
        if( currMotion.interDir & ( 1 << iRefList ) )
        {
          if( !( currMotion.mv[iRefList] == mi.mv[iRefList] && currMotion.refIdx[iRefList] == mi.refIdx[iRefList] ) )
          {
            return true;
          }
        }
      }
      return false;
    }
  }
  else
  {
    return false;
  }
}

// TU tools

bool TU::useDST(const TransformUnit &tu, const ComponentID &compID)
{
  return isLuma(compID) && tu.cu->predMode == MODE_INTRA;
}

bool TU::isNonTransformedResidualRotated(const TransformUnit &tu, const ComponentID &compID)
{
  return tu.cs->sps->getSpsRangeExtension().getTransformSkipRotationEnabledFlag() && tu.blocks[compID].width == 4 && tu.cu->predMode == MODE_INTRA;
}

bool TU::getCbf( const TransformUnit &tu, const ComponentID &compID )
{
  return getCbfAtDepth( tu, compID, tu.depth );
}

bool TU::getCbfAtDepth(const TransformUnit &tu, const ComponentID &compID, const unsigned &depth)
{
  return ((tu.cbf[compID] >> depth) & 1) == 1;
}

void TU::setCbfAtDepth(TransformUnit &tu, const ComponentID &compID, const unsigned &depth, const bool &cbf)
{
  // first clear the CBF at the depth
  tu.cbf[compID] &= ~(1  << depth);
  // then set the CBF
  tu.cbf[compID] |= ((cbf ? 1 : 0) << depth);
}

bool TU::hasTransformSkipFlag(const CodingStructure& cs, const CompArea& area)
{
  UInt transformSkipLog2MaxSize = cs.pps->getPpsRangeExtension().getLog2MaxTransformSkipBlockSize();

  if( cs.pcv->rectCUs )
  {
    return ( area.width * area.height <= (1 << ( transformSkipLog2MaxSize << 1 )) );
  }
  return ( area.width <= (1 << transformSkipLog2MaxSize) );
}

UInt TU::getGolombRiceStatisticsIndex(const TransformUnit &tu, const ComponentID &compID)
{
  const Bool transformSkip    = tu.transformSkip[compID];
  const Bool transquantBypass = tu.cu->transQuantBypass;

  //--------

  const UInt channelTypeOffset = isChroma(compID) ? 2 : 0;
  const UInt nonTransformedOffset = (transformSkip || transquantBypass) ? 1 : 0;

  //--------

  const UInt selectedIndex = channelTypeOffset + nonTransformedOffset;
  CHECK( selectedIndex >= RExt__GOLOMB_RICE_ADAPTATION_STATISTICS_SETS, "Invalid golomb rice adaptation statistics set" );

  return selectedIndex;
}

UInt TU::getCoefScanIdx(const TransformUnit &tu, const ComponentID &compID)
{

  //------------------------------------------------

  //this mechanism is available for intra only

  if (!CU::isIntra(*tu.cu))
  {
    return SCAN_DIAG;
  }

  //------------------------------------------------

  //check that MDCS can be used for this TU


  const CompArea &area      = tu.blocks[compID];
  const SPS &sps            = *tu.cs->sps;
  const ChromaFormat format = sps.getChromaFormatIdc();

  const UInt maximumWidth  = MDCS_MAXIMUM_WIDTH  >> getComponentScaleX(compID, format);
  const UInt maximumHeight = MDCS_MAXIMUM_HEIGHT >> getComponentScaleY(compID, format);

  if ((area.width > maximumWidth) || (area.height > maximumHeight))
  {
    return SCAN_DIAG;
  }

  //------------------------------------------------

  //otherwise, select the appropriate mode

  const PredictionUnit &pu = *tu.cs->getPU( area.pos(), toChannelType( compID ) );

  UInt uiDirMode = PU::getFinalIntraMode(pu, toChannelType(compID));

  //------------------

       if (abs((Int) uiDirMode - VER_IDX) <= MDCS_ANGLE_LIMIT)
  {
    return SCAN_HOR;
  }
  else if (abs((Int) uiDirMode - HOR_IDX) <= MDCS_ANGLE_LIMIT)
  {
    return SCAN_VER;
  }
  else
  {
    return SCAN_DIAG;
  }
}

bool TU::isProcessingAllQuadrants(const UnitArea &tuArea)
{
  if (tuArea.chromaFormat == CHROMA_444)
  {
    return true;
  }
  else
  {
    return tuArea.Cb().valid() && tuArea.lumaSize().width != tuArea.chromaSize().width;
  }
}

bool TU::hasCrossCompPredInfo( const TransformUnit &tu, const ComponentID &compID )
{
  return ( isChroma(compID) && tu.cs->pps->getPpsRangeExtension().getCrossComponentPredictionEnabledFlag() && TU::getCbf( tu, COMPONENT_Y ) &&
         ( CU::isInter(*tu.cu) || PU::isChromaIntraModeCrossCheckMode( *tu.cs->getPU( tu.blocks[compID].pos(), toChannelType( compID ) ) ) ) );
}

UInt TU::getNumNonZeroCoeffsNonTS( const TransformUnit& tu )
{
  UInt count = 0;
  for( UInt i = 0; i < ::getNumberValidTBlocks( *tu.cs->pcv ); i++ )
  {
    if( tu.blocks[i].valid() && !tu.transformSkip[i] && TU::getCbf( tu, ComponentID( i ) ) )
    {
      UInt area = tu.blocks[i].area();
      const TCoeff* coeff = tu.getCoeffs( ComponentID( i ) ).buf;
      for( UInt j = 0; j < area; j++ )
      {
        count += coeff[j] != 0;
      }
    }
  }
  return count;
}


Bool TU::needsSqrt2Scale(const Size& size)
{
  return (((g_aucLog2[size.width] + g_aucLog2[size.height]) & 1) == 1);
}

// other tools

UInt getCtuAddr( const Position& pos, const PreCalcValues& pcv )
{
  return ( pos.x >> pcv.maxCUWidthLog2 ) + ( pos.y >> pcv.maxCUHeightLog2 ) * pcv.widthInCtus;
}

