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

/** \file     CodingStructure.h
 *  \brief    A class managing the coding information for a specific image part
 */

#include "CodingStructure.h"

#include "Unit.h"
#include "Slice.h"
#include "Picture.h"
#include "UnitTools.h"
#include "UnitPartitioner.h"

XUCache g_globalUnitCache = XUCache();

const UnitScale UnitScaleArray[NUM_CHROMA_FORMAT][MAX_NUM_COMPONENT] =
{
  { {2,2}, {0,0}, {0,0} },  // 4:0:0
  { {2,2}, {1,1}, {1,1} },  // 4:2:0
  { {2,2}, {1,2}, {1,2} },  // 4:2:2
  { {2,2}, {2,2}, {2,2} }   // 4:4:4
};

// ---------------------------------------------------------------------------
// coding structure method definitions
// ---------------------------------------------------------------------------

CodingStructure::CodingStructure()
  : area      ()
  , picture   ( nullptr )
  , parent    ( nullptr )
  , chType    ( CHANNEL_TYPE_LUMA )
  , m_isTuEnc ( false )
  , m_cuCache ( g_globalUnitCache.cuCache )
  , m_puCache ( g_globalUnitCache.puCache )
  , m_tuCache ( g_globalUnitCache.tuCache )
{
  for( UInt i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    m_coeffs[i] = nullptr;
    m_pcmbuf[i] = nullptr;

    m_offsets[i] = 0;
  }

  for( UInt i = 0; i < MAX_NUM_CHANNEL_TYPE; i++ )
  {
    m_cuIdx[i] = nullptr;
    m_puIdx[i] = nullptr;
    m_tuIdx[i] = nullptr;
    m_isDecomp[i] = nullptr;
  }

  m_motionBuf     = nullptr;
  m_motionBufFRUC = nullptr;

  if( g_isEncoder )
  {
    features.resize( NUM_ENC_FEATURES );
  }
}

CodingStructure::CodingStructure(CUCache& cuCache, PUCache& puCache, TUCache& tuCache)
  : area      ()
  , picture   ( nullptr )
  , parent    ( nullptr )
  , chType    ( CHANNEL_TYPE_LUMA )
  , m_isTuEnc ( false )
  , m_cuCache ( cuCache )
  , m_puCache ( puCache )
  , m_tuCache ( tuCache )
{
  for( UInt i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    m_coeffs[ i ] = nullptr;
    m_pcmbuf[ i ] = nullptr;

    m_offsets[ i ] = 0;
  }

  for( UInt i = 0; i < MAX_NUM_CHANNEL_TYPE; i++ )
  {
    m_cuIdx   [ i ] = nullptr;
    m_puIdx   [ i ] = nullptr;
    m_tuIdx   [ i ] = nullptr;
    m_isDecomp[ i ] = nullptr;
  }

  m_motionBuf     = nullptr;
  m_motionBufFRUC = nullptr;

  if( g_isEncoder )
  {
    features.resize( NUM_ENC_FEATURES );
  }
}

void CodingStructure::destroy()
{
  picture   = nullptr;
  parent    = nullptr;

  m_pred.destroy();
  m_resi.destroy();
  m_reco.destroy();
  m_orgr.destroy();

  destroyCoeffs();

  for( UInt i = 0; i < MAX_NUM_CHANNEL_TYPE; i++ )
  {
    delete[] m_isDecomp[ i ];
    m_isDecomp[ i ] = nullptr;

    delete[] m_cuIdx[ i ];
    m_cuIdx[ i ] = nullptr;

    delete[] m_puIdx[ i ];
    m_puIdx[ i ] = nullptr;

    delete[] m_tuIdx[ i ];
    m_tuIdx[ i ] = nullptr;
  }

  delete[] m_motionBuf;
  m_motionBuf = nullptr;

  delete[] m_motionBufFRUC;
  m_motionBufFRUC = nullptr;

  m_tuCache.cache( tus );
  m_puCache.cache( pus );
  m_cuCache.cache( cus );
}

void CodingStructure::releaseIntermediateData()
{
  clearTUs();
  clearPUs();
  clearCUs();
}

bool CodingStructure::isDecomp( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ )
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  if( area.blocks[effChType].contains( pos ) )
  {
    return m_isDecomp[effChType][rsAddr( pos, area.blocks[effChType], area.blocks[effChType].width, unitScale[effChType] )];
  }
  else if( parent )
  {
    return parent->isDecomp( pos, effChType );
  }
  else
  {
    return false;
  }
}

bool CodingStructure::isDecomp( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  if( area.blocks[effChType].contains( pos ) )
  {
    return m_isDecomp[effChType][rsAddr( pos, area.blocks[effChType].pos(), area.blocks[effChType].width, unitScale[effChType] )];
  }
  else if( parent )
  {
    return parent->isDecomp( pos, effChType );
  }
  else
  {
    return false;
  }
}

void CodingStructure::setDecomp(const CompArea &_area, const bool _isCoded /*= true*/)
{
  const UnitScale& scale = unitScale[_area.compID];
  AreaBuf<Bool> isCodedBlk( m_isDecomp[toChannelType( _area.compID )] + rsAddr( _area, area.blocks[_area.compID].pos(), area.blocks[_area.compID].width, scale ),
                            area.blocks[_area.compID].width >> scale.posx,
                            _area.width                     >> scale.posx,
                            _area.height                    >> scale.posy);
  isCodedBlk.fill( _isCoded );
}

void CodingStructure::setDecomp(const UnitArea &_area, const bool _isCoded /*= true*/)
{
  for( UInt i = 0; i < _area.blocks.size(); i++ )
  {
    if( _area.blocks[i].valid() ) setDecomp( _area.blocks[i], _isCoded );
  }
}



CodingUnit* CodingStructure::getCU( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ )
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const CompArea &_blk = area.blocks[ effChType ];

  if( !_blk.contains( pos ) )
  {
    if( parent ) return parent->getCU( pos, effChType );
    else         return nullptr;
  }
  else
  {
    const unsigned idx = m_cuIdx[ effChType ][ rsAddr( pos, _blk.pos(), _blk.width, unitScale[ effChType ] ) ];

    if( idx != 0 ) return cus[ idx - 1 ];
    else           return nullptr;
  }
}

const CodingUnit* CodingStructure::getCU( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const CompArea &_blk = area.blocks[ effChType ];

  if( !_blk.contains( pos ) )
  {
    if( parent ) return parent->getCU( pos, effChType );
    else         return nullptr;
  }
  else
  {
    const unsigned idx = m_cuIdx[ effChType ][ rsAddr( pos, _blk.pos(), _blk.width, unitScale[ effChType ] ) ];

    if( idx != 0 ) return cus[ idx - 1 ];
    else           return nullptr;
  }
}

PredictionUnit* CodingStructure::getPU( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ )
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const CompArea &_blk = area.blocks[ effChType ];

  if( !_blk.contains( pos ) )
  {
    if( parent ) return parent->getPU( pos, effChType );
    else         return nullptr;
  }
  else
  {
    const unsigned idx = m_puIdx[ effChType ][ rsAddr( pos, _blk.pos(), _blk.width, unitScale[ effChType ] ) ];

    if( idx != 0 ) return pus[ idx - 1 ];
    else           return nullptr;
  }
}

const PredictionUnit * CodingStructure::getPU( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const CompArea &_blk = area.blocks[ effChType ];

  if( !_blk.contains( pos ) )
  {
    if( parent ) return parent->getPU( pos, effChType );
    else         return nullptr;
  }
  else
  {
    const unsigned idx = m_puIdx[ effChType ][ rsAddr( pos, _blk.pos(), _blk.width, unitScale[ effChType ] ) ];

    if( idx != 0 ) return pus[ idx - 1 ];
    else           return nullptr;
  }
}

TransformUnit* CodingStructure::getTU( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ )
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const CompArea &_blk = area.blocks[ effChType ];

  if( !_blk.contains( pos ) )
  {
    if( parent ) return parent->getTU( pos, effChType );
    else         return nullptr;
  }
  else
  {
    const unsigned idx = m_tuIdx[ effChType ][ rsAddr( pos, _blk.pos(), _blk.width, unitScale[ effChType ] ) ];

    if( idx != 0 )       return tus[ idx - 1 ];
    else if( m_isTuEnc ) return parent->getTU( pos, effChType );
    else                 return nullptr;
  }
}

const TransformUnit * CodingStructure::getTU( const Position &pos, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const CompArea &_blk = area.blocks[ effChType ];

  if( !_blk.contains( pos ) )
  {
    if( parent ) return parent->getTU( pos, effChType );
    else         return nullptr;
  }
  else
  {
    const unsigned idx = m_tuIdx[ effChType ][ rsAddr( pos, _blk.pos(), _blk.width, unitScale[ effChType ] ) ];

    if( idx != 0 )       return tus[ idx - 1 ];
    else if( m_isTuEnc ) return parent->getTU( pos, effChType );
    else                 return nullptr;
  }
}

CodingUnit& CodingStructure::addCU(const UnitArea &unit)
{
  CodingUnit *cu = m_cuCache.get();

  cu->UnitArea::operator=( unit );
  cu->initData();
  cu->cs        = this;
  cu->slice     = nullptr;
  cu->next      = nullptr;
  cu->firstPU   = nullptr;
  cu->lastPU    = nullptr;
  cu->firstTU   = nullptr;
  cu->lastTU    = nullptr;

  CodingUnit *prevCU = m_numCUs > 0 ? cus.back() : nullptr;

  if( prevCU )
  {
    prevCU->next = cu;
  }

  cus.push_back( cu );

  UInt idx = ++m_numCUs;
  cu->idx  = idx;

  UInt numCh = ::getNumberValidChannels( area.chromaFormat );

  for( UInt i = 0; i < numCh; i++ )
  {
    if( !cu->blocks[i].valid() )
    {
      continue;
    }

    const CompArea &_selfBlk = area.blocks[i];
    const CompArea     &_blk = cu-> blocks[i];

    const UnitScale& scale = unitScale[_blk.compID];
    const Area scaledSelf  = scale.scale( _selfBlk );
    const Area scaledBlk   = scale.scale(     _blk );
    AreaBuf<UInt>( m_cuIdx[i] + rsAddr( scaledBlk.pos(), scaledSelf.pos(), scaledSelf.width ), scaledSelf.width, scaledBlk.size() ).fill( idx );
  }

  return *cu;
}

PredictionUnit& CodingStructure::addPU( const UnitArea &unit )
{
  PredictionUnit *pu = m_puCache.get();

  pu->UnitArea::operator=( unit );
  pu->initData();
  pu->next  = nullptr;
  pu->cs    = this;
  pu->cu    = m_isTuEnc ? cus[0] : getCU( unit.blocks[chType].pos(), chType );

  PredictionUnit *prevPU = m_numPUs > 0 ? pus.back() : nullptr;

  if( prevPU )
  {
    prevPU->next = pu;
  }

  pus.push_back( pu );

  if( pu->cu->firstPU == nullptr )
  {
    pu->cu->firstPU = pu;
  }
  pu->cu->lastPU = pu;

  UInt idx = ++m_numPUs;
  pu->idx  = idx;

  UInt numCh = ::getNumberValidChannels( area.chromaFormat );
  for( UInt i = 0; i < numCh; i++ )
  {
    if( !pu->blocks[i].valid() )
    {
      continue;
    }

    const CompArea &_selfBlk = area.blocks[i];
    const CompArea     &_blk = pu-> blocks[i];

    const UnitScale& scale = unitScale[_blk.compID];
    const Area scaledSelf  = scale.scale( _selfBlk );
    const Area scaledBlk   = scale.scale(     _blk );
    AreaBuf<UInt>( m_puIdx[i] + rsAddr( scaledBlk.pos(), scaledSelf.pos(), scaledSelf.width ), scaledSelf.width, scaledBlk.size() ).fill( idx );
  }

  return *pu;
}

TransformUnit& CodingStructure::addTU( const UnitArea &unit )
{
  TransformUnit *tu = m_tuCache.get();

  tu->UnitArea::operator=( unit );
  tu->initData();
  tu->next  = nullptr;
  tu->cs    = this;
  tu->cu    = m_isTuEnc ? cus[0] : getCU( unit.blocks[chType].pos(), chType );

#if ENABLE_CHROMA_422
  if( pcv->multiBlock422 && tu->blocks.size() < MAX_NUM_TBLOCKS ) // special case: 5 transform blocks (2 per chroma component)
  {
    tu->blocks[ COMPONENT_Cb ].height >>= 1;
    tu->blocks[ COMPONENT_Cr ].height >>= 1;
    tu->blocks.push_back                ( tu->blocks[ COMPONENT_Cb ] );
    tu->blocks.push_back                ( tu->blocks[ COMPONENT_Cr ] );
    tu->blocks[ COMPONENT_Cb2].y       += tu->blocks[ COMPONENT_Cb ].height;
    tu->blocks[ COMPONENT_Cr2].y       += tu->blocks[ COMPONENT_Cr ].height;
  }
#endif

  TransformUnit *prevTU = m_numTUs > 0 ? tus.back() : nullptr;

  if( prevTU )
  {
    prevTU->next = tu;
  }

  tus.push_back( tu );

  if( tu->cu )
  {
    if( tu->cu->firstTU == nullptr )
    {
      tu->cu->firstTU = tu;
    }
    tu->cu->lastTU = tu;
  }

  UInt idx = ++m_numTUs;
  tu->idx  = idx;

  TCoeff *coeffs[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
  Pel    *pcmbuf[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

  UInt numCh = ::getNumberValidComponents( area.chromaFormat );

  for( UInt i = 0; i < numCh; i++ )
  {
    if( !tu->blocks[i].valid() )
    {
      continue;
    }

    if (i < ::getNumberValidChannels(area.chromaFormat))
    {
      const CompArea &_selfBlk = area.blocks[i];
      const CompArea     &_blk = tu-> blocks[i];

      const UnitScale& scale = unitScale[_blk.compID];
      const Area scaledSelf  = scale.scale( _selfBlk );
      const Area scaledBlk   = scale.scale(     _blk );
      AreaBuf<UInt>( m_tuIdx[i] + rsAddr( scaledBlk.pos(), scaledSelf.pos(), scaledSelf.width ), scaledSelf.width, scaledBlk.size() ).fill( idx );
    }

    coeffs[i] = m_coeffs[i] + m_offsets[i];
    pcmbuf[i] = m_pcmbuf[i] + m_offsets[i];

    unsigned areaSize = tu->blocks[i].area();
#if ENABLE_CHROMA_422
    if( pcv->multiBlock422 && i != COMPONENT_Y )
    {
      coeffs[i+SCND_TBLOCK_OFFSET] = coeffs[i] + areaSize;
      pcmbuf[i+SCND_TBLOCK_OFFSET] = pcmbuf[i] + areaSize;
      areaSize                   <<= 1;
    }
#endif
    m_offsets[i] += areaSize;
  }

  tu->init( coeffs, pcmbuf );

  return *tu;
}

CUTraverser CodingStructure::traverseCUs( const UnitArea& unit, const ChannelType _chType )
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  CodingUnit* firstCU = getCU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  CodingUnit* lastCU = firstCU;

  do { } while( lastCU && ( lastCU = lastCU->next ) && unit.contains( *lastCU ) );

  return CUTraverser( firstCU, lastCU );
}

PUTraverser CodingStructure::traversePUs( const UnitArea& unit, const ChannelType _chType )
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  PredictionUnit* firstPU = getPU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  PredictionUnit* lastPU  = firstPU;

  do { } while( lastPU && ( lastPU = lastPU->next ) && unit.contains( *lastPU ) );

  return PUTraverser( firstPU, lastPU );
}

TUTraverser CodingStructure::traverseTUs( const UnitArea& unit, const ChannelType _chType )
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  TransformUnit* firstTU = getTU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  TransformUnit* lastTU  = firstTU;

  do { } while( lastTU && ( lastTU = lastTU->next ) && unit.contains( *lastTU ) );

  return TUTraverser( firstTU, lastTU );
}

cCUTraverser CodingStructure::traverseCUs( const UnitArea& unit, const ChannelType _chType ) const
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const CodingUnit* firstCU = getCU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  const CodingUnit* lastCU  = firstCU;

  do { } while( lastCU && ( lastCU = lastCU->next ) && unit.contains( *lastCU ) );

  return cCUTraverser( firstCU, lastCU );
}

cPUTraverser CodingStructure::traversePUs( const UnitArea& unit, const ChannelType _chType ) const
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const PredictionUnit* firstPU = getPU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  const PredictionUnit* lastPU  = firstPU;

  do { } while( lastPU && ( lastPU = lastPU->next ) && unit.contains( *lastPU ) );

  return cPUTraverser( firstPU, lastPU );
}

cTUTraverser CodingStructure::traverseTUs( const UnitArea& unit, const ChannelType _chType ) const
{
  const ChannelType effChType = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;

  const TransformUnit* firstTU = getTU( isLuma( effChType ) ? unit.lumaPos() : unit.chromaPos(), effChType );
  const TransformUnit* lastTU  = firstTU;

  do { } while( lastTU && ( lastTU = lastTU->next ) && unit.contains( *lastTU ) );

  return cTUTraverser( firstTU, lastTU );
}

// coding utilities

void CodingStructure::create(const ChromaFormat &_chromaFormat, const Area& _area, const bool isTopLayer)
{
  createInternals( UnitArea( _chromaFormat, _area ), !isTopLayer );

  if( isTopLayer ) return;

  m_reco.create( area );
  m_pred.create( area );
  m_resi.create( area );
  m_orgr.create( area );
}

void CodingStructure::create(const UnitArea& _unit, const bool isTopLayer)
{
  createInternals( _unit, !isTopLayer );

  if( isTopLayer ) return;

  m_reco.create( area );
  m_pred.create( area );
  m_resi.create( area );
  m_orgr.create( area );
}

void CodingStructure::createInternals( const UnitArea& _unit, const bool _createCoeffs )
{
  area = _unit;

  ::memcpy( unitScale, UnitScaleArray[area.chromaFormat], sizeof( unitScale ) );

  picture = nullptr;
  parent  = nullptr;

  unsigned numCh = ::getNumberValidChannels(area.chromaFormat);

  for (unsigned i = 0; i < numCh; i++)
  {
    unsigned _area = unitScale[i].scale( area.blocks[i].size() ).area();

    m_isDecomp[i] = _area > 0 ? new bool    [_area] : nullptr;
    m_cuIdx[i]    = _area > 0 ? new unsigned[_area] : nullptr;
    m_puIdx[i]    = _area > 0 ? new unsigned[_area] : nullptr;
    m_tuIdx[i]    = _area > 0 ? new unsigned[_area] : nullptr;
  }

  numCh = getNumberValidComponents(area.chromaFormat);

  for (unsigned i = 0; i < numCh; i++)
  {
    m_offsets[i] = 0;
  }

  if( _createCoeffs ) createCoeffs();

  unsigned _lumaAreaScaled = g_miScaling.scale( area.lumaSize() ).area();
  m_motionBuf     = new MotionInfo[_lumaAreaScaled];
  m_motionBufFRUC = new MotionInfo[_lumaAreaScaled];

  initStructData();
}



void CodingStructure::rebindPicBufs()
{
  CHECK( parent, "rebindPicBufs can only be used for the top level CodingStructure" );

  if( !picture->m_bufs[PIC_RECONSTRUCTION].bufs.empty() ) m_reco.createFromBuf( picture->m_bufs[PIC_RECONSTRUCTION] );
  else                                                    m_reco.destroy();
  if( !picture->m_bufs[PIC_PREDICTION    ].bufs.empty() ) m_pred.createFromBuf( picture->m_bufs[PIC_PREDICTION] );
  else                                                    m_pred.destroy();
  if( !picture->m_bufs[PIC_RESIDUAL      ].bufs.empty() ) m_resi.createFromBuf( picture->m_bufs[PIC_RESIDUAL] );
  else                                                    m_resi.destroy();
                                                           
  if( g_isEncoder )                                        
  {                                                        
    if( !picture->m_bufs[PIC_RESIDUAL    ].bufs.empty() ) m_orgr.create( area.chromaFormat, area.blocks[0], pcv->maxCUWidth );
    else                                                  m_orgr.destroy();

  }
}

void CodingStructure::createCoeffs()
{
  const unsigned numCh = getNumberValidComponents( area.chromaFormat );

  for( unsigned i = 0; i < numCh; i++ )
  {
    unsigned _area = area.blocks[i].area();

    m_coeffs[i] = _area > 0 ? ( TCoeff* ) xMalloc( TCoeff, _area ) : nullptr;
    m_pcmbuf[i] = _area > 0 ? ( Pel*    ) xMalloc( Pel,    _area ) : nullptr;
  }
}

void CodingStructure::destroyCoeffs()
{
  for( UInt i = 0; i < MAX_NUM_COMPONENT; i++ )
  {
    if( m_coeffs[i] ) { xFree( m_coeffs[i] ); m_coeffs[i] = nullptr; }
    if( m_pcmbuf[i] ) { xFree( m_pcmbuf[i] ); m_pcmbuf[i] = nullptr; }
  }
}

void CodingStructure::initSubStructure( CodingStructure& subStruct, const UnitArea &subArea, const bool &isTuEnc, const ChannelType &_chType )
{
  CHECK( this == &subStruct, "Trying to init self as sub-structure" );

  for( UInt i = 0; i < subStruct.area.blocks.size(); i++ )
  {
    CHECKD( subStruct.area.blocks[i].size() != subArea.blocks[i].size(), "Trying to init sub-structure of incompatible size" );

    subStruct.area.blocks[i].pos() = subArea.blocks[i].pos();
  }

  if( parent )
  {
    // allow this to be false at the top level (need for edge CTU's)
    CHECKD( !area.contains( subStruct.area ), "Trying to init sub-structure not contained in the parent" );
  }

  subStruct.parent    = this;
  subStruct.chType    = _chType == MAX_NUM_CHANNEL_TYPE ? chType : _chType;
  subStruct.picture   = picture;

  subStruct.sps       = sps;
  subStruct.vps       = vps;
  subStruct.pps       = pps;
  subStruct.slice     = slice;
  subStruct.prevQP    = prevQP;
  subStruct.pcv       = pcv;

  subStruct.m_isTuEnc = isTuEnc;

  subStruct.initStructData( currQP, isLossless );

  if( isTuEnc )
  {
    CHECKD( area != subStruct.area, "Trying to init sub-structure for TU-encoding of incompatible size" );

    for( const auto &pcu : cus )
    {
      CodingUnit &cu = subStruct.addCU( *pcu );

      cu = *pcu;
    }

    for( const auto &ppu : pus )
    {
      PredictionUnit &pu = subStruct.addPU( *ppu );

      pu = *ppu;
    }

    unsigned numComp = ::getNumberValidChannels( area.chromaFormat );
    for( unsigned i = 0; i < numComp; i++)
    {
      ::memcpy( subStruct.m_isDecomp[i], m_isDecomp[i], ( unitScale[i].scale( area.blocks[i].size() ).area() * sizeof( Bool ) ) );
    }
  }
}

void CodingStructure::useSubStructure( const CodingStructure& subStruct, const UnitArea &subArea, const bool cpyPred /*= true*/, const bool cpyReco /*= true*/, const bool cpyOrgResi /*= true*/, const bool cpyResi /*= true*/ )
{
  fracBits += subStruct.fracBits;
  dist     += subStruct.dist;
  cost     += subStruct.cost;

  if( parent )
  {
    // allow this to be false at the top level
    CHECKD( !area.contains( subArea ), "Trying to use a sub-structure not contained in self" );
  }

  // copy the CUs over
  if( subStruct.m_isTuEnc )
  {
    // don't copy if the substruct was created for encoding of the TUs
  }
  else
  {
    for( const auto &pcu : subStruct.traverseCUs( subArea ) )
    {
      // add an analogue CU into own CU store
      const UnitArea &cuPatch = pcu;

      CodingUnit &cu = addCU( cuPatch );

      // copy the CU info from subPatch
      cu = pcu;
    }
  }

  // copy the PUs over
  if( subStruct.m_isTuEnc )
  {
    // don't copy if the substruct was created for encoding of the TUs
  }
  else
  {
    for( const auto &ppu : subStruct.traversePUs( subArea ) )
    {
      // add an analogue PU into own PU store
      const UnitArea &puPatch = ppu;

      PredictionUnit &pu = addPU( puPatch );

      // copy the PU info from subPatch
      pu = ppu;
    }
  }
  // copy the TUs over
  for( const auto &ptu : subStruct.traverseTUs( subArea ) )
  {
    // add an analogue TU into own TU store
    const UnitArea &tuPatch = ptu;

    TransformUnit &tu = addTU( tuPatch );

    // copy the TU info from subPatch
    tu = ptu;
  }

  UnitArea clippedArea = clipArea( subArea, area );

  setDecomp( clippedArea );

  CPelUnitBuf subPredBuf = cpyPred ? subStruct.getPredBuf( clippedArea ) : CPelUnitBuf();
  CPelUnitBuf subResiBuf = cpyResi ? subStruct.getResiBuf( clippedArea ) : CPelUnitBuf();
  CPelUnitBuf subRecoBuf = cpyReco ? subStruct.getRecoBuf( clippedArea ) : CPelUnitBuf();

  if( parent )
  {
    // copy data to picture
    if( cpyPred )    getPredBuf   ( clippedArea ).copyFrom( subPredBuf );
    if( cpyResi )    getResiBuf   ( clippedArea ).copyFrom( subResiBuf );
    if( cpyReco )    getRecoBuf   ( clippedArea ).copyFrom( subRecoBuf );
    if( cpyOrgResi ) getOrgResiBuf( clippedArea ).copyFrom( subStruct.getOrgResiBuf( clippedArea ) );
  }

  if( cpyPred ) picture->getPredBuf( clippedArea ).copyFrom( subPredBuf );
  if( cpyResi ) picture->getResiBuf( clippedArea ).copyFrom( subResiBuf );
  if( cpyReco ) picture->getRecoBuf( clippedArea ).copyFrom( subRecoBuf );

  if( !subStruct.m_isTuEnc && !slice->isIntra() )
  {
    // copy motion buffer
    MotionBuf ownMB  = getMotionBuf          ( clippedArea );
    CMotionBuf subMB = subStruct.getMotionBuf( clippedArea );

    ownMB.copyFrom( subMB );
  }
}

void CodingStructure::copyStructure( const CodingStructure& other, const bool copyTUs, const bool copyRecoBuf )
{
  fracBits = other.fracBits;
  dist     = other.dist;
  cost     = other.cost;

  CHECKD( area != other.area, "Incompatible sizes" );

  // copy the CUs over
  for (const auto &pcu : other.cus)
  {
    // add an analogue CU into own CU store
    const UnitArea &cuPatch = *pcu;

    CodingUnit &cu = addCU(cuPatch);

    // copy the CU info from subPatch
    cu = *pcu;
  }

  // copy the PUs over
  for (const auto &ppu : other.pus)
  {
    // add an analogue PU into own PU store
    const UnitArea &puPatch = *ppu;

    PredictionUnit &pu = addPU(puPatch);

    // copy the PU info from subPatch
    pu = *ppu;
  }

  if( !other.slice->isIntra() )
  {
    // copy motion buffer
    MotionBuf  ownMB = getMotionBuf();
    CMotionBuf subMB = other.getMotionBuf();

    ownMB.copyFrom( subMB );
  }

  if( copyTUs )
  {
    // copy the TUs over
    for( const auto &ptu : other.tus )
    {
      // add an analogue TU into own TU store
      const UnitArea &tuPatch = *ptu;

      TransformUnit&tu = addTU( tuPatch );

      // copy the TU info from subPatch
      tu = *ptu;
    }
  }

  if( copyRecoBuf )
  {
    CPelUnitBuf recoBuf = other.getRecoBuf( area );

    if( parent )
    {
      // copy data to self for neighbors
      getRecoBuf( area ).copyFrom( recoBuf );
    }

    // copy data to picture
    picture->getRecoBuf( area ).copyFrom( recoBuf );
  }
}

void CodingStructure::initStructData( const int &QP, const bool &_isLosses )
{
  m_cuCache.cache( cus );
  m_puCache.cache( pus );
  m_tuCache.cache( tus );

  m_numCUs = m_numPUs = m_numTUs = 0;

  UInt numCh = ::getNumberValidChannels( area.chromaFormat );
  for( UInt i = 0; i < numCh; i++ )
  {
    size_t _area = ( area.blocks[ i ].area() >> unitScale[ i ].area );

    memset( m_isDecomp[ i ], false, sizeof( *m_isDecomp[ 0 ] ) * _area );

    memset( m_tuIdx[ i ], 0, sizeof( *m_tuIdx[ 0 ] ) * _area );

    if( !m_isTuEnc )
    {
      memset( m_puIdx[ i ], 0, sizeof( *m_puIdx[ 0 ] ) * _area );
      memset( m_cuIdx[ i ], 0, sizeof( *m_cuIdx[ 0 ] ) * _area );
    }
  }

  numCh = getNumberValidComponents( area.chromaFormat );
  for( UInt i = 0; i < numCh; i++ )
  {
    m_offsets[ i ] = 0;
  }

  if( QP >= 0 )
  {
    currQP     = QP;
    isLossless = _isLosses;
  }

  if( !parent || ( ( slice->getSliceType() != I_SLICE ) && !m_isTuEnc ) )
  {
    getMotionBuf()      .memset( 0 );
    if( !parent )
    {
      getMotionBufFRUC().memset( 0 );
    }
  }

  fracBits = 0;
  dist     = 0;
  cost     = MAX_DOUBLE;
  interHad = MAX_UINT;
}

void CodingStructure::clearTUs()
{
  m_tuCache.cache( tus );
  m_numTUs = 0;

  UInt numCh = getNumberValidComponents( area.chromaFormat );
  for( UInt i = 0; i < numCh; i++ )
  {
    m_offsets[i] = 0;
  }

  for( auto &pcu : cus )
  {
    pcu->firstTU = pcu->lastTU = nullptr;
  }
}

void CodingStructure::clearPUs()
{
  m_puCache.cache( pus );
  m_numPUs = 0;

  for( auto &pcu : cus )
  {
    pcu->firstPU = pcu->lastPU = nullptr;
  }
}

void CodingStructure::clearCUs()
{
  m_cuCache.cache( cus );
  m_numCUs = 0;
}

MotionBuf CodingStructure::getMotionBuf( const Area& _area )
{
  const CompArea& _luma = area.Y();

  CHECKD( !_luma.contains( _area ), "Trying to access motion information outside of this coding structure" );

  const Area miArea   = g_miScaling.scale( _area );
  const Area selfArea = g_miScaling.scale( _luma );

  return MotionBuf( m_motionBuf + rsAddr( miArea.pos(), selfArea.pos(), selfArea.width ), selfArea.width, miArea.size() );
}

const CMotionBuf CodingStructure::getMotionBuf( const Area& _area ) const
{
  const CompArea& _luma = area.Y();

  CHECKD( !_luma.contains( _area ), "Trying to access motion information outside of this coding structure" );

  const Area miArea   = g_miScaling.scale( _area );
  const Area selfArea = g_miScaling.scale( _luma );

  return MotionBuf( m_motionBuf + rsAddr( miArea.pos(), selfArea.pos(), selfArea.width ), selfArea.width, miArea.size() );
}

MotionInfo& CodingStructure::getMotionInfo( const Position& pos )
{
  CHECKD( !area.Y().contains( pos ), "Trying to access motion information outside of this coding structure" );

  //return getMotionBuf().at( g_miScaling.scale( pos - area.lumaPos() ) );
  // bypass the motion buf calling and get the value directly
  const unsigned stride = g_miScaling.scaleHor( area.lumaSize().width );
  const Position miPos  = g_miScaling.scale( pos - area.lumaPos() );

  return *( m_motionBuf + miPos.y * stride + miPos.x );
}

const MotionInfo& CodingStructure::getMotionInfo( const Position& pos ) const
{
  CHECKD( !area.Y().contains( pos ), "Trying to access motion information outside of this coding structure" );

  //return getMotionBuf().at( g_miScaling.scale( pos - area.lumaPos() ) );
  // bypass the motion buf calling and get the value directly
  const unsigned stride = g_miScaling.scaleHor( area.lumaSize().width );
  const Position miPos  = g_miScaling.scale( pos - area.lumaPos() );

  return *( m_motionBuf + miPos.y * stride + miPos.x );
}

MotionBuf CodingStructure::getMotionBufFRUC( const Area& _area )
{
  if( parent )
  {
    // redirect to the top-level CS
    return picture->cs->getMotionBufFRUC( _area );
  }

  const CompArea& _luma = area.Y();

  CHECKD( !_luma.contains( _area ), "Trying to access motion information outside of this coding structure" );

  const Area miArea   = g_miScaling.scale( _area );
  const Area selfArea = g_miScaling.scale( _luma );

  return MotionBuf( m_motionBufFRUC + rsAddr( miArea.pos(), selfArea.pos(), selfArea.width ), selfArea.width, miArea.size() );
}

const CMotionBuf CodingStructure::getMotionBufFRUC( const Area& _area ) const
{
  if( parent )
  {
    // redirect to the top-level CS
    return picture->cs->getMotionBufFRUC( _area );
  }

  const CompArea& _luma = area.Y();

  CHECKD( !_luma.contains( _area ), "Trying to access motion information outside of this coding structure" );

  const Area miArea   = g_miScaling.scale( _area );
  const Area selfArea = g_miScaling.scale( _luma );

  return MotionBuf( m_motionBufFRUC + rsAddr( miArea.pos(), selfArea.pos(), selfArea.width ), selfArea.width, miArea.size() );
}

MotionInfo& CodingStructure::getMotionInfoFRUC( const Position& pos )
{
  if( parent )
  {
    // redirect to the top-level CS
    return picture->cs->getMotionInfoFRUC( pos );
  }

  CHECKD( !area.Y().contains( pos ), "Trying to access motion information outside of this coding structure" );

  //return getMotionBuf().at( g_miScaling.scale( pos - area.lumaPos() ) );
  // bypass the motion buf calling and get the value directly
  const unsigned stride = g_miScaling.scaleHor( area.lumaSize().width );
  const Position miPos  = g_miScaling.scale( pos - area.lumaPos() );

  return *( m_motionBufFRUC + miPos.y * stride + miPos.x );
}

const MotionInfo& CodingStructure::getMotionInfoFRUC( const Position& pos ) const
{
  if( parent )
  {
    // redirect to the top-level CS
    return picture->cs->getMotionInfoFRUC( pos );
  }

  CHECKD( !area.Y().contains( pos ), "Trying to access motion information outside of this coding structure" );

  //return getMotionBuf().at( g_miScaling.scale( pos - area.lumaPos() ) );
  // bypass the motion buf calling and get the value directly
  const unsigned stride = g_miScaling.scaleHor( area.lumaSize().width );
  const Position miPos  = g_miScaling.scale( pos - area.lumaPos() );

  return *( m_motionBufFRUC + miPos.y * stride + miPos.x );
}

// data accessors
       PelBuf     CodingStructure::getPredBuf(const CompArea &blk)           { return getBuf(blk,  PIC_PREDICTION); }
const CPelBuf     CodingStructure::getPredBuf(const CompArea &blk)     const { return getBuf(blk,  PIC_PREDICTION); }
       PelUnitBuf CodingStructure::getPredBuf(const UnitArea &unit)          { return getBuf(unit, PIC_PREDICTION); }
const CPelUnitBuf CodingStructure::getPredBuf(const UnitArea &unit)    const { return getBuf(unit, PIC_PREDICTION); }

       PelBuf     CodingStructure::getResiBuf(const CompArea &blk)           { return getBuf(blk,  PIC_RESIDUAL); }
const CPelBuf     CodingStructure::getResiBuf(const CompArea &blk)     const { return getBuf(blk,  PIC_RESIDUAL); }
       PelUnitBuf CodingStructure::getResiBuf(const UnitArea &unit)          { return getBuf(unit, PIC_RESIDUAL); }
const CPelUnitBuf CodingStructure::getResiBuf(const UnitArea &unit)    const { return getBuf(unit, PIC_RESIDUAL); }

       PelBuf     CodingStructure::getRecoBuf(const CompArea &blk)           { return getBuf(blk,  PIC_RECONSTRUCTION); }
const CPelBuf     CodingStructure::getRecoBuf(const CompArea &blk)     const { return getBuf(blk,  PIC_RECONSTRUCTION); }
       PelUnitBuf CodingStructure::getRecoBuf(const UnitArea &unit)          { return getBuf(unit, PIC_RECONSTRUCTION); }
const CPelUnitBuf CodingStructure::getRecoBuf(const UnitArea &unit)    const { return getBuf(unit, PIC_RECONSTRUCTION); }

       PelBuf     CodingStructure::getOrgResiBuf(const CompArea &blk)        { return getBuf(blk,  PIC_ORG_RESI); }
const CPelBuf     CodingStructure::getOrgResiBuf(const CompArea &blk)  const { return getBuf(blk,  PIC_ORG_RESI); }
       PelUnitBuf CodingStructure::getOrgResiBuf(const UnitArea &unit)       { return getBuf(unit, PIC_ORG_RESI); }
const CPelUnitBuf CodingStructure::getOrgResiBuf(const UnitArea &unit) const { return getBuf(unit, PIC_ORG_RESI); }

       PelBuf     CodingStructure::getOrgBuf(const CompArea &blk)            { return getBuf(blk,  PIC_ORIGINAL); }
const CPelBuf     CodingStructure::getOrgBuf(const CompArea &blk)      const { return getBuf(blk,  PIC_ORIGINAL); }
       PelUnitBuf CodingStructure::getOrgBuf(const UnitArea &unit)           { return getBuf(unit, PIC_ORIGINAL); }
const CPelUnitBuf CodingStructure::getOrgBuf(const UnitArea &unit)     const { return getBuf(unit, PIC_ORIGINAL); }

       PelBuf     CodingStructure::getOrgBuf(const ComponentID &compID)      { return picture->getBuf(area.blocks[compID], PIC_ORIGINAL); }
const CPelBuf     CodingStructure::getOrgBuf(const ComponentID &compID)const { return picture->getBuf(area.blocks[compID], PIC_ORIGINAL); }
       PelUnitBuf CodingStructure::getOrgBuf()                               { return picture->getBuf(area, PIC_ORIGINAL); }
const CPelUnitBuf CodingStructure::getOrgBuf()                         const { return picture->getBuf(area, PIC_ORIGINAL); }

PelBuf CodingStructure::getBuf( const CompArea &blk, const PictureType &type )
{
  if (!blk.valid())
  {
    return PelBuf();
  }

  if (type == PIC_ORIGINAL)
  {
    return picture->getBuf(blk, type);
  }

  const ComponentID compID = blk.compID;

  PelStorage* buf = type == PIC_PREDICTION ? &m_pred : ( type == PIC_RESIDUAL ? &m_resi : ( type == PIC_RECONSTRUCTION ? &m_reco : ( type == PIC_ORG_RESI ? &m_orgr : nullptr ) ) );

  CHECK( !buf, "Unknown buffer requested" );

      // no parent fetching for buffers
  CHECKD( !area.blocks[compID].contains(blk), "Buffer not contained in self requested" );

  CompArea cFinal = blk;
  cFinal.relativeTo( area.blocks[compID] );
  return buf->getBuf( cFinal );
}

const CPelBuf CodingStructure::getBuf( const CompArea &blk, const PictureType &type ) const
{
  if (!blk.valid())
  {
    return PelBuf();
  }

  if (type == PIC_ORIGINAL)
  {
    return picture->getBuf(blk, type);
  }

  const ComponentID compID = blk.compID;

  const PelStorage* buf = type == PIC_PREDICTION ? &m_pred : ( type == PIC_RESIDUAL ? &m_resi : ( type == PIC_RECONSTRUCTION ? &m_reco : ( type == PIC_ORG_RESI ? &m_orgr : nullptr ) ) );

  CHECK( !buf, "Unknown buffer requested" );

  CHECKD( !area.blocks[compID].contains( blk ), "Buffer not contained in self requested" );

  CompArea cFinal = blk;
  cFinal.relativeTo( area.blocks[compID] );
  return buf->getBuf( cFinal );
}

PelUnitBuf CodingStructure::getBuf( const UnitArea &unit, const PictureType &type )
{
  // no parent fetching for buffers
  if( area.chromaFormat == CHROMA_400 )
  {
    return PelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ) );
  }
  else
  {
    return PelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ), getBuf( unit.Cb(), type ), getBuf( unit.Cr(), type ) );
  }
}

const CPelUnitBuf CodingStructure::getBuf( const UnitArea &unit, const PictureType &type ) const
{
  // no parent fetching for buffers
  if( area.chromaFormat == CHROMA_400 )
  {
    return CPelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ) );
  }
  else
  {
    return CPelUnitBuf( area.chromaFormat, getBuf( unit.Y(), type ), getBuf( unit.Cb(), type ), getBuf( unit.Cr(), type ) );
  }
}

const CodingUnit* CodingStructure::getCURestricted( const Position &pos, const CodingUnit& curCu, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const CodingUnit* cu = getCU( pos, _chType );
  // exists       same slice and tile                  cu precedes curCu in encoding order
  //                                                  (thus, is either from parent CS in RD-search or its index is lower)
  if( cu && CU::isSameSliceAndTile( *cu, curCu ) && ( cu->cs != curCu.cs || cu->idx <= curCu.idx ) )
  {
    return cu;
  }
  else
  {
    return nullptr;
  }
}

const CodingUnit* CodingStructure::getCURestricted( const Position &pos, const unsigned curSliceIdx, const unsigned curTileIdx, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const CodingUnit* cu = getCU( pos, _chType );
  return ( cu && cu->slice->getIndependentSliceIdx() == curSliceIdx && cu->tileIdx == curTileIdx ) ? cu : nullptr;
}

const PredictionUnit* CodingStructure::getPURestricted( const Position &pos, const PredictionUnit& curPu, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const PredictionUnit* pu = getPU( pos, _chType );
  // exists       same slice and tile                  pu precedes curPu in encoding order
  //                                                  (thus, is either from parent CS in RD-search or its index is lower)
  if( pu && CU::isSameSliceAndTile( *pu->cu, *curPu.cu ) && ( pu->cs != curPu.cs || pu->idx <= curPu.idx ) )
  {
    return pu;
  }
  else
  {
    return nullptr;
  }
}

const TransformUnit* CodingStructure::getTURestricted( const Position &pos, const TransformUnit& curTu, const ChannelType _chType /*= MAX_NUM_CHANNEL_TYPE*/ ) const
{
  const TransformUnit* tu = getTU( pos, _chType );
  // exists       same slice and tile                  tu precedes curTu in encoding order
  //                                                  (thus, is either from parent CS in RD-search or its index is lower)
  if( tu && CU::isSameSliceAndTile( *tu->cu, *curTu.cu ) && ( tu->cs != curTu.cs || tu->idx <= curTu.idx ) )
  {
    return tu;
  }
  else
  {
    return nullptr;
  }
}

