/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2015, ITU/ISO/IEC
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

/** \file     EncAdaptiveLoopFilter.cpp
 \brief    estimation part of adaptive loop filter class
 */

#include "EncAdaptiveLoopFilter.h"

#include "dtrace_codingstruct.h"
#include "UnitTools.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// ====================================================================================================================
// Constants
// ====================================================================================================================


#define ALF_NUM_OF_REDESIGN 1

// ====================================================================================================================
// Tables
// ====================================================================================================================

const Int EncAdaptiveLoopFilter::m_aiSymmetricArray9x9[81] =
{
   0,  1,  2,  3,  4,  5,  6,  7,  8,
   9, 10, 11, 12, 13, 14, 15, 16, 17,
  18, 19, 20, 21, 22, 23, 24, 25, 26,
  27, 28, 29, 30, 31, 32, 33, 34, 35,
  36, 37, 38, 39, 40, 39, 38, 37, 36,
  35, 34, 33, 32, 31, 30, 29, 28, 27,
  26, 25, 24, 23, 22, 21, 20, 19, 18,
  17, 16, 15, 14, 13, 12, 11, 10,  9,
   8,  7,  6,  5,  4,  3,  2,  1,  0
};

const Int EncAdaptiveLoopFilter::m_aiSymmetricArray7x7[49] =
{
  0,  1,  2,  3,  4,  5,  6,
  7,  8,  9, 10, 11, 12, 13,
  14, 15, 16, 17, 18, 19, 20,
  21, 22, 23, 24, 23, 22, 21,
  20, 19, 18, 17, 16, 15, 14,
  13, 12, 11, 10,  9,  8,  7,
  6,  5,  4,  3,  2,  1,  0,
};

const Int EncAdaptiveLoopFilter::m_aiSymmetricArray5x5[25] =
{
  0,  1,  2,  3,  4,
  5,  6,  7,  8,  9,
  10, 11, 12, 11, 10,
  9,  8,  7,  6,  5,
  4,  3,  2,  1,  0,
};

const Int EncAdaptiveLoopFilter::m_aiSymmetricArray9x7[63] =
{
   0,  1,  2,  3,  4,  5,  6,  7,  8,
   9, 10, 11, 12, 13, 14, 15, 16, 17,
  18, 19, 20, 21, 22, 23, 24, 25, 26,
  27, 28, 29, 30, 31, 30, 29, 28, 27,
  26, 25, 24, 23, 22, 21, 20, 19, 18,
  17, 16, 15, 14, 13, 12, 11, 10,  9,
   8,  7,  6,  5,  4,  3,  2,  1,  0
};

const Int EncAdaptiveLoopFilter::m_aiTapPos9x9_In9x9Sym[21] =
{
                  0,  1,  2,
              3,  4,  5,  6,  7,
          8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20
};

const Int EncAdaptiveLoopFilter::m_aiTapPos7x7_In9x9Sym[14] =
{
                  1,
              4,  5,  6,
          9, 10, 11, 12, 13,
     16, 17, 18, 19, 20
};

const Int EncAdaptiveLoopFilter::m_aiTapPos5x5_In9x9Sym[8]  =
{

            5,
       10, 11, 12,
   17, 18, 19, 20
};

const Int* EncAdaptiveLoopFilter::m_iTapPosTabIn9x9Sym[m_NO_TEST_FILT] =
{
  m_aiTapPos9x9_In9x9Sym, m_aiTapPos7x7_In9x9Sym, m_aiTapPos5x5_In9x9Sym
};

// ====================================================================================================================
// Constructor / destructor
// ====================================================================================================================



EncAdaptiveLoopFilter::EncAdaptiveLoopFilter()
{
  m_eSliceType          = I_SLICE;
  m_iPicNalReferenceIdc = 0;

  m_pcBestAlfParam = nullptr;
  m_pcTempAlfParam = nullptr;

  m_EGlobalSym    = nullptr;
  m_yGlobalSym    = nullptr;
  m_pixAcc        = nullptr;
  m_E_temp        = nullptr;
  m_y_temp        = nullptr;
  m_E_merged      = nullptr;
  m_y_merged      = nullptr;
  m_pixAcc_merged = nullptr;
  m_varImg        = nullptr;

  m_filterCoeff          = nullptr;
  m_pdDoubleAlfCoeff     = nullptr;
  m_filterCoeffQuantMod  = nullptr;
  m_filterCoeffQuant     = nullptr;
  m_diffFilterCoeffQuant = nullptr;
  m_FilterCoeffQuantTemp = nullptr;
  m_filterCoeffSymQuant  = nullptr;

  m_ppdAlfCorr     = nullptr;
  m_CABACEstimator = nullptr;
  m_CABACEncoder   = nullptr;
  m_pSlice         = nullptr;

  m_dLambdaLuma   = 0;
  m_dLambdaChroma = 0;
  m_isDec         = false;
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

Void EncAdaptiveLoopFilter::create( const Int iPicWidth, Int iPicHeight, const ChromaFormat chromaFormatIDC, const Int uiMaxCUWidth, const UInt uiMaxCUHeight, const UInt uiMaxCUDepth, const Int nInputBitDepth, const Int nInternalBitDepth, const Int numberOfCTUs )
{
  AdaptiveLoopFilter::create( iPicWidth, iPicHeight, chromaFormatIDC, uiMaxCUWidth, uiMaxCUHeight, uiMaxCUDepth, nInputBitDepth, nInternalBitDepth, numberOfCTUs );

  m_bestPelBuf.create( chromaFormatIDC, Area(0, 0, iPicWidth, iPicHeight ), uiMaxCUWidth, 0, 0, false );
  m_tempPelBuf.create( chromaFormatIDC, Area(0, 0, iPicWidth, iPicHeight ), uiMaxCUWidth, 0, 0, false );

  m_maskBuf.buf = (UInt *) calloc( iPicWidth*iPicHeight, sizeof(UInt ) );
  m_maskBuf.width   = iPicWidth;
  m_maskBuf.height  = iPicHeight;
  m_maskBuf.stride  = iPicWidth;

  m_maskBestBuf.buf = (UInt *)calloc(iPicWidth*iPicHeight, sizeof(UInt));
  m_maskBestBuf.width = iPicWidth;
  m_maskBestBuf.height = iPicHeight;
  m_maskBestBuf.stride = iPicWidth;


  m_varImg          = m_varImgMethods;

  m_pcBestAlfParam = new ALFParam;
  m_pcTempAlfParam = new ALFParam;
  allocALFParam(m_pcBestAlfParam);
  allocALFParam(m_pcTempAlfParam);

  // init qc_filter
  initMatrix4D_double( &m_EGlobalSym, m_NO_TEST_FILT,  m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH, m_MAX_SQR_FILT_LENGTH);
  initMatrix3D_double( &m_yGlobalSym, m_NO_TEST_FILT, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
  m_pixAcc = (double *) calloc(m_NO_VAR_BINS, sizeof(double));

  initMatrix_double( &m_E_temp, m_MAX_SQR_FILT_LENGTH, m_MAX_SQR_FILT_LENGTH);//
  m_y_temp = (double *) calloc(m_MAX_SQR_FILT_LENGTH, sizeof(double));//
  initMatrix3D_double(&m_E_merged, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH, m_MAX_SQR_FILT_LENGTH);//
  initMatrix_double(&m_y_merged, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH); //
  m_pixAcc_merged = (double *) calloc(m_NO_VAR_BINS, sizeof(double));//

  initMatrix_int(&m_filterCoeffSymQuant, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
  m_filterCoeffQuantMod = (int *) calloc(m_MAX_SQR_FILT_LENGTH, sizeof(int));//
  m_filterCoeff = (double *) calloc(m_MAX_SQR_FILT_LENGTH, sizeof(double));//
  m_filterCoeffQuant = (int *) calloc(m_MAX_SQR_FILT_LENGTH, sizeof(int));//
  initMatrix_int(&m_diffFilterCoeffQuant, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);//
  initMatrix_int(&m_FilterCoeffQuantTemp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);//
#if JVET_C0038_NO_PREV_FILTERS
  initMatrix_int(&m_imgY_preFilter, iPicHeight, iPicWidth);
#endif
}



Void EncAdaptiveLoopFilter::init( CodingStructure& cs, CABACEncoder* cabacEncoder )
{
  m_CABACEstimator = cabacEncoder->getCABACEstimator( cs.slice->getSPS() );
  m_CABACEncoder   = cabacEncoder;
  m_eSliceType = cs.slice->getSliceType();
  m_iPicNalReferenceIdc = cs.picture->referenced ? 1 :0;

  // copy the clip ranges
  m_clpRngs = cs.slice->clpRngs();
  m_isGALF  = cs.sps->getSpsNext().getGALFEnabled();

  xInitParam();
}

Void EncAdaptiveLoopFilter::destroy()
{
  if( !m_wasCreated )
    return;

  xUninitParam();

  m_bestPelBuf.destroy();
  m_tempPelBuf.destroy();

  free( m_maskBuf.buf );
  m_maskBuf.buf = nullptr;

  free(m_maskBestBuf.buf);
  m_maskBestBuf.buf = nullptr;

  m_CABACEstimator = NULL;
  m_CABACEncoder   = nullptr;

  freeALFParam(m_pcBestAlfParam);
  freeALFParam(m_pcTempAlfParam);
  delete m_pcBestAlfParam;
  delete m_pcTempAlfParam;
  m_pcBestAlfParam = nullptr;
  m_pcTempAlfParam = nullptr;

  // delete qc filters
  destroyMatrix4D_double(m_EGlobalSym, m_NO_TEST_FILT,  m_NO_VAR_BINS);
  destroyMatrix3D_double(m_yGlobalSym, m_NO_TEST_FILT);
  destroyMatrix_int(m_filterCoeffSymQuant);

  free(m_pixAcc);

  destroyMatrix3D_double(m_E_merged, m_NO_VAR_BINS);
  destroyMatrix_double(m_y_merged);
  destroyMatrix_double(m_E_temp);
  free(m_pixAcc_merged);

  free(m_filterCoeffQuantMod);
  free(m_y_temp);

  free(m_filterCoeff);
  free(m_filterCoeffQuant);
  destroyMatrix_int(m_diffFilterCoeffQuant);
  destroyMatrix_int(m_FilterCoeffQuantTemp);
#if JVET_C0038_NO_PREV_FILTERS
  destroyMatrix_int(m_imgY_preFilter);
#endif
  AdaptiveLoopFilter::destroy();
}

/**
 \param pcAlfParam           ALF parameter
 \param dLambda              lambda value for RD cost computation
 \retval ruiDist             distortion
 \retval ruiBits             required bits
 \retval ruiMaxAlfCtrlDepth  optimal partition depth
 */
Void EncAdaptiveLoopFilter::ALFProcess(CodingStructure& cs, ALFParam* pcAlfParam, Double dLambdaLuma, Double dLambdaChroma )
{
#if COM16_C806_ALF_TEMPPRED_NUM
  const Int tidx = cs.slice->getTLayer();
  CHECK( tidx >= E0104_ALF_MAX_TEMPLAYERID, " index out of range");
  ALFParam *pcStoredAlfPara = cs.slice->isIntra() ? NULL : m_acStoredAlfPara[tidx];
  Int iStoredAlfParaNum = m_storedAlfParaNum[tidx];
#endif
  CHECK( cs.pcv->lumaHeight != m_img_height     , "wrong parameter set" )
  CHECK( cs.pcv->lumaWidth  != m_img_width      , "wrong parameter set" )
  CHECK( cs.pcv->sizeInCtus != m_uiNumCUsInFrame, "wrong parameter set" )

  Int tap, num_coef;

  // set global variables
  tap      = m_ALF_MAX_NUM_TAP;
  Int tapV = AdaptiveLoopFilter::ALFTapHToTapV(tap);
  num_coef = (tap * tapV + 1) >> 1;
  num_coef = num_coef + (m_isGALF?0:1); // DC offset

  resetALFParam( pcAlfParam );
  resetALFParam( m_pcBestAlfParam );
  resetALFParam( m_pcTempAlfParam );

  if( m_isGALF )
    xInitFixedFilters();

  // set lambda
  m_dLambdaLuma   = dLambdaLuma;
  m_dLambdaChroma = dLambdaChroma;
  m_pSlice        = cs.slice;
  const PelUnitBuf orgUnitBuf = cs.getOrgBuf();

  PelUnitBuf recUnitBuf = cs.getRecoBuf();

  m_tmpRecExtBuf.copyFrom( recUnitBuf );
  PelUnitBuf recExtBuf = m_tmpRecExtBuf.getBuf( cs.area );
  recExtBuf.extendBorderPel( m_FILTER_LENGTH >> 1 );

  const PelUnitBuf cRecExtBuf = recExtBuf;
  // set min cost
  UInt64 uiMinRate = MAX_INT;
  UInt64 uiMinDist = MAX_INT;
  Double dMinCost  = MAX_DOUBLE;

  UInt64 ruiBits = MAX_INT;
  UInt64 ruiDist = MAX_INT;

  UInt64  uiOrigRate;
  UInt64  uiOrigDist;
  Double  dOrigCost;

  // calc original cost
  xCalcRDCostLuma( orgUnitBuf, cRecExtBuf, NULL, uiOrigRate, uiOrigDist, dOrigCost );

  m_pcBestAlfParam->alf_flag        = 0;
  m_pcBestAlfParam->cu_control_flag = 0;

  // initialize temp_alfps
  m_pcTempAlfParam->alf_flag        = 1;
  m_pcTempAlfParam->tapH            = tap;
  m_pcTempAlfParam->tapV            = tapV;
  m_pcTempAlfParam->num_coeff       = num_coef;
  m_pcTempAlfParam->chroma_idc      = 0;
  m_pcTempAlfParam->cu_control_flag = 0;


  // adaptive in-loop wiener filtering
  xEncALFLuma( orgUnitBuf, cRecExtBuf, recUnitBuf, uiMinRate, uiMinDist, dMinCost, cs.slice );

  if( !m_isGALF || !m_pSlice->isIntra())
  // cu-based filter on/off control
  xCheckCUAdaptation( cs, orgUnitBuf, cRecExtBuf, recUnitBuf, uiMinRate, uiMinDist, dMinCost );

  // adaptive tap-length
  xFilterTypeDecision( cs, orgUnitBuf, recExtBuf, recUnitBuf, uiMinRate, uiMinDist, dMinCost, cs.slice);

  // compute RD cost
  xCalcRDCostLuma( orgUnitBuf, recUnitBuf, m_pcBestAlfParam, uiMinRate, uiMinDist, dMinCost );

  // compare RD cost to non-ALF case
  if( dMinCost < dOrigCost )
  {
    m_pcBestAlfParam->alf_flag = 1;
    ruiBits = uiMinRate;
    ruiDist = uiMinDist;
  }
  else
  {
    m_pcBestAlfParam->alf_flag        = 0;
    m_pcBestAlfParam->cu_control_flag = 0;

    uiMinRate = uiOrigRate;
    uiMinDist = uiOrigDist;
    dMinCost  = dOrigCost;
    recUnitBuf.get( COMPONENT_Y ).copyFrom( recExtBuf.get( COMPONENT_Y ) );

    ruiBits = uiOrigRate;
    ruiDist = uiOrigDist;
  }

  // if ALF works
  if( m_pcBestAlfParam->alf_flag )
  {
    // do additional ALF process for chroma
    xEncALFChroma( uiMinRate, orgUnitBuf, recExtBuf, recUnitBuf, ruiDist, ruiBits , cs.slice );
  }
  // copy to best storage
  copyALFParam( pcAlfParam, m_pcBestAlfParam);
#if COM16_C806_ALF_TEMPPRED_NUM
  if (pcStoredAlfPara != NULL && iStoredAlfParaNum > 0)
  {
    m_bestPelBuf.copyFrom(recUnitBuf);
    //test stored ALF coefficients of both luma and chroma.
    for (UInt iAlfIdx = 0; iAlfIdx < iStoredAlfParaNum && iAlfIdx < C806_ALF_TEMPPRED_NUM; iAlfIdx++)
    {
      recUnitBuf.copyFrom(recExtBuf);
      ALFParam& pcStoredAlf = pcStoredAlfPara[iAlfIdx];

      if (xFilteringLumaChroma(cs, &pcStoredAlf, orgUnitBuf, recExtBuf, recUnitBuf, ruiBits, ruiDist, dMinCost, iAlfIdx, cs.slice))
      {
        pcAlfParam->temporalPredFlag = false;
        copyALFParam(pcAlfParam, m_pcBestAlfParam);
        pcAlfParam->temporalPredFlag = true;
        pcAlfParam->prevIdx = iAlfIdx;
        if (m_pcBestAlfParam->cu_control_flag)
        {
          pcAlfParam->alf_max_depth = m_pcBestAlfParam->alf_max_depth;
        }
      }
    }
    recUnitBuf.copyFrom(m_bestPelBuf);
  }
#endif
}


// ====================================================================================================================
// LUMA
// ====================================================================================================================
Void EncAdaptiveLoopFilter::xEncALFLuma( const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf, UInt64& ruiMinRate, UInt64& ruiMinDist, Double& rdMinCost, const Slice* pSlice)
{
  m_updateMatrix = true;
#if JVET_C0038_NO_PREV_FILTERS
  bFindBestFixedFilter = false;
#endif

  AlfFilterType filtType = ALF_FILTER_SYM_9;

  ALFParam   cFrmAlfParam;
  ALFParam* pcAlfParam = NULL;

  pcAlfParam = &(cFrmAlfParam);
  allocALFParam(pcAlfParam);
  resetALFParam(pcAlfParam);

  pcAlfParam->alf_flag          = 1;
  pcAlfParam->chroma_idc        = 0;
  pcAlfParam->cu_control_flag   = 0;
  pcAlfParam->filterType        = filtType;
  pcAlfParam->tapH              = AdaptiveLoopFilter::m_FilterTapsOfType[ filtType ];
  pcAlfParam->tapV              = AdaptiveLoopFilter::ALFTapHToTapV( pcAlfParam->tapH);
  pcAlfParam->num_coeff         = AdaptiveLoopFilter::ALFTapHToNumCoeff(pcAlfParam->tapH);
  pcAlfParam->filters_per_group = AdaptiveLoopFilter::m_NO_FILTERS;
  pcAlfParam->forceCoeff0       = 0;
  pcAlfParam->predMethod        = 0;

  xSetInitialMask( recExtBuf.get(COMPONENT_Y) );
  xStoreInBlockMatrix(orgUnitBuf, recExtBuf, filtType);
//  xFindFilterCoeffsLuma(orgUnitBuf, recExtBuf, filtType);

  if( m_isGALF )
  {
    xCheckFilterMergingGalf(cFrmAlfParam
  #if JVET_C0038_NO_PREV_FILTERS
      , orgUnitBuf, recExtBuf
  #endif
      );
    xFilterFrame_enGalf(m_tempPelBuf, recExtBuf, filtType
  #if COM16_C806_ALF_TEMPPRED_NUM
      , &cFrmAlfParam, true
  #endif
      , pSlice->clpRng( COMPONENT_Y )
      );
  }
  else
  {
    xCheckFilterMergingAlf(cFrmAlfParam
  #if JVET_C0038_NO_PREV_FILTERS
      , orgUnitBuf, recExtBuf
  #endif
      );
    xFilterFrame_enAlf(m_tempPelBuf, recExtBuf, filtType
  #if COM16_C806_ALF_TEMPPRED_NUM
      , &cFrmAlfParam, true
  #endif
      , pSlice->clpRng( COMPONENT_Y )
      );
  }


  PelBuf recLuma = recUnitBuf.get(COMPONENT_Y);
  const CPelBuf tmpLuma = m_tempPelBuf.get(COMPONENT_Y);
  recLuma.copyFrom( tmpLuma );

  copyALFParam( m_pcBestAlfParam, &cFrmAlfParam);

  xCalcRDCostLuma( orgUnitBuf,  m_tempPelBuf, pcAlfParam, ruiMinRate, ruiMinDist, rdMinCost );
  freeALFParam(&cFrmAlfParam);
}


// ====================================================================================================================
// CHROMA
// ====================================================================================================================

Void EncAdaptiveLoopFilter::xstoreInBlockMatrixForChroma(const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, Int tap, Int chroma_idc)
{
  Pel* ImgOrg;
  Pel* ImgDec;
  Int i, j, k, l, varInd = 0, ii, jj;
  Int x, y, yLocal;
  Int fl = tap / 2;
  Int flV = AdaptiveLoopFilter::ALFFlHToFlV(fl);
  Int sqrFiltLength = AdaptiveLoopFilter::ALFTapHToNumCoeff(tap);
  Int fl2 = 9 / 2; //extended size at each side of the frame
  Int ELocal[m_MAX_SQR_FILT_LENGTH];
  Int filtType = 0; //for chroma
  Int iImgHeight = recExtBuf.get(COMPONENT_Cb).height;
  Int iImgWidth = recExtBuf.get(COMPONENT_Cb).width;
  Double **E, *yy;

  const Int *p_pattern = m_patternTab[filtType];

  memset(m_pixAcc, 0, sizeof(Double)*m_NO_VAR_BINS);
  {
    memset(m_yGlobalSym[filtType][varInd], 0, sizeof(Double)*m_MAX_SQR_FILT_LENGTH);
    for (k = 0; k<sqrFiltLength; k++)
    {
      memset(m_EGlobalSym[filtType][varInd][k], 0, sizeof(Double)*m_MAX_SQR_FILT_LENGTH);
    }
  }
  for (Int iColorIdx = 0; iColorIdx < 2; iColorIdx++)
  {
    if ((iColorIdx == 0 && chroma_idc < 2) || (iColorIdx == 1 && (chroma_idc & 0x01) == 0))
    {
      continue;
    }
    ImgOrg = (Pel*)orgUnitBuf.get(ComponentID(iColorIdx + 1)).buf;
    ImgDec = (Pel*)recExtBuf.get(ComponentID(iColorIdx + 1)).buf;
    Int Stride = recExtBuf.get(ComponentID(iColorIdx + 1)).stride;
    Int StrideO = orgUnitBuf.get(ComponentID(iColorIdx + 1)).stride;

    for (i = 0, y = fl2; i < iImgHeight; i++, y++)
    {
      for (j = 0, x = fl2; j < iImgWidth; j++, x++)
      {
        varInd = 0;
        k = 0;
        memset(ELocal, 0, sqrFiltLength*sizeof(Int));
        for (ii = -flV; ii < 0; ii++)
        {
          for (jj = -fl - ii; jj <= fl + ii; jj++)
          {
            ELocal[p_pattern[k++]] += (ImgDec[(i + ii)*Stride + (j + jj)] + ImgDec[(i - ii)*Stride + (j - jj)]);
          }
        }
        Int iOffset = i * Stride + j;
        Int iOffsetO = i * StrideO + j;
        for (jj = -fl; jj<0; jj++)
        {
          ELocal[p_pattern[k++]] += (ImgDec[iOffset + jj] + ImgDec[iOffset - jj]);
        }
        ELocal[p_pattern[k++]] += ImgDec[iOffset];
        yLocal = ImgOrg[iOffsetO];
        m_pixAcc[varInd] += (yLocal*yLocal);
        E = m_EGlobalSym[filtType][varInd];
        yy = m_yGlobalSym[filtType][varInd];
        for (k = 0; k<sqrFiltLength; k++)
        {
          for (l = k; l<sqrFiltLength; l++)
          {
            E[k][l] += (Double)(ELocal[k] * ELocal[l]);
          }
          yy[k] += (Double)(ELocal[k] * yLocal);
        }
      }
    }
  }
  // Matrix EGlobalSeq is symmetric, only part of it is calculated
  Double **pE = m_EGlobalSym[filtType][varInd];
  for (k = 1; k < sqrFiltLength; k++)
  {
    for (l = 0; l < k; l++)
    {
      pE[k][l] = pE[l][k];
    }
  }
}

#if JVET_C0038_NO_PREV_FILTERS
Double EncAdaptiveLoopFilter::xTestFixedFilterFast(Double ***A, Double **b, Double *pixAcc, Double *filterCoeffSym, Double *filterCoeffDefault, int varInd)
{

  Int j;
  Double seFilt = 0, seOrig = 0, error, filterCoeffTemp[m_MAX_SQR_FILT_LENGTH];
  Int sqrFiltLength = (m_MAX_SQR_FILT_LENGTH / 2 + 1);

  for (j = 0; j < sqrFiltLength; j++)
  {
    filterCoeffTemp[j] = filterCoeffSym[j] - filterCoeffDefault[j];
  }
  seOrig = pixAcc[varInd];
  seFilt = pixAcc[varInd] + xCalcErrorForGivenWeights(A[varInd], b[varInd], filterCoeffTemp, sqrFiltLength);

  error = 0;
  if (seFilt < seOrig)
  {
    error += seFilt;
  }
  else
  {
    error += seOrig;
  }
  return(error);
}

Double EncAdaptiveLoopFilter::xTestFixedFilter(const Pel *imgY_Rec, const Pel *imgY_org, const Pel* imgY_append, Int usePrevFilt[], Int noVarBins, Int orgStride, Int recStride, Int filtType)
{

  Int i, j, varInd, pixelInt, temp;
  Double seFilt[m_NO_VAR_BINS] ={0}, seOrig[m_NO_VAR_BINS] ={0}, error;
  Int fl = m_FILTER_LENGTH / 2;
  Int offset = (1 << (m_NUM_BITS - 2));
  const ClpRng clpRng = m_clpRngs.comp[ COMPONENT_Y ];

  for (i = fl; i < m_img_height + fl; i++)
  {
    for (j = fl; j < m_img_width + fl; j++)
    {
      if (m_maskBuf.at(j - fl, i - fl))
      {
        varInd = m_varImg[(i - fl)][(j - fl)];
        pixelInt = xFilterPixel(imgY_append, &varInd, m_filterCoeffFinal, NULL, i, j, fl, recStride, (AlfFilterType)filtType);
        pixelInt = ((pixelInt + offset) >> (m_NUM_BITS - 1));
        pixelInt = ClipPel( pixelInt, clpRng );

        Int iOffsetOrg = (i - fl)*orgStride + (j - fl);
        Int iOffsetRec = (i - fl)*recStride + (j - fl);
        temp = pixelInt - imgY_org[iOffsetOrg];
        seFilt[varInd] += temp*temp;
        temp = imgY_Rec[iOffsetRec] - imgY_org[iOffsetOrg];
        seOrig[varInd] += temp*temp;
      }
    }
  }

  error = 0;
  for (i = 0; i< noVarBins; i++)
  {
    if (seFilt[i] < seOrig[i])
    {
      usePrevFilt[i] = 1;
      error += seFilt[i];
    }
    else
    {
      usePrevFilt[i] = 0;
      error += seOrig[i];
    }
  }
  return(error);
}

Void EncAdaptiveLoopFilter::xPreFilterFr(Int** imgY_preFilter, const Pel* imgY_rec, const Pel * imgY_org, const Pel* imgY_append, Int usePrevFilt[], Int Stride, Int filtType)
{

  Int i, j;
  Int fl = m_FILTER_LENGTH / 2;

  Int temp = 0, pixelInt = 0, offset = (1 << (m_NUM_BITS - 2));
  const ClpRng clpRng = m_clpRngs.comp[ COMPONENT_Y ];

  for (i = fl; i < m_img_height + fl; i++)
  {
    for (j = fl; j < m_img_width + fl; j++)
    {
      Int varInd = m_varImg[(i - fl)][(j - fl)];
      Int varIndAfterMapping = selectTransposeVarInd(varInd, &temp);
      if (m_maskBuf.at(j - fl, i - fl) && usePrevFilt[varIndAfterMapping] > 0)
      {
        pixelInt = xFilterPixel(imgY_append, &varInd, m_filterCoeffFinal, NULL, i, j, fl, Stride, (AlfFilterType)filtType);
        pixelInt = ((pixelInt + offset) >> (m_NUM_BITS - 1));
        imgY_preFilter[(i - fl)][(j - fl)] = ClipPel( pixelInt, clpRng );
      }
      else
      {
        imgY_preFilter[(i - fl)][(j - fl)] = imgY_rec[(i - fl)*Stride + (j - fl)];
      }
    }
  }
}
#endif
Void EncAdaptiveLoopFilter::xEncALFChroma(UInt64 uiLumaRate, const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf, UInt64& ruiDist, UInt64& ruiBits, const Slice* pSlice)
{
  // restriction for non-referenced B-slice
  if( m_eSliceType == B_SLICE && m_iPicNalReferenceIdc == 0 )
  {
    return;
  }

  Int tap, num_coef;

  // set global variables
  tap         = m_ALF_MAX_NUM_TAP_C;
  if( m_isGALF )
  {
    num_coef = AdaptiveLoopFilter::ALFTapHToNumCoeff(tap);
  }
  else
  {
    num_coef    = (tap*tap+1)>>1;
    num_coef    = num_coef + 1; // DC offset
  }

  // set min cost
  UInt64 uiMinRate = uiLumaRate;
  UInt64 uiMinDist = MAX_INT;
  Double dMinCost  = MAX_DOUBLE;

  // calc original cost
  copyALFParam( m_pcTempAlfParam, m_pcBestAlfParam );
  xCalcRDCostChroma( orgUnitBuf, recUnitBuf, m_pcTempAlfParam, uiMinRate, uiMinDist, dMinCost );

  // initialize temp_alfps
  m_pcTempAlfParam->chroma_idc = 3;
  m_pcTempAlfParam->tap_chroma       = tap;
  m_pcTempAlfParam->num_coeff_chroma = num_coef;

  // Adaptive in-loop wiener filtering for chroma
  xFilteringFrameChroma(orgUnitBuf, recExtBuf, recUnitBuf );

  // filter on/off decision for chroma
  UInt64 uiFiltDistCb = xCalcSSD( orgUnitBuf, recUnitBuf, COMPONENT_Cb);
  UInt64 uiFiltDistCr = xCalcSSD( orgUnitBuf, recUnitBuf, COMPONENT_Cr);
  UInt64 uiOrgDistCb  = xCalcSSD( orgUnitBuf, recExtBuf, COMPONENT_Cb);
  UInt64 uiOrgDistCr  = xCalcSSD( orgUnitBuf, recExtBuf, COMPONENT_Cr);

  m_pcTempAlfParam->chroma_idc = 0;
  if(uiOrgDistCb > uiFiltDistCb)
  {
    m_pcTempAlfParam->chroma_idc += 2;
  }
  if(uiOrgDistCr  > uiFiltDistCr )
  {
    m_pcTempAlfParam->chroma_idc += 1;
  }

  if(m_pcTempAlfParam->chroma_idc )
  {
    if(m_pcTempAlfParam->chroma_idc !=3 )
    {
      // chroma filter re-design
      xFilteringFrameChroma( orgUnitBuf, recExtBuf, recUnitBuf );
    }

    UInt64 uiRate, uiDist;
    Double dCost;
    xCalcRDCostChroma( orgUnitBuf, recUnitBuf, m_pcTempAlfParam, uiRate, uiDist, dCost );

    if( dCost < dMinCost )
    {
      copyALFParam(m_pcBestAlfParam, m_pcTempAlfParam);
      if( ! m_isGALF )
      {
        predictALFCoeffChroma(m_pcBestAlfParam);
      }
      ruiBits += uiRate;
      ruiDist += uiDist;
    }
    else
    {
      m_pcBestAlfParam->chroma_idc = 0;
      if( (m_pcTempAlfParam->chroma_idc>>1) & 0x01 )
      {
        recUnitBuf.get(COMPONENT_Cb).copyFrom( recExtBuf.get(COMPONENT_Cb) );
      }
      if( m_pcTempAlfParam->chroma_idc & 0x01 )
      {
        recUnitBuf.get(COMPONENT_Cr).copyFrom( recExtBuf.get(COMPONENT_Cr) );
      }
      ruiBits += uiMinRate;
      ruiDist += uiMinDist;
    }
  }
  else
  {
    m_pcBestAlfParam->chroma_idc = 0;

    ruiBits += uiMinRate;
    ruiDist += uiMinDist;
    recUnitBuf.get(COMPONENT_Cb).copyFrom( recExtBuf.get(COMPONENT_Cb) );
    recUnitBuf.get(COMPONENT_Cr).copyFrom( recExtBuf.get(COMPONENT_Cr) );
  }
}

Void EncAdaptiveLoopFilter::xInitFixedFilters()
{
  Int maxFilterLength = m_MAX_SQR_FILT_LENGTH / 2 + 1;
#if JVET_C0038_NO_PREV_FILTERS
  Int factor = (1 << (m_NUM_BITS - 1));
  for (Int i = 0; i < maxFilterLength; i++)
  {
    for (Int j = 0; j < m_NO_FILTERS*JVET_C0038_NO_PREV_FILTERS; j++)
    {
      m_filterCoeffPrev[j][i] = (Double)m_ALFfilterCoeffFixed[j][i] / (Double)factor;
    }
  }
#endif
  memset(m_filterCoeffDefault, 0, (maxFilterLength - 1)*sizeof(Double));
  m_filterCoeffDefault[maxFilterLength - 1] = 1.0;
}

Void EncAdaptiveLoopFilter::xCheckReUseFilterSet( CodingStructure& cs, const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& dstUnitBuf, Double& rdMinCost, ALFParam& filterSet, Int filterSetIdx )
{
  ALFParam cFrmAlfParam;
  allocALFParam(&cFrmAlfParam);
  copyALFParam( &cFrmAlfParam, &filterSet );

  cFrmAlfParam.alf_flag         = 1;
  cFrmAlfParam.temporalPredFlag = true;
  cFrmAlfParam.prevIdx          = filterSetIdx;

  cFrmAlfParam.cu_control_flag  = 0;
  cFrmAlfParam.chroma_idc       = 0;

  Double dCost  = 0;
  UInt64 uiRate = 0;
  UInt64 uiDist = 0;

  xFilterFrame_en(dstUnitBuf, recExtBuf, cFrmAlfParam, cs.slice->clpRng(COMPONENT_Y));
  uiDist = xCalcSSD(orgUnitBuf, dstUnitBuf, COMPONENT_Y);
  xCalcRDCost( uiDist, &cFrmAlfParam, uiRate, dCost );

  if (dCost < rdMinCost)
  {
    rdMinCost = dCost;
    copyALFParam(m_pcBestAlfParam, &cFrmAlfParam );
    m_bestPelBuf.get(COMPONENT_Y).copyFrom(dstUnitBuf.get(COMPONENT_Y));
  }

  cFrmAlfParam.cu_control_flag = 1;

  for( UInt uiDepth = 0; uiDepth < m_uiMaxTotalCUDepth; uiDepth++ )
  {
    m_tempPelBuf.get(COMPONENT_Y).copyFrom(dstUnitBuf.get(COMPONENT_Y));
    copyALFParam( m_pcTempAlfParam, &cFrmAlfParam);
    m_pcTempAlfParam->alf_max_depth = uiDepth;

    xSetCUAlfCtrlFlags( cs, orgUnitBuf, recExtBuf, m_tempPelBuf, uiDist, uiDepth, m_pcTempAlfParam); //set up varImg here
    xCalcRDCost( uiDist, m_pcTempAlfParam, uiRate, dCost );
    if (dCost < rdMinCost )
    {
      rdMinCost  = dCost;
      m_bestPelBuf.get(COMPONENT_Y).copyFrom( m_tempPelBuf.get(COMPONENT_Y) );
      copyALFParam( m_pcBestAlfParam, m_pcTempAlfParam );
    }
  }

  freeALFParam(&cFrmAlfParam);

  if( m_pcBestAlfParam->temporalPredFlag )
  {
    if( filterSet.chroma_idc != 0 )
    {
      //CHECK CHROMA
      copyALFParam( m_pcTempAlfParam, m_pcBestAlfParam );

      xFilteringFrameChroma( orgUnitBuf, recExtBuf, dstUnitBuf );

      UInt64 uiOrgDistCb  = xCalcSSD(orgUnitBuf, recExtBuf, COMPONENT_Cb);
      UInt64 uiFiltDistCb = xCalcSSD(orgUnitBuf, dstUnitBuf, COMPONENT_Cb);

      if( uiFiltDistCb < uiOrgDistCb )
      {
        m_pcBestAlfParam->chroma_idc |= 2;
        m_bestPelBuf.get(COMPONENT_Cb).copyFrom( dstUnitBuf.get(COMPONENT_Cb ) );
      }
      else
      {
        m_bestPelBuf.get(COMPONENT_Cb).copyFrom( recExtBuf.get(COMPONENT_Cb ) );
      }

      UInt64 uiOrgDistCr  = xCalcSSD(orgUnitBuf, recExtBuf, COMPONENT_Cr);
      UInt64 uiFiltDistCr = xCalcSSD(orgUnitBuf, dstUnitBuf, COMPONENT_Cr);

      if(uiOrgDistCr  > uiFiltDistCr )
      {
        m_pcBestAlfParam->chroma_idc |= 1;
        m_bestPelBuf.get(COMPONENT_Cr).copyFrom( dstUnitBuf.get(COMPONENT_Cr) );
      }
      else
      {
        m_bestPelBuf.get(COMPONENT_Cr).copyFrom( recExtBuf.get(COMPONENT_Cr) );
      }
    }
    else
    {
      m_bestPelBuf.get(COMPONENT_Cb).copyFrom( recExtBuf.get(COMPONENT_Cb) );
      m_bestPelBuf.get(COMPONENT_Cr).copyFrom( recExtBuf.get(COMPONENT_Cr) );
    }
  }
}


// ====================================================================================================================
// Private member functions
// ====================================================================================================================

Void EncAdaptiveLoopFilter::xInitParam()
{
  Int i, j;

  if (m_ppdAlfCorr != NULL)
  {
    for (i = 0; i < m_ALF_MAX_NUM_COEF; i++)
    {
      for (j = 0; j < m_ALF_MAX_NUM_COEF+1; j++)
      {
        m_ppdAlfCorr[i][j] = 0;
      }
    }
  }
  else
  {
    m_ppdAlfCorr = new Double*[m_ALF_MAX_NUM_COEF];
    for (i = 0; i < m_ALF_MAX_NUM_COEF; i++)
    {
      m_ppdAlfCorr[i] = new Double[m_ALF_MAX_NUM_COEF+1];
      for (j = 0; j < m_ALF_MAX_NUM_COEF+1; j++)
      {
        m_ppdAlfCorr[i][j] = 0;
      }
    }
  }

  if (m_pdDoubleAlfCoeff != NULL)
  {
    for (i = 0; i < m_ALF_MAX_NUM_COEF; i++)
    {
      m_pdDoubleAlfCoeff[i] = 0;
    }
  }
  else
  {
    m_pdDoubleAlfCoeff = new Double[m_ALF_MAX_NUM_COEF];
    for (i = 0; i < m_ALF_MAX_NUM_COEF; i++)
    {
      m_pdDoubleAlfCoeff[i] = 0;
    }
  }
}

Void EncAdaptiveLoopFilter::xUninitParam()
{
  Int i;

  if (m_ppdAlfCorr != NULL)
  {
    for (i = 0; i < m_ALF_MAX_NUM_COEF; i++)
    {
      delete[] m_ppdAlfCorr[i];
      m_ppdAlfCorr[i] = NULL;
    }
    delete[] m_ppdAlfCorr;
    m_ppdAlfCorr = NULL;
  }

  if (m_pdDoubleAlfCoeff != NULL)
  {
    delete[] m_pdDoubleAlfCoeff;
    m_pdDoubleAlfCoeff = NULL;
  }
}


//********************************
// DERIVING FILTER COEFFICIENTS
//********************************

//COEFF-STORAGES:
// m_filterCoeff
// m_filterCoeffQuant
// m_filterCoeffSym
// m_filterCoeffSymQuant
// m_filterCoeffPrevSelected

//TODO needs cleanup:
//NOT USED IN THIS VERSION / Coefficents are calculated in xCheckFilterMerging
Void EncAdaptiveLoopFilter::xFindFilterCoeffsLuma(const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, AlfFilterType filtType
#if COM16_C806_ALF_TEMPPRED_NUM
  , ALFParam* pcAlfParam
#endif
  )
{

  Int filters_per_fr, sqrFiltLength;

  Double **ySym, ***ESym;
  Int lambda_val = (Int) m_dLambdaLuma;
      lambda_val = lambda_val * (1<<(2*m_nBitIncrement));

  sqrFiltLength = m_sqrFiltLengthTab[filtType];

  ESym = m_EGlobalSym[ filtType ];
  ySym = m_yGlobalSym[ filtType ];

  xStoreInBlockMatrix(orgUnitBuf, recExtBuf, filtType);

  filters_per_fr = AdaptiveLoopFilter::m_NO_FILTERS;

  memset(m_varIndTab,0,sizeof(int)*m_NO_VAR_BINS);
  for( Int filtIdx = 0; filtIdx < filters_per_fr; filtIdx++)
  {
    xSolveAndQuant( m_filterCoeff, m_filterCoeffQuant, ESym[filtIdx], ySym[filtIdx], sqrFiltLength, m_weightsTab[filtType], m_NUM_BITS );
    for( Int k = 0; k < sqrFiltLength; k++)
    {
      m_filterCoeffSym     [filtIdx][k] = m_filterCoeffQuant[k];
      m_filterCoeffSymQuant[filtIdx][k] = m_filterCoeffQuant[k];
    }
    m_varIndTab[filtIdx] = filtIdx;
  }

  xcalcPredFilterCoeff(filtType
#if COM16_C806_ALF_TEMPPRED_NUM
    , pcAlfParam
#endif
    );
}

Double EncAdaptiveLoopFilter::xCalcFilterCoeffsGalf( Double     ***EGlobalSeq,
                                                     Double      **yGlobalSeq,
                                                     Double       *pixAccGlobalSeq,
                                                     Int           interval[m_NO_VAR_BINS],
                                                     Int           filters_per_fr,
                                                     AlfFilterType filtType,
                                                     Double    errorTabForce0Coeff[m_NO_VAR_BINS][2])
{

  static double pixAcc_temp;

  Int sqrFiltLength  = m_sqrFiltLengthTab[ filtType ];
  const Int* weights = m_weightsTab[ filtType ];

  double error;
  int k;

  error = 0;
  for(Int filtIdx = 0; filtIdx < filters_per_fr; filtIdx++ )
  {
    add_A_galf(m_E_temp, EGlobalSeq, interval, filtIdx, sqrFiltLength);
    add_b_galf(m_y_temp, yGlobalSeq, interval, filtIdx, sqrFiltLength);
    pixAcc_temp = 0;
    for (k = 0; k < m_NO_VAR_BINS; k++)
    {
      if (interval[k] == filtIdx)
      {
        pixAcc_temp += pixAccGlobalSeq[k];
      }
    }
    // Find coeffcients
    errorTabForce0Coeff[filtIdx][1] = pixAcc_temp + xSolveAndQuant(m_filterCoeff, m_filterCoeffQuant, m_E_temp, m_y_temp, sqrFiltLength, weights, m_NUM_BITS );
    errorTabForce0Coeff[filtIdx][0] = pixAcc_temp;
    error += errorTabForce0Coeff[filtIdx][1];

    for(k = 0; k < sqrFiltLength; k++)
    {
      m_filterCoeffSym[filtIdx][k]      = m_filterCoeffQuant[k];
      m_filterCoeffSymQuant[filtIdx][k] = m_filterCoeffQuant[k];
    }
  }
  return(error);
}

Double EncAdaptiveLoopFilter::xCalcFilterCoeffs( Double     ***EGlobalSeq,
                                                 Double      **yGlobalSeq,
                                                 Double       *pixAccGlobalSeq,
                                                 Int           intervalBest[m_NO_VAR_BINS][2],
                                                 Int           filters_per_fr,
                                                 AlfFilterType filtType,
                                                 Int     **filterCoeffSeq,
                                                 Int     **filterCoeffQuantSeq,
                                                 Double    errorTabForce0Coeff[m_NO_VAR_BINS][2] )
{

  static double pixAcc_temp;

  Int sqrFiltLength  = m_sqrFiltLengthTab[ filtType ];
  const Int* weights = m_weightsTab[ filtType ];

  double error;
  int k;

  error = 0;
  for(Int filtIdx = 0; filtIdx < filters_per_fr; filtIdx++ )
  {
    add_A(m_E_temp, EGlobalSeq, intervalBest[filtIdx][0], intervalBest[filtIdx][1], sqrFiltLength);
    add_b(m_y_temp, yGlobalSeq, intervalBest[filtIdx][0], intervalBest[filtIdx][1], sqrFiltLength);
    pixAcc_temp = 0;
    for (k = intervalBest[filtIdx][0]; k <= intervalBest[filtIdx][1]; k++)
    {
      pixAcc_temp += pixAccGlobalSeq[k];
    }
    // Find coeffcients
    errorTabForce0Coeff[filtIdx][1] = pixAcc_temp + xSolveAndQuant(m_filterCoeff, m_filterCoeffQuant, m_E_temp, m_y_temp, sqrFiltLength, weights, m_NUM_BITS );
    errorTabForce0Coeff[filtIdx][0] = pixAcc_temp;
    error += errorTabForce0Coeff[filtIdx][1];

    for(k = 0; k < sqrFiltLength; k++)
    {
      filterCoeffSeq[filtIdx][k]      = m_filterCoeffQuant[k];
      filterCoeffQuantSeq[filtIdx][k] = m_filterCoeffQuant[k];
    }
  }
  return(error);
}

#if FORCE0
Double EncAdaptiveLoopFilter::xCalcDistForce0(
  Int           filters_per_fr,
  AlfFilterType filtType,
  Int			  sqrFiltLength,
  Double        errorTabForce0Coeff[m_NO_VAR_BINS][2],
  Double        lambda,
  Int           codedVarBins[m_NO_VAR_BINS])
{
  Double distForce0;
  Int bitsVarBin[m_NO_VAR_BINS];

  xcollectStatCodeFilterCoeffForce0(m_filterCoeffSymQuant, filtType, sqrFiltLength,
    filters_per_fr, bitsVarBin);

  xdecideCoeffForce0(codedVarBins, &distForce0, errorTabForce0Coeff, bitsVarBin, lambda, filters_per_fr);

  return distForce0;
}
#endif

Void EncAdaptiveLoopFilter::xStoreInBlockMatrix(const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, AlfFilterType filtType)
{
  const CPelBuf orgLuma = orgUnitBuf.get(COMPONENT_Y);
  const Pel*    orgBuf = orgLuma.buf;
  const Int     orgStride = orgLuma.stride;

  const CPelBuf recExtLuma = recExtBuf.get(COMPONENT_Y);
  const Pel*    recBufExt = recExtLuma.buf;
  const Int     recStrideExt = recExtLuma.stride;

  if ( !m_isGALF || m_updateMatrix)
  {
    Int var_step_size_w = m_ALF_VAR_SIZE_W;
    Int var_step_size_h = m_ALF_VAR_SIZE_H;

    Int tap = m_mapTypeToNumOfTaps[ filtType ];

    Int i,j,k,l,varInd;
    Int x, y;
    Int fl =tap/2;
    Int flV = AdaptiveLoopFilter::ALFFlHToFlV(fl);
    Int sqrFiltLength = AdaptiveLoopFilter::ALFTapHToNumCoeff(tap);
    Int fl2=9/2; //extended size at each side of the frame
    Int ELocal[m_MAX_SQR_FILT_LENGTH];
    Int yLocal;

    Double **E,*yy;
    Int count_valid=0;


    const Int *p_pattern = m_patternTab[filtType];

    memset( m_pixAcc, 0,sizeof(Double) * m_NO_VAR_BINS);

    for (varInd=0; varInd<m_NO_VAR_BINS; varInd++)
    {
      memset(m_yGlobalSym[filtType][varInd],0,sizeof(double)*m_MAX_SQR_FILT_LENGTH);
      for (k=0; k<sqrFiltLength; k++)
      {
        memset(m_EGlobalSym[filtType][varInd][k],0,sizeof(double)*m_MAX_SQR_FILT_LENGTH);
      }
    }

    for (i = fl2; i < m_img_height+fl2; i++)
    {
      for (j = fl2; j < m_img_width+fl2; j++)
      {
        if ( m_maskBuf.at(j-fl2, i-fl2) == 1) //[i-fl2][j-fl2]
        {
          count_valid++;
        }
      }
    }

    for (i=0,y=fl2; i<m_img_height; i++,y++)
    {
      for (j=0,x=fl2; j<m_img_width; j++,x++)
      {
        Int condition = (m_maskBuf.at(j,i) == 0 && count_valid > 0);
        if(!condition)
        {
          k = 0;
          memset(ELocal, 0, sqrFiltLength*sizeof(int));
          if( m_isGALF )
          {
            varInd = m_varImg[i][j];
            Int transpose = 0;
            Int varIndMod = selectTransposeVarInd(varInd, &transpose);
            yLocal = orgBuf[(i)*orgStride + (j)] - recBufExt[(i)*recStrideExt + (j)];
            calcMatrixE(ELocal, recBufExt, p_pattern, i, j, flV, fl, transpose, recStrideExt);
            E = m_EGlobalSym[filtType][varIndMod];
            yy = m_yGlobalSym[filtType][varIndMod];

            for (k = 0; k < sqrFiltLength; k++)
            {
              for (l = k; l < sqrFiltLength; l++)
              {
                E[k][l] += (double)(ELocal[k] * ELocal[l]);
              }
              yy[k] += (double)(ELocal[k] * yLocal);
            }
            m_pixAcc[varIndMod] += (yLocal*yLocal);
          }
          else
          {
            varInd = m_varImg[i / var_step_size_h][j / var_step_size_w];
            for (int ii = -flV; ii < 0; ii++)
            {
              for (int jj=-fl-ii; jj<=fl+ii; jj++)
              {
                ELocal[p_pattern[k++]]+=(recBufExt[(i+ii)*recStrideExt + (j+jj)]+recBufExt[(i-ii)*recStrideExt + (j-jj)] );
              }
            }
            for (int jj=-fl; jj<0; jj++)
            {
              ELocal[p_pattern[k++]]+=(recBufExt[(i)*recStrideExt + (j+jj)]+recBufExt[(i)*recStrideExt + (j-jj)]);
            }
            ELocal[p_pattern[k++]] += recBufExt[(i)*recStrideExt + (j)];
            ELocal[sqrFiltLength-1]=1;
            yLocal=orgBuf[(i)*orgStride + (j)];

            m_pixAcc[varInd]+=(yLocal*yLocal);
            E = m_EGlobalSym[filtType][varInd];
            yy= m_yGlobalSym[filtType][varInd];

            for (k=0; k<sqrFiltLength; k++)
            {
              for (l=k; l<sqrFiltLength; l++)
                E[k][l]+=(double)(ELocal[k]*ELocal[l]);
              yy[k]+=(double)(ELocal[k]*yLocal);
            }
          }
        }
      }
    }

    // Matrix EGlobalSeq is symmetric, only part of it is calculated
    for (varInd=0; varInd<m_NO_VAR_BINS; varInd++)
    {
      double **pE = m_EGlobalSym[filtType][varInd];
      for (k=1; k<sqrFiltLength; k++)
      {
        for (l=0; l<k; l++)
        {
          pE[k][l]=pE[l][k];
        }
      }
    }
  }
  else
  {
    CHECK(filtType == 2, "filterType has to be 0 or 1!");
    for (Int varInd = 0; varInd < m_NO_VAR_BINS; varInd++)
    {
      xDeriveGlobalEyFromLgrTapFilter(m_EGlobalSym[2][varInd], m_yGlobalSym[2][varInd], m_EGlobalSym[filtType][varInd], m_yGlobalSym[filtType][varInd], m_patternMapTab[2], m_patternMapTab[filtType]);
    }
  }
}


Void EncAdaptiveLoopFilter::calcMatrixE(int *ELocal, const Pel *recBufExt, const int *p_pattern, int i, int j, int flV, int fl, int transpose, int recStrideExt)
{
  int ii, jj, k = 0;

  if (transpose == 0)
  {
    for (ii = -flV; ii < 0; ii++)
    {
      for (jj = -fl - ii; jj <= fl + ii; jj++)
      {
        ELocal[p_pattern[k++]] += (recBufExt[(i + ii)*recStrideExt + (j + jj)] + recBufExt[(i - ii)*recStrideExt + (j - jj)]);
      }
    }
    for (jj = -fl; jj<0; jj++)
    {
      ELocal[p_pattern[k++]] += (recBufExt[(i)*recStrideExt + (j + jj)] + recBufExt[(i)*recStrideExt + (j - jj)]);
    }
    ELocal[p_pattern[k++]] += recBufExt[(i)*recStrideExt + (j)];
  }
  else if (transpose == 1)
  {
    for (jj = -flV; jj < 0; jj++)
    {
      for (ii = -fl - jj; ii <= fl + jj; ii++)
      {
        ELocal[p_pattern[k++]] += (recBufExt[(i + ii)*recStrideExt + (j + jj)] + recBufExt[(i - ii)*recStrideExt + (j - jj)]);
      }
    }
    for (ii = -fl; ii<0; ii++)
    {
      ELocal[p_pattern[k++]] += (recBufExt[(i + ii)*recStrideExt + j] + recBufExt[(i - ii)*recStrideExt + j]);
    }
    ELocal[p_pattern[k++]] += recBufExt[(i)*recStrideExt + (j)];
  }
  else if (transpose == 2)
  {
    for (ii = -flV; ii < 0; ii++)
    {
      for (jj = fl + ii; jj >= -fl - ii; jj--)
      {
        ELocal[p_pattern[k++]] += (recBufExt[(i + ii)*recStrideExt + (j + jj)] + recBufExt[(i - ii)*recStrideExt + (j - jj)]);
      }
    }
    for (jj = -fl; jj<0; jj++)
    {
      ELocal[p_pattern[k++]] += (recBufExt[(i)*recStrideExt + (j + jj)] + recBufExt[(i)*recStrideExt + (j - jj)]);
    }
    ELocal[p_pattern[k++]] += recBufExt[(i)*recStrideExt + (j)];
  }
  else
  {
    for (jj = -flV; jj < 0; jj++)
    {
      for (ii = fl + jj; ii >= -fl - jj; ii--)
      {
        ELocal[p_pattern[k++]] += (recBufExt[(i + ii)*recStrideExt + (j + jj)] + recBufExt[(i - ii)*recStrideExt + (j - jj)]);
      }
    }
    for (ii = -fl; ii<0; ii++)
    {
      ELocal[p_pattern[k++]] += (recBufExt[(i + ii)*recStrideExt + j] + recBufExt[(i - ii)*recStrideExt + j]);
    }
    ELocal[p_pattern[k++]] += recBufExt[(i)*recStrideExt + (j)];
  }
}

Int EncAdaptiveLoopFilter::xFilterPixel(const Pel *ImgDec, Int* varIndBeforeMapping, Int **filterCoeffSym, Int *pattern, Int i, Int j, Int fl, Int Stride, AlfFilterType filtNo)
{
  Int varInd;
  Int transpose;
  const Pel *imgY_rec = ImgDec;
  const Pel *p_imgY_pad, *p_imgY_pad0;

  varInd = selectTransposeVarInd((*varIndBeforeMapping), &transpose);
  (*varIndBeforeMapping) = varInd;
  Int *coef = filterCoeffSym == NULL ? m_filterCoeffPrevSelected[varInd] : filterCoeffSym[varInd];
  Int pixelInt = 0;

  if (filterCoeffSym == NULL)
  {
    if (transpose == 1)
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[38] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[30] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[39] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[32] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[22] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[31] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[40] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[37] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[29] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[33] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[21] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[23] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[13] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);
      }
      else
      {
        pixelInt += coef[36] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[28] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[37] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[34] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[20] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[29] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[33] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[24] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[12] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[21] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[23] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[14] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[4] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);
      }
    }
    else if (transpose == 3)
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[38] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[32] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[39] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[30] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[22] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[31] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[40] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[37] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[33] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[29] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[23] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[21] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[13] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);

      }
      else
      {
        pixelInt += coef[36] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[34] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[37] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[28] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[24] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[33] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[29] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[20] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[14] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[23] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[21] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[4] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);

      }
    }
    else if (transpose == 2)
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[22] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[32] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[31] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[30] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[38] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[39] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[40] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[13] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[23] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[21] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[33] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[29] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[37] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);

      }
      else
      {
        pixelInt += coef[4] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[14] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[24] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[23] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[21] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[20] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[34] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[33] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[29] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[28] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[36] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[37] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);
      }
    }
    else
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[22] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[30] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[31] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[32] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[38] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[39] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[40] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[13] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[21] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[23] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[29] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[33] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[37] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);

      }
      else
      {
        pixelInt += coef[4] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[12] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[14] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[20] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[21] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[22] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[23] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[24] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[28] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[29] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[30] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[31] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[32] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[33] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[34] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[36] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[37] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[38] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[39] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[40] * (p_imgY_pad[j - fl]);

      }
    }
  }
  else
  {
    //test prev filter
    if (transpose == 1)
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[4] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[1] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[5] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[3] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[0] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[2] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[6] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[9] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[4] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[8] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[1] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[3] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[0] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl]);

      }
      else
      {
        pixelInt += coef[16] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[9] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[17] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[15] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[4] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[18] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[14] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[8] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[1] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[19] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[3] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[0] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[20] * (p_imgY_pad[j - fl]);
      }
    }
    else if (transpose == 3)
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[4] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[3] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[5] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[1] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[0] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[2] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[6] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[9] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[8] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[4] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[3] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[1] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[0] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl]);
      }
      else
      {
        pixelInt += coef[16] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[15] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[17] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[9] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[8] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[14] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[18] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[4] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[3] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[19] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[1] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[0] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[20] * (p_imgY_pad[j - fl]);
      }
    }
    else if (transpose == 2)
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[0] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[3] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[2] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[1] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[4] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[5] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[6] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[0] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[3] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[1] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[8] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[4] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[9] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl]);

      }
      else
      {
        pixelInt += coef[0] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[3] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[1] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[8] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[4] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[15] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[14] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[9] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[16] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[17] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[18] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[19] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[20] * (p_imgY_pad[j - fl]);
      }
    }
    else
    {
      if (filtNo == 0) //5x5
      {
        pixelInt += coef[0] * (imgY_rec[(i - fl + 2)*Stride + j - fl] + imgY_rec[(i - fl - 2)*Stride + j - fl]);

        pixelInt += coef[1] * (imgY_rec[(i - fl + 1)*Stride + j - fl + 1] + imgY_rec[(i - fl - 1)*Stride + j - fl - 1]);
        pixelInt += coef[2] * (imgY_rec[(i - fl + 1)*Stride + j - fl] + imgY_rec[(i - fl - 1)*Stride + j - fl]);
        pixelInt += coef[3] * (imgY_rec[(i - fl + 1)*Stride + j - fl - 1] + imgY_rec[(i - fl - 1)*Stride + j - fl + 1]);

        pixelInt += coef[4] * (imgY_rec[(i - fl)*Stride + j - fl - 2] + imgY_rec[(i - fl)*Stride + j - fl + 2]);
        pixelInt += coef[5] * (imgY_rec[(i - fl)*Stride + j - fl - 1] + imgY_rec[(i - fl)*Stride + j - fl + 1]);
        pixelInt += coef[6] * (imgY_rec[(i - fl)*Stride + j - fl]);
      }
      else if (filtNo == 1) //7x7
      {
        pixelInt += coef[0] * (imgY_rec[(i - fl + 3)*Stride + j - fl] + imgY_rec[(i - fl - 3)*Stride + j - fl]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[1] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[3] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[4] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[8] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[9] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl]);
      }
      else
      {
        pixelInt += coef[0] * (imgY_rec[(i - fl + 4)*Stride + j - fl] + imgY_rec[(i - fl - 4)*Stride + j - fl]);
        p_imgY_pad = imgY_rec + (i - fl + 3)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 3)*Stride;
        pixelInt += coef[1] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[2] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[3] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);

        p_imgY_pad = imgY_rec + (i - fl + 2)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 2)*Stride;
        pixelInt += coef[4] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[5] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[6] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[7] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[8] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);

        p_imgY_pad = imgY_rec + (i - fl + 1)*Stride;
        p_imgY_pad0 = imgY_rec + (i - fl - 1)*Stride;
        pixelInt += coef[9] * (p_imgY_pad[j - fl + 3] + p_imgY_pad0[j - fl - 3]);
        pixelInt += coef[10] * (p_imgY_pad[j - fl + 2] + p_imgY_pad0[j - fl - 2]);
        pixelInt += coef[11] * (p_imgY_pad[j - fl + 1] + p_imgY_pad0[j - fl - 1]);
        pixelInt += coef[12] * (p_imgY_pad[j - fl] + p_imgY_pad0[j - fl]);
        pixelInt += coef[13] * (p_imgY_pad[j - fl - 1] + p_imgY_pad0[j - fl + 1]);
        pixelInt += coef[14] * (p_imgY_pad[j - fl - 2] + p_imgY_pad0[j - fl + 2]);
        pixelInt += coef[15] * (p_imgY_pad[j - fl - 3] + p_imgY_pad0[j - fl + 3]);

        p_imgY_pad = imgY_rec + (i - fl)*Stride;
        pixelInt += coef[16] * (p_imgY_pad[j - fl + 4] + p_imgY_pad[j - fl - 4]);
        pixelInt += coef[17] * (p_imgY_pad[j - fl + 3] + p_imgY_pad[j - fl - 3]);
        pixelInt += coef[18] * (p_imgY_pad[j - fl + 2] + p_imgY_pad[j - fl - 2]);
        pixelInt += coef[19] * (p_imgY_pad[j - fl + 1] + p_imgY_pad[j - fl - 1]);
        pixelInt += coef[20] * (p_imgY_pad[j - fl]);
      }
    }
  }
  return(pixelInt);
}


Void EncAdaptiveLoopFilter::xcalcPredFilterCoeff(AlfFilterType filtType
#if COM16_C806_ALF_TEMPPRED_NUM
  , ALFParam* alfParam
#endif
  )
{
  if( m_isGALF )
  {
    int varInd, i, k;

    const int *patternMap = m_patternMapTab[filtType];
    const Int *weights = AdaptiveLoopFilter::m_weightsTab[2];
    Int quantCoeffSum = 0;
    Int factor = (1 << (AdaptiveLoopFilter::m_NUM_BITS - 1));

    for (varInd=0; varInd<m_NO_VAR_BINS; ++varInd)
    {
      k=0;
      quantCoeffSum = 0;

      for (i = 0; i < m_MAX_SQR_FILT_LENGTH; i++)
      {
        if (m_pattern9x9Sym_Quart[i]>0 && patternMap[i]>0)
        {
          m_filterCoeffPrevSelected[varInd][i] = (m_filterCoeffFinal[varInd][m_pattern9x9Sym_Quart[i] - 1] + m_filterCoeffSym[m_varIndTab[varInd]][patternMap[i] - 1]);
          if (i != (m_MAX_SQR_FILT_LENGTH - 1))
          {
            quantCoeffSum += (weights[m_pattern9x9Sym_Quart[i] - 1] * m_filterCoeffPrevSelected[varInd][i]);
          }
          k++;
        }
        else if (m_pattern9x9Sym_Quart[i] > 0)
        {
          m_filterCoeffPrevSelected[varInd][i] = m_filterCoeffFinal[varInd][m_pattern9x9Sym_Quart[i] - 1];
          if (i != (m_MAX_SQR_FILT_LENGTH - 1))
          {
            quantCoeffSum += (weights[m_pattern9x9Sym_Quart[i] - 1] * m_filterCoeffPrevSelected[varInd][i]);
          }
        }
        else
        {
          m_filterCoeffPrevSelected[varInd][i] = 0;
        }
      }
      m_filterCoeffPrevSelected[varInd][m_MAX_SQR_FILT_LENGTH - 1] = factor - quantCoeffSum;
    }

  #if COM16_C806_ALF_TEMPPRED_NUM

    patternMap = m_patternMapTab[2];
    for (Int varInd = 0; varInd<m_NO_VAR_BINS; ++varInd)
    {
      Int k = 0;
      for (Int i = 0; i < m_MAX_SQR_FILT_LENGTH; i++)
      {
        if (patternMap[i] > 0)
        {
          alfParam->alfCoeffLuma[varInd][k] = m_filterCoeffPrevSelected[varInd][i];
          k++;
        }
      }
    }
  #endif
  }
  else
  {
    const int* patternMap = m_patternMapTab[filtType];
    for( int varInd=0; varInd<m_NO_VAR_BINS; ++varInd)
    {
      int k=0;
      for( int i = 0; i < m_MAX_SQR_FILT_LENGTH; i++)
      {
        if (patternMap[i]>0)
        {
          m_filterCoeffPrevSelected[varInd][i]=m_filterCoeffSym[m_varIndTab[varInd]][k];
          alfParam->alfCoeffLuma[varInd][k] = m_filterCoeffPrevSelected[varInd][i];
          k++;
        }
        else
        {
          m_filterCoeffPrevSelected[varInd][i]=0;
        }
      }
    }
  }
}

Double EncAdaptiveLoopFilter::xSolveAndQuant(Double *filterCoeff, Int *filterCoeffQuant, Double **E, Double *y, Int sqrFiltLength, const Int *weights, Int bit_depth, Bool bChroma )
{
  double error;

  int factor = (1<<(bit_depth-1)), i;
  int quantCoeffSum, minInd, targetCoeffSumInt, k, diff;
  double errMin;

  gnsSolveByChol(E, y, filterCoeff, sqrFiltLength);

  if( m_isGALF )
  {
    roundFiltCoeff(filterCoeffQuant, filterCoeff, sqrFiltLength, factor);
    if (bChroma)
    {
      targetCoeffSumInt = factor;
    }
    else
    targetCoeffSumInt = 0;
  }
  else
  {
    double targetCoeffSum = 0;
    for( i=0; i<sqrFiltLength; i++)
    {
      targetCoeffSum+=(weights[i]*filterCoeff[i]*factor);
    }
    targetCoeffSumInt=ROUND(targetCoeffSum);

    roundFiltCoeff(filterCoeffQuant, filterCoeff, sqrFiltLength, factor);
  }

  quantCoeffSum=0;
  for (i=0; i<sqrFiltLength; i++)
  {
    quantCoeffSum += weights[i]*filterCoeffQuant[i];
  }

  int count=0;
  while(quantCoeffSum!=targetCoeffSumInt && count < 10)
  {
    if (quantCoeffSum>targetCoeffSumInt)
    {
      diff=quantCoeffSum-targetCoeffSumInt;
      errMin=0; minInd=-1;
      for (k=0; k<sqrFiltLength; k++)
      {
        if (weights[k]<=diff)
        {
          for (i=0; i<sqrFiltLength; i++)
          {
            m_filterCoeffQuantMod[i]=filterCoeffQuant[i];
          }
          m_filterCoeffQuantMod[k]--;
          for (i=0; i<sqrFiltLength; i++)
          {
            filterCoeff[i]=(double)m_filterCoeffQuantMod[i]/(double)factor;
          }
          error = xCalcErrorForGivenWeights(E, y, filterCoeff, sqrFiltLength);
          if (error<errMin || minInd==-1)
          {
            errMin=error;
            minInd=k;
          }
        } // if (weights(k)<=diff){
      } // for (k=0; k<sqrFiltLength; k++){
      filterCoeffQuant[minInd]--;
    }
    else
    {
      diff=targetCoeffSumInt-quantCoeffSum;
      errMin=0; minInd=-1;
      for (k=0; k<sqrFiltLength; k++)
      {
        if (weights[k]<=diff)
        {
          for (i=0; i<sqrFiltLength; i++)
          {
            m_filterCoeffQuantMod[i]=filterCoeffQuant[i];
          }
          m_filterCoeffQuantMod[k]++;
          for (i=0; i<sqrFiltLength; i++)
          {
            filterCoeff[i]=(double)m_filterCoeffQuantMod[i]/(double)factor;
          }
          error = xCalcErrorForGivenWeights(E, y, filterCoeff, sqrFiltLength);
          if (error<errMin || minInd==-1)
          {
            errMin=error;
            minInd=k;
          }
        } // if (weights(k)<=diff){
      } // for (k=0; k<sqrFiltLength; k++){
      filterCoeffQuant[minInd]++;
    }

    quantCoeffSum=0;
    for (i=0; i<sqrFiltLength; i++)
    {
      quantCoeffSum+=weights[i]*filterCoeffQuant[i];
    }
  }
  if( count == 10 )
  {
    for (i=0; i<sqrFiltLength; i++)
    {
      filterCoeffQuant[i] = 0;
    }
  }

  if( m_isGALF )
  {
    quantCoeffSum = 0;
    for (i = 0; i<sqrFiltLength - 1; i++)
    {
      quantCoeffSum += weights[i] * filterCoeffQuant[i];
    }
    if (bChroma)
    {
      filterCoeffQuant[sqrFiltLength - 1] = factor - quantCoeffSum;
    }
    else
    {
      filterCoeffQuant[sqrFiltLength - 1] = -quantCoeffSum;
    }
  }

  Int max_value = std::min((1<<(3+m_nInputBitDepth + m_nBitIncrement))-1, (1<<14)-1);
  Int min_value = std::max(-(1<<(3+m_nInputBitDepth + m_nBitIncrement)), -(1<<14));
  for (i=0; i<sqrFiltLength; i++)
  {
    filterCoeffQuant[i] = std::min( max_value , std::max( min_value , filterCoeffQuant[i] ) );
    filterCoeff[i]=(double)filterCoeffQuant[i]/(double)factor;
  }

  if( ! m_isGALF )
  {
    // Encoder-side restriction on the ALF coefficients for limiting the Clipping LUT size
    Int maxSampleValue, minSampleValue;
    Int sumCoef[2]    = {0, 0};
    Int maxPxlVal     = m_nIBDIMax;
    Int numBitsMinus1 = m_NUM_BITS-1;
    Int offset        = (1<<(m_NUM_BITS-2));
    Int lastCoef      = sqrFiltLength-1;
    Int centerCoef    = sqrFiltLength-2;

    Int *coef = filterCoeffQuant;
    Int clipTableMax = ( ( m_ALF_HM3_QC_CLIP_RANGE - m_ALF_HM3_QC_CLIP_OFFSET - 1 ) << m_nBitIncrement );
    Int clipTableMin = ( ( - m_ALF_HM3_QC_CLIP_OFFSET ) << m_nBitIncrement );

    for (i=0; i<centerCoef; i++)
    {
      sumCoef[coef[i]>0?0:1] += (coef[i]<<1);
    }
    sumCoef[ coef[centerCoef] > 0 ? 0 : 1 ] += coef[centerCoef];

    maxSampleValue = ( maxPxlVal * sumCoef[0] + coef[lastCoef] + offset ) >> numBitsMinus1;
    minSampleValue = ( maxPxlVal * sumCoef[1] + coef[lastCoef] + offset ) >> numBitsMinus1;

    if ( maxSampleValue > clipTableMax || minSampleValue < clipTableMin )
    {
      memset( coef, 0, sizeof(Int) * sqrFiltLength );
      coef[centerCoef] = (1<<numBitsMinus1);
      for (Int j=0; j<sqrFiltLength; j++)
      {
        CHECK( !(filterCoeffQuant[j]>=min_value && filterCoeffQuant[j]<=max_value), "ALF: quant coeffs out of bound" );
        filterCoeff[j]=(double)filterCoeffQuant[j]/(double)factor;
      }
    }
  }
  error=xCalcErrorForGivenWeights(E, y, filterCoeff, sqrFiltLength);
  return(error);
}


Void EncAdaptiveLoopFilter::roundFiltCoeff(Int *filterCoeffQuant, Double *filterCoeff, Int sqrFiltLength, Int factor)
{
  int i;
  double diff;
  int diffInt, sign;

  for(i = 0; i < sqrFiltLength; i++)
  {
    sign               = (filterCoeff[i]>0) ?  1: -1;
    diff               = filterCoeff[i]*sign;
    diffInt            = (Int)(diff*(Double)factor+0.5);
    filterCoeffQuant[i] = diffInt*sign;
  }
}

Void EncAdaptiveLoopFilter::xCheckFilterMergingGalf(ALFParam& alfParam
#if JVET_C0038_NO_PREV_FILTERS
  , const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf
#endif
)
{
#if JVET_C0038_NO_PREV_FILTERS
  const PelBuf orgLuma = orgUnitBuf.get(COMPONENT_Y);
  const Pel*   pOrg = orgLuma.buf;
  const Int    orgStride = orgLuma.stride;

  const PelBuf recExtLuma = recExtBuf.get(COMPONENT_Y);
  const Pel*   pRecExt = recExtLuma.buf;
  const Int    extStride = recExtLuma.stride;
#endif

  static double ***E_temp;
  static double  **y_temp;
  static double   *pixAcc_temp;
  static int     **FilterCoeffQuantTemp;
  static int       first = 0;

  static int first9x9 = 0;
  static Double **y_temp9x9;

  if (first==0)
  {
    initMatrix3D_double(&E_temp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH, m_MAX_SQR_FILT_LENGTH);
    initMatrix_double  (&y_temp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
    pixAcc_temp = (double *) calloc(m_NO_VAR_BINS, sizeof(double));
    initMatrix_int(&FilterCoeffQuantTemp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
    first=1;
  }

  AlfFilterType filtType = alfParam.filterType;

  double ***ESym = m_EGlobalSym[filtType];
  double **ySym  = m_yGlobalSym[filtType];
  double *pixAcc = m_pixAcc;

  Int filters_per_fr_best=0;
  Int filters_per_fr, firstFilt;
  Int intervalBest[m_NO_VAR_BINS][m_NO_VAR_BINS];
  Int i, k, varInd;
  Double  lagrangian, lagrangianMin;
#if FORCE0
  Double lagrangianForce0;
#endif
  Int lambda =  ((Int)m_dLambdaLuma) * (1<<(2*m_nBitIncrement));
  Int sqrFiltLength;

  Double errorForce0CoeffTab[m_NO_VAR_BINS][2];


  sqrFiltLength = m_sqrFiltLengthTab[ filtType];

  if (first9x9 == 0)
  {
    initMatrix_double(&y_temp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
    initMatrix_double(&y_temp9x9, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
    first9x9 = 1;
  }
  else if (!m_updateMatrix)
  {
    for (Int varInd = 0; varInd < m_NO_VAR_BINS; varInd++)
    {
      xDeriveLocalEyFromLgrTapFilter(y_temp9x9[varInd], y_temp[varInd], m_patternMapTab[2], m_patternMapTab[filtType]);
    }
  }

  memcpy(pixAcc_temp, pixAcc, sizeof(double)*m_NO_VAR_BINS);

  for (varInd=0; varInd<m_NO_VAR_BINS; varInd++)
  {
    if (m_updateMatrix)
    {
      memcpy(y_temp[varInd], ySym[varInd], sizeof(double)*sqrFiltLength);
    }
    for (k = 0; k < sqrFiltLength; k++)
    {
      memcpy(E_temp[varInd][k], ESym[varInd][k], sizeof(double)*sqrFiltLength);
    }
  }
#if JVET_C0038_NO_PREV_FILTERS
  static Int usePrevFiltBest[m_NO_VAR_BINS];
  Int iFixedFilters = alfParam.iAvailableFilters;
#endif

  xfindBestFilterPredictor(
#if JVET_C0038_NO_PREV_FILTERS
    E_temp, y_temp, pixAcc_temp, filtType, pOrg, pRecExt, orgStride, extStride,
    usePrevFiltBest, sqrFiltLength, iFixedFilters
#endif
    );

  for(i = 0; i < m_NO_VAR_BINS; i++)
  {
    memset(m_filterCoeffSym[i], 0, sizeof(int)*m_MAX_SQR_FILT_LENGTH);
    memset(m_filterCoeffSymQuant[i], 0, sizeof(int)*m_MAX_SQR_FILT_LENGTH);
  }

  firstFilt=1;  lagrangianMin=0;
  filters_per_fr = m_NO_FILTERS;
  Int predMode     = 0;
  Int bestPredMode = 0;
  Double dist;
  Int coeffBits;
#if FORCE0
  Double distForce0;
  Int coeffBitsForce0;
  Int codedVarBins[m_NO_VAR_BINS];
#endif
  // zero all variables 
  memset(intervalBest, 0, sizeof(Int)*m_NO_VAR_BINS);
  xMergeFiltersGreedyGalf(E_temp, y_temp, pixAcc_temp, intervalBest, sqrFiltLength);

  while(filters_per_fr >=1 )
  {

    dist = xCalcFilterCoeffsGalf(E_temp, y_temp, pixAcc_temp, intervalBest[filters_per_fr - 1], filters_per_fr, filtType,
      errorForce0CoeffTab);
#if FORCE0
    distForce0 = xCalcDistForce0(filters_per_fr, filtType, sqrFiltLength, errorForce0CoeffTab,
      lambda, codedVarBins);
#endif

    coeffBits = xCheckFilterPredictionMode(m_filterCoeffSymQuant, filters_per_fr, filtType, predMode);
#if FORCE0
    coeffBitsForce0 = xCalcBitsForce0(m_filterCoeffSymQuant, filters_per_fr, filtType, codedVarBins);
#endif

    lagrangian = dist + lambda * coeffBits;
#if FORCE0
    lagrangianForce0 = distForce0 + lambda * coeffBitsForce0;
    if (lagrangianForce0 < lagrangian)
    {
      lagrangian = lagrangianForce0;
    }
#endif
    if( lagrangian<=lagrangianMin || firstFilt==1)
    {
      firstFilt=0;
      lagrangianMin=lagrangian;

      filters_per_fr_best = filters_per_fr;
      bestPredMode        = predMode;
      memcpy(m_varIndTab, intervalBest[filters_per_fr-1], m_NO_VAR_BINS * sizeof(int));
    }
    filters_per_fr--;
  }

  dist = xCalcFilterCoeffsGalf(E_temp, y_temp, pixAcc_temp, m_varIndTab, filters_per_fr_best, filtType,
    errorForce0CoeffTab);
  coeffBits = xCheckFilterPredictionMode(m_filterCoeffSymQuant, filters_per_fr_best, filtType, predMode);
#if FORCE0
  distForce0 = xCalcDistForce0(filters_per_fr_best, filtType, sqrFiltLength, errorForce0CoeffTab, lambda, codedVarBins);
  coeffBitsForce0 = xCalcBitsForce0(m_filterCoeffSymQuant, filters_per_fr_best, filtType, codedVarBins);
  lagrangian = dist + lambda * coeffBits;
  lagrangianForce0 = distForce0 + lambda * coeffBitsForce0;
  alfParam.forceCoeff0 = (lagrangian < lagrangianForce0) ? 0 : 1;
  if (alfParam.forceCoeff0)
  {
    memcpy(alfParam.codedVarBins, codedVarBins, sizeof(int)*m_NO_VAR_BINS);
  }
#endif

  CHECK( predMode != bestPredMode, "wrong re-calculation" )

  //set alfParameter
  alfParam.filters_per_group_diff = filters_per_fr_best;
  alfParam.filters_per_group      = filters_per_fr_best;
  alfParam.predMethod             = bestPredMode;

#if FORCE0
  if (alfParam.forceCoeff0)
  {
    alfParam.predMethod = 0;
    for (varInd = 0; varInd < filters_per_fr_best; varInd++)
    {
      if (codedVarBins[varInd] == 0)
      {
        memset(m_filterCoeffSym[varInd], 0, sizeof(Int)*m_MAX_SQR_FILT_LENGTH);
        memset(m_filterCoeffSymQuant[varInd], 0, sizeof(Int)*m_MAX_SQR_FILT_LENGTH);
      }
    }
  }
#endif
  for (Int ind = 0; ind < alfParam.filters_per_group; ++ind)
  {
    for(Int i = 0; i < sqrFiltLength; i++)
    {
      if (alfParam.predMethod)
      {
        alfParam.coeffmulti[ind][i] = m_diffFilterCoeffQuant[ind][i];
      }
      else
      {
        alfParam.coeffmulti[ind][i] = m_filterCoeffSymQuant[ind][i];
      }
    }
  }
  memcpy(alfParam.filterPattern, m_varIndTab, m_NO_VAR_BINS * sizeof(Int));
#if JVET_C0038_NO_PREV_FILTERS
  memcpy(alfParam.PrevFiltIdx, usePrevFiltBest, m_NO_VAR_BINS * sizeof(Int));

  Int predPattern;

  // frame that filter is predicted from
  if (alfParam.iAvailableFilters > 0)
  {
    // prediction pattern
    predPattern = alfParam.PrevFiltIdx[0] > 0 ? 1 : 0;
    for (i = 1; i < m_NO_VAR_BINS; i++)
    {
      Int iCurrPredPattern = alfParam.PrevFiltIdx[i] > 0 ? 1 : 0;
      if (iCurrPredPattern != predPattern)
      {
        predPattern = 2;
        break;
      }
    }
    alfParam.iPredPattern = predPattern;
  }
#endif

  if (m_updateMatrix)
  {
    for (Int i = 0; i< m_NO_VAR_BINS; i++)
    {
      for (Int j = 0; j< m_MAX_SQR_FILT_LENGTH; j++)
      {
        y_temp9x9[i][j] = y_temp[i][j];
      }
    }
  }
}

Void EncAdaptiveLoopFilter::xCheckFilterMergingAlf(ALFParam& alfParam
#if JVET_C0038_NO_PREV_FILTERS
  , const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf
#endif
)
{
  static double ***E_temp;
  static double  **y_temp;
  static double   *pixAcc_temp;
  static int     **FilterCoeffQuantTemp;
  static int       first = 0;

  if (first==0)
  {
    initMatrix3D_double(&E_temp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH, m_MAX_SQR_FILT_LENGTH);
    initMatrix_double  (&y_temp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
    pixAcc_temp = (double *) calloc(m_NO_VAR_BINS, sizeof(double));
    initMatrix_int(&FilterCoeffQuantTemp, m_NO_VAR_BINS, m_MAX_SQR_FILT_LENGTH);
    first=1;
  }

  AlfFilterType filtType = alfParam.filterType;

  double ***ESym = m_EGlobalSym[filtType];
  double **ySym  = m_yGlobalSym[filtType];
  double *pixAcc = m_pixAcc;
  int **filterCoeffSym      = m_filterCoeffSym; //TODO Why do we need this here?
  int **filterCoeffSymQuant = m_filterCoeffSymQuant;

  Int filters_per_fr_best=0;
  Int filters_per_fr, firstFilt;
  Int interval[m_NO_VAR_BINS][2], intervalBest[m_NO_VAR_BINS][2];
  Int i, k, varInd;
  Double  lagrangian, lagrangianMin;
  Int lambda =  ((Int)m_dLambdaLuma) * (1<<(2*m_nBitIncrement));
  Int sqrFiltLength;

  Double errorForce0CoeffTab[m_NO_VAR_BINS][2];

  sqrFiltLength = m_sqrFiltLengthTab[ filtType];

  memcpy(pixAcc_temp, pixAcc, sizeof(double)*m_NO_VAR_BINS);

  for (varInd=0; varInd<m_NO_VAR_BINS; varInd++)
  {
    memcpy(y_temp[varInd], ySym[varInd], sizeof(double)*sqrFiltLength);
    for (k = 0; k < sqrFiltLength; k++)
    {
      memcpy(E_temp[varInd][k], ESym[varInd][k], sizeof(double)*sqrFiltLength);
    }
  }
  for (i = 0; i < m_NO_VAR_BINS; i++)
  {
    memset(filterCoeffSym[i], 0, sizeof(int)*m_MAX_SQR_FILT_LENGTH);
    memset(filterCoeffSymQuant[i], 0, sizeof(int)*m_MAX_SQR_FILT_LENGTH);
  }

  firstFilt=1;  lagrangianMin=0;
  filters_per_fr = m_NO_FILTERS;
  Int predMode     = 0;
  Int bestPredMode = 0;

  while(filters_per_fr >=1 )
  {
    xMergeFiltersGreedy(E_temp, y_temp, pixAcc_temp, interval, sqrFiltLength, filters_per_fr);
    Double dist = xCalcFilterCoeffs(E_temp, y_temp, pixAcc_temp, interval, filters_per_fr, filtType,
      filterCoeffSym, filterCoeffSymQuant, errorForce0CoeffTab);

    Int coeffBits = xCheckFilterPredictionMode(filterCoeffSymQuant, filters_per_fr, filtType, predMode);
    lagrangian = dist + lambda * coeffBits;
    if( lagrangian<=lagrangianMin || firstFilt==1)
    {
      firstFilt=0;
      lagrangianMin=lagrangian;

      filters_per_fr_best = filters_per_fr;
      bestPredMode        = predMode;
      memcpy(intervalBest, interval, m_NO_VAR_BINS*2*sizeof(int));
    }
    filters_per_fr--;
  }
  xCalcFilterCoeffs( E_temp, y_temp, pixAcc_temp, intervalBest, filters_per_fr_best, filtType,
                     filterCoeffSym, filterCoeffSymQuant , errorForce0CoeffTab);
  xCheckFilterPredictionMode(filterCoeffSymQuant, filters_per_fr_best, filtType, predMode);
  CHECK( predMode != bestPredMode, "wrong re-calculation" )

  //set alfParameter
  alfParam.filters_per_group_diff = filters_per_fr_best;
  alfParam.filters_per_group      = filters_per_fr_best;
  alfParam.predMethod             = bestPredMode;

  for (Int ind = 0; ind < alfParam.filters_per_group; ++ind)
  {
    for(Int i = 0; i < sqrFiltLength; i++)
    {
      if (alfParam.predMethod)
      {
        alfParam.coeffmulti[ind][i] = m_diffFilterCoeffQuant[ind][i];
      }
      else
      {
        alfParam.coeffmulti[ind][i] = m_filterCoeffSymQuant[ind][i];
      }
    }
  }

  memset(m_varIndTab, 0, sizeof(int)*m_NO_VAR_BINS);
  if( filters_per_fr_best > 2 )
  {
    alfParam.filterMode = ALF_MULTIPLE_FILTERS;
    memset(alfParam.filterPattern, 0, m_NO_VAR_BINS * sizeof(int));
    for(Int filtIdx = 0; filtIdx < filters_per_fr_best; filtIdx++)
    {
      for(k = intervalBest[filtIdx][0]; k <= intervalBest[filtIdx][1]; k++)
      {
        m_varIndTab[k] = filtIdx;
      }
      if( filtIdx > 0 )
      {
        alfParam.filterPattern[ intervalBest[filtIdx][0] ] = 1;
      }
    }
  }
  else if( filters_per_fr_best == 2 )
  {
    alfParam.filterMode        = ALF_TWO_FILTERS;
    alfParam.startSecondFilter = intervalBest[1][0];
    for( k = intervalBest[1][0]; k <= intervalBest[1][1]; k++ )
    {
      m_varIndTab[k] = 1;
    }
  }
  else
  {
    alfParam.filterMode = ALF_ONE_FILTER;
  }
}

Void EncAdaptiveLoopFilter::xMergeFiltersGreedyGalf(Double ***EGlobalSeq, Double **yGlobalSeq, Double *pixAccGlobalSeq, Int intervalBest[m_NO_VAR_BINS][m_NO_VAR_BINS], Int sqrFiltLength)
{
  Int first, ind, ind1, ind2, noRemaining, i, j, exist, indexList[m_NO_VAR_BINS], indexListTemp[m_NO_VAR_BINS], available[m_NO_VAR_BINS], bestToMerge[2];
  Double error, error1, error2, errorMin;
  static Double *y_temp, **E_temp, pixAcc_temp;
  static Int init = 0;
  if (init == 0)
  {
    initMatrix_double(&E_temp, m_MAX_SQR_FILT_LENGTH, m_MAX_SQR_FILT_LENGTH);
    y_temp = new Double[m_MAX_SQR_FILT_LENGTH];
    init = 1;
  }

  noRemaining = m_NO_VAR_BINS;
  for (ind = 0; ind<m_NO_VAR_BINS; ind++)
  {
    intervalBest[noRemaining - 1][ind] = ind;
    indexList[ind] = ind;
    available[ind] = 1;
    m_pixAcc_merged[ind] = pixAccGlobalSeq[ind];

    memcpy(m_y_merged[ind], yGlobalSeq[ind], sizeof(Double)*sqrFiltLength);
    for (i = 0; i < sqrFiltLength; i++)
    {
      memcpy(m_E_merged[ind][i], EGlobalSeq[ind][i], sizeof(Double)*sqrFiltLength);
    }
  }
  for (ind = 0; ind<m_NO_VAR_BINS; ind++)
  {
    intervalBest[0][ind] = 0;
  }

  // Try merging different matrices

  while (noRemaining > 2)
  {
    errorMin = 0; first = 1; bestToMerge[0] = 0; bestToMerge[1] = 1;
    for (ind1 = 0; ind1<m_NO_VAR_BINS - 1; ind1++)
    {
      if (available[ind1])
      {
        for (ind2 = ind1 + 1; ind2<m_NO_VAR_BINS; ind2++)
        {
          if (available[ind2])
          {
            error1 = calculateErrorAbs(m_E_merged[ind1], m_y_merged[ind1], m_pixAcc_merged[ind1], sqrFiltLength);
            error2 = calculateErrorAbs(m_E_merged[ind2], m_y_merged[ind2], m_pixAcc_merged[ind2], sqrFiltLength);

            pixAcc_temp = m_pixAcc_merged[ind1] + m_pixAcc_merged[ind2];
            for (i = 0; i<sqrFiltLength; i++) {
              y_temp[i] = m_y_merged[ind1][i] + m_y_merged[ind2][i];
              for (j = 0; j<sqrFiltLength; j++) {
                E_temp[i][j] = m_E_merged[ind1][i][j] + m_E_merged[ind2][i][j];
              }
            }
            error = calculateErrorAbs(E_temp, y_temp, pixAcc_temp, sqrFiltLength) - error1 - error2;

            if (error<errorMin || first == 1)
            {
              errorMin = error;
              bestToMerge[0] = ind1;
              bestToMerge[1] = ind2;
              first = 0;
            }
          }
        }
      }
    }

    ind1 = bestToMerge[0];
    ind2 = bestToMerge[1];

    m_pixAcc_merged[ind1] += m_pixAcc_merged[ind2];
    for (i = 0; i<sqrFiltLength; i++)
    {
      m_y_merged[ind1][i] += m_y_merged[ind2][i];
      for (j = 0; j<sqrFiltLength; j++)
      {
        m_E_merged[ind1][i][j] += m_E_merged[ind2][i][j];
      }
    }

    available[ind2] = 0;

    for (i = 0; i<m_NO_VAR_BINS; i++)
    {
      if (indexList[i] == ind2)
      {
        indexList[i] = ind1;
      }
    }
    noRemaining--;
    if (noRemaining <= m_NO_VAR_BINS)
    {
      for (i = 0; i<m_NO_VAR_BINS; i++)
      {
        indexListTemp[i] = indexList[i];
      }

      exist = 0; ind = 0;
      for (j = 0; j<m_NO_VAR_BINS; j++)
      {
        exist = 0;
        for (i = 0; i<m_NO_VAR_BINS; i++)
        {
          if (indexListTemp[i] == j)
          {
            exist = 1;
            break;
          }
        }

        if (exist)
        {
          for (i = 0; i<m_NO_VAR_BINS; i++)
          {
            if (indexListTemp[i] == j)
            {
              intervalBest[noRemaining - 1][i] = ind;
              indexListTemp[i] = -1;
            }
          }
          ind++;
        }
      }
    }
  }
}

Double EncAdaptiveLoopFilter::xMergeFiltersGreedy(Double ***EGlobalSeq, Double **yGlobalSeq, Double *pixAccGlobalSeq, Int intervalBest[m_NO_VAR_BINS][2], Int sqrFiltLength, Int noIntervals)
{
  static double pixAcc_temp;
  static double error_tab[m_NO_VAR_BINS];
  static double error_comb_tab[m_NO_VAR_BINS];
  static int    indexList[m_NO_VAR_BINS];
  static int    available[m_NO_VAR_BINS];
  static int    noRemaining;

  int first, ind, ind1, ind2, i, j, bestToMerge ;
  double error, error1, error2, errorMin;

  if (noIntervals == m_NO_FILTERS)
  {
    noRemaining=m_NO_VAR_BINS;
    for (ind=0; ind<m_NO_VAR_BINS; ind++)
    {
      indexList[ind]=ind;
      available[ind]=1;
      m_pixAcc_merged[ind]=pixAccGlobalSeq[ind];
      memcpy(m_y_merged[ind],yGlobalSeq[ind],sizeof(double)*sqrFiltLength);
      for (i=0; i<sqrFiltLength; i++)
      {
        memcpy(m_E_merged[ind][i],EGlobalSeq[ind][i],sizeof(double)*sqrFiltLength);
      }
    }
  }
  // Try merging different matrices
  if (noIntervals == m_NO_FILTERS)
  {
    for (ind=0; ind<m_NO_VAR_BINS; ind++)
    {
      error_tab[ind]=calculateErrorAbs(m_E_merged[ind], m_y_merged[ind], m_pixAcc_merged[ind], sqrFiltLength);
    }
    for (ind=0; ind<m_NO_VAR_BINS-1; ind++)
    {
      ind1=indexList[ind];
      ind2=indexList[ind+1];

      error1=error_tab[ind1];
      error2=error_tab[ind2];

      pixAcc_temp=m_pixAcc_merged[ind1]+m_pixAcc_merged[ind2];
      for (i=0; i<sqrFiltLength; i++)
      {
        m_y_temp[i]=m_y_merged[ind1][i]+m_y_merged[ind2][i];
        for (j=0; j<sqrFiltLength; j++)
        {
          m_E_temp[i][j]=m_E_merged[ind1][i][j]+m_E_merged[ind2][i][j];
        }
      }
      error_comb_tab[ind1]=calculateErrorAbs(m_E_temp, m_y_temp, pixAcc_temp, sqrFiltLength)-error1-error2;
    }
  }

  while( noRemaining > noIntervals )
  {
    errorMin=0; first=1;
    bestToMerge = 0;
    for (ind=0; ind<noRemaining-1; ind++)
    {
      error = error_comb_tab[indexList[ind]];
      if ((error<errorMin || first==1))
      {
        errorMin=error;
        bestToMerge=ind;
        first=0;
      }
    }
    ind1=indexList[bestToMerge];
    ind2=indexList[bestToMerge+1];
    m_pixAcc_merged[ind1]+=m_pixAcc_merged[ind2];
    for (i=0; i<sqrFiltLength; i++)
    {
      m_y_merged[ind1][i]+=m_y_merged[ind2][i];
      for (j=0; j<sqrFiltLength; j++)
      {
        m_E_merged[ind1][i][j]+=m_E_merged[ind2][i][j];
      }
    }
    available[ind2] = 0;

    //update error tables
    error_tab[ind1]=error_comb_tab[ind1]+error_tab[ind1]+error_tab[ind2];
    if (indexList[bestToMerge] > 0)
    {
      ind1=indexList[bestToMerge-1];
      ind2=indexList[bestToMerge];
      error1=error_tab[ind1];
      error2=error_tab[ind2];
      pixAcc_temp=m_pixAcc_merged[ind1]+m_pixAcc_merged[ind2];
      for (i=0; i<sqrFiltLength; i++)
      {
        m_y_temp[i]=m_y_merged[ind1][i]+m_y_merged[ind2][i];
        for (j=0; j<sqrFiltLength; j++)
        {
          m_E_temp[i][j]=m_E_merged[ind1][i][j]+m_E_merged[ind2][i][j];
        }
      }
      error_comb_tab[ind1]=calculateErrorAbs(m_E_temp, m_y_temp, pixAcc_temp, sqrFiltLength)-error1-error2;
    }
    if (indexList[bestToMerge+1] < m_NO_VAR_BINS-1)
    {
      ind1=indexList[bestToMerge];
      ind2=indexList[bestToMerge+2];
      error1=error_tab[ind1];
      error2=error_tab[ind2];
      pixAcc_temp=m_pixAcc_merged[ind1]+m_pixAcc_merged[ind2];
      for (i=0; i<sqrFiltLength; i++)
      {
        m_y_temp[i]=m_y_merged[ind1][i]+m_y_merged[ind2][i];
        for (j=0; j<sqrFiltLength; j++)
        {
          m_E_temp[i][j]=m_E_merged[ind1][i][j]+m_E_merged[ind2][i][j];
        }
      }
      error_comb_tab[ind1]=calculateErrorAbs(m_E_temp, m_y_temp, pixAcc_temp, sqrFiltLength)-error1-error2;
    }

    ind=0;
    for (i=0; i<m_NO_VAR_BINS; i++)
    {
      if (available[i]==1)
      {
        indexList[ind]=i;
        ind++;
      }
    }
    noRemaining--;
  }

  errorMin = 0;
  for (ind=0; ind<noIntervals; ind++)
  {
    errorMin+=error_tab[indexList[ind]];
  }

  for (ind=0; ind < noIntervals-1; ind++)
  {
    intervalBest[ind][0]=indexList[ind]; intervalBest[ind][1]=indexList[ind+1]-1;
  }

  intervalBest[noIntervals-1][0] = indexList[noIntervals-1];
  intervalBest[noIntervals-1][1] = m_NO_VAR_BINS-1;

  return(errorMin);
}



//********************************
// CLASSIFICATION
//********************************

Void EncAdaptiveLoopFilter::xSetInitialMask( const CPelBuf& recBufExt )
{
  xClassify(m_varImg, recBufExt, 9 / 2, m_VAR_SIZE);
  m_maskBuf.fill( 1 );
}



//********************************
// CU Adaptation
//********************************
Void EncAdaptiveLoopFilter::xCheckCUAdaptation( CodingStructure& cs, const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf, UInt64& ruiMinRate, UInt64& ruiMinDist, Double& rdMinCost )
{
  ALFParam cFrmAlfParam;
  allocALFParam(&cFrmAlfParam);
  copyALFParam( &cFrmAlfParam, m_pcBestAlfParam );
  Int    Height = orgUnitBuf.get(COMPONENT_Y).height;
  Int    Width = orgUnitBuf.get(COMPONENT_Y).width;

  for( UInt uiDepth = 0; uiDepth < m_uiMaxTotalCUDepth; uiDepth++ )
  {
    Int nBlkSize = ( cs.slice->getSPS()->getMaxCUHeight() * cs.slice->getSPS()->getMaxCUWidth() ) >> ( uiDepth << 1 );
    Int nPicSize = orgUnitBuf.get(COMPONENT_Y).width * orgUnitBuf.get(COMPONENT_Y).height;
    if( ( nBlkSize << 4 ) > nPicSize )
    {
      // block is too large
      continue;
    }

    m_tempPelBuf.get( COMPONENT_Y).copyFrom( recUnitBuf.get(COMPONENT_Y) );
    copyALFParam( m_pcTempAlfParam, &cFrmAlfParam);

    m_pcTempAlfParam->cu_control_flag = 1;
    m_pcTempAlfParam->alf_max_depth = uiDepth;

    for (UInt uiRD = 0; uiRD <= ALF_NUM_OF_REDESIGN; uiRD++)
    {
      if (uiRD)
      {
        // re-design filter coefficients
        xReDesignFilterCoeff( orgUnitBuf, recExtBuf, m_tempPelBuf, 
#if COM16_C806_ALF_TEMPPRED_NUM
          m_pcTempAlfParam,
#endif
          cs.slice->clpRng(COMPONENT_Y) );
      }

      UInt64 uiRate, uiDist;
      Double dCost;
      xSetCUAlfCtrlFlags( cs, orgUnitBuf, recExtBuf, m_tempPelBuf, uiDist, uiDepth, m_pcTempAlfParam ); //set up varImg here
      xCalcRDCost( uiDist, m_pcTempAlfParam, uiRate, dCost );

      if (dCost < rdMinCost )
      {
        rdMinCost = dCost;
        ruiMinDist = uiDist;
        ruiMinRate = uiRate;
        m_bestPelBuf.get(COMPONENT_Y).copyFrom( m_tempPelBuf.get(COMPONENT_Y) );
        copyALFParam( m_pcBestAlfParam, m_pcTempAlfParam );
        if( m_isGALF )
        {
          for (Int i = 0; i < Height; i++)
          {
            for (Int j = 0; j < Width; j++)
            {
              m_maskBestBuf.at(j, i) = m_maskBuf.at(j, i);
            }
          }
        }
      }
    }
  }

  if (m_pcBestAlfParam->cu_control_flag)
  {
    recUnitBuf.get(COMPONENT_Y).copyFrom( m_bestPelBuf.get(COMPONENT_Y) );
    if( m_isGALF )
    {
      for (Int i = 0; i < Height; i++)
      {
        for (Int j = 0; j < Width; j++)
        {
          m_maskBuf.at(j, i) = m_maskBestBuf.at(j, i);
        }
      }
    }
  }
  freeALFParam(&cFrmAlfParam);
}


Void EncAdaptiveLoopFilter::xSetCUAlfCtrlFlags( CodingStructure& cs, const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf, UInt64& ruiDist, UInt uiAlfCtrlDepth, ALFParam *pAlfParam )
{
  ruiDist = 0;
  pAlfParam->num_alf_cu_flag = 0;

  const SPS* sps = cs.slice->getSPS();

  const unsigned widthInCtus  = cs.pcv->widthInCtus;
  const unsigned maxCUSize    = sps->getMaxCUWidth();
  const unsigned alfCtrlSize  = maxCUSize >> uiAlfCtrlDepth;
  const unsigned imgWidth     = orgUnitBuf.get(COMPONENT_Y).width;
  const unsigned imgHeight    = orgUnitBuf.get(COMPONENT_Y).height;

  Partitioner* partitioner = PartitionerFactory::get( *cs.slice );

  for( UInt uiCTUAddr = 0; uiCTUAddr < cs.pcv->sizeInCtus ; uiCTUAddr++ )
  {
    const unsigned  ctuXPosInCtus         = uiCTUAddr % widthInCtus;
    const unsigned  ctuYPosInCtus         = uiCTUAddr / widthInCtus;

    Position ctuPos ( ctuXPosInCtus * maxCUSize, ctuYPosInCtus * maxCUSize );
    UnitArea ctuArea( cs.area.chromaFormat, Area( ctuPos.x, ctuPos.y, maxCUSize, maxCUSize ) );

    for( auto &currCU : cs.traverseCUs( ctuArea ) )
    {
      const Position&    cuPos    = currCU.lumaPos();
      const Int          qtDepth  = currCU.qtDepth;
      const unsigned     qtSize   = maxCUSize >> qtDepth;
      const Position     qtPos0   = Position( (cuPos.x / qtSize)      * qtSize     , (cuPos.y / qtSize)      * qtSize );
      const Position     ctrlPos0 = Position( (cuPos.x / alfCtrlSize) * alfCtrlSize, (cuPos.y / alfCtrlSize) * alfCtrlSize );

      if( qtDepth >= uiAlfCtrlDepth && cuPos == ctrlPos0 )
      {
        UnitArea ctrlArea( cs.area.chromaFormat, Area( cuPos.x, cuPos.y, std::min( alfCtrlSize, imgWidth-cuPos.x ), std::min( alfCtrlSize, imgHeight-cuPos.y ) ) );
        xSetCUAlfCtrlFlag( cs, ctrlArea, orgUnitBuf, recExtBuf, recUnitBuf, ruiDist, pAlfParam );
      }
      else if ( (qtDepth < uiAlfCtrlDepth) && cuPos == qtPos0 )
      {
        UnitArea ctrlArea( cs.area.chromaFormat, Area( cuPos.x, cuPos.y, std::min( qtSize, imgWidth-cuPos.x ), std::min( qtSize, imgHeight-cuPos.y ) ) );
        xSetCUAlfCtrlFlag( cs, ctrlArea, orgUnitBuf, recExtBuf, recUnitBuf, ruiDist, pAlfParam );
      }
    }
  }

  delete partitioner;
}


Void EncAdaptiveLoopFilter::xSetCUAlfCtrlFlag( CodingStructure& cs, const UnitArea alfCtrlArea, const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf, UInt64& ruiDist, ALFParam *pAlfParam)
{

  const Position& blkPos   = alfCtrlArea.lumaPos();
  const Size&     blkSize  = alfCtrlArea.lumaSize();

  CPelBuf orgBufCUs    = orgUnitBuf.get(COMPONENT_Y).subBuf( blkPos, blkSize );
  CPelBuf recBufCUs    =  recExtBuf.get(COMPONENT_Y).subBuf( blkPos, blkSize );

  CPelBuf cfiltBufCUs  = recUnitBuf.get(COMPONENT_Y).subBuf( blkPos, blkSize );
  UIntBuf  maskBufCUs  = m_maskBuf.subBuf( blkPos, blkSize );

  UInt64 uiRecSSD  = xCalcSSD( orgBufCUs, recBufCUs  );
  UInt64 uiFiltSSD = xCalcSSD( orgBufCUs, cfiltBufCUs );

  UInt filterFlag = !!(uiFiltSSD < uiRecSSD );
  maskBufCUs.fill( filterFlag );
  ruiDist += filterFlag ? uiFiltSSD : uiRecSSD;
  pAlfParam->alf_cu_flag[pAlfParam->num_alf_cu_flag] = filterFlag;
  pAlfParam->num_alf_cu_flag++;

  PelBuf filtBufCUs = recUnitBuf.get(COMPONENT_Y).subBuf( blkPos, blkSize );
  if( !filterFlag )
  {
    filtBufCUs.copyFrom( recBufCUs );
  }
}

//********************************
// FILTER FRAME
//********************************
Void EncAdaptiveLoopFilter::xFilterFrame_en(PelUnitBuf& recDstBuf, const PelUnitBuf& recExtBuf, ALFParam& alfParam, const ClpRng& clpRng)
{
  AlfFilterType filtType = alfParam.filterType;
  xDecodeFilter( &alfParam );

  if( m_isGALF )
  {
    xFilterFrame_enGalf(recDstBuf, recExtBuf, filtType,
#if COM16_C806_ALF_TEMPPRED_NUM
    &alfParam, true,
#endif
    clpRng );
  }
  else
  {
    xFilterFrame_enAlf(recDstBuf, recExtBuf, filtType,
#if COM16_C806_ALF_TEMPPRED_NUM
    &alfParam, true,
#endif
    clpRng );
  }
}

//using filter coefficients in m_filterCoeffSym
Void EncAdaptiveLoopFilter::xFilterFrame_enGalf(PelUnitBuf& recDstBuf, const PelUnitBuf& recExtBuf, AlfFilterType filtType,
  #if COM16_C806_ALF_TEMPPRED_NUM 
	  ALFParam *alfParam, Bool updateFilterCoef, 
#endif
  const ClpRng& clpRng )
{
  const PelBuf recExtLuma = recExtBuf.get(COMPONENT_Y);
  const Pel*   srcBuf = recExtLuma.buf;
  const Int    srcStride = recExtLuma.stride;

  PelBuf filtFrLuma = recDstBuf.get(COMPONENT_Y);
  Pel*   dstBuf = filtFrLuma.buf;
  const Int    dstStride = filtFrLuma.stride;
  const Pel* imgY_rec = srcBuf;

  int i, j, y, x;

  int fl;
  int pixelInt;
  int offset = (1<<(m_NUM_BITS - 2));

#if COM16_C806_ALF_TEMPPRED_NUM
  if (updateFilterCoef)
  {
#endif
    xcalcPredFilterCoeff(filtType
#if COM16_C806_ALF_TEMPPRED_NUM
      , alfParam
#endif
      );
#if COM16_C806_ALF_TEMPPRED_NUM
  }
#endif
  fl=m_FILTER_LENGTH/2;
  for (y=0, i = fl; i < m_img_height+fl; i++, y++)
  {
    for (x=0, j = fl; j < m_img_width+fl; j++, x++)
    {
      Int varInd = m_varImg[y][x];
      AlfFilterType filtTypeBig = (AlfFilterType)2;
      pixelInt = xFilterPixel(imgY_rec, &varInd, NULL, NULL, i, j, fl, srcStride, filtTypeBig);
      pixelInt = (int)((pixelInt+offset) >> (m_NUM_BITS - 1));
      dstBuf[y*dstStride + x] = ClipPel(pixelInt, clpRng);  
    }
  }
}

//using filter coefficients in m_filterCoeffSym
Void EncAdaptiveLoopFilter::xFilterFrame_enAlf(PelUnitBuf& recDstBuf, const PelUnitBuf& recExtBuf, AlfFilterType filtType,
  #if COM16_C806_ALF_TEMPPRED_NUM 
	  ALFParam *alfParam, Bool updateFilterCoef, 
#endif
  const ClpRng& clpRng )
{
  const PelBuf recExtLuma = recExtBuf.get(COMPONENT_Y);
  const Pel*   srcBuf = recExtLuma.buf;
  const Int    srcStride = recExtLuma.stride;

  PelBuf filtFrLuma = recDstBuf.get(COMPONENT_Y);
  Pel*   dstBuf = filtFrLuma.buf;
  const Int    dstStride = filtFrLuma.stride;

  const Pel* imgY_rec = srcBuf;
  const Pel* p_imgY_pad, *p_imgY_pad0;

  int var_step_size_w = m_ALF_VAR_SIZE_W;
  int var_step_size_h = m_ALF_VAR_SIZE_H;

  int i, j, y, x;

  int fl;
  int pixelInt;
  int offset = (1<<(m_NUM_BITS - 2));

  int sqrFiltLength;
  sqrFiltLength = m_MAX_SQR_FILT_LENGTH;
#if COM16_C806_ALF_TEMPPRED_NUM
  if (updateFilterCoef)
  {
#endif
    xcalcPredFilterCoeff(filtType
#if COM16_C806_ALF_TEMPPRED_NUM
      , alfParam
#endif
      );
#if COM16_C806_ALF_TEMPPRED_NUM
  }
#endif
  fl=m_FILTER_LENGTH/2;
  for (y=0, i = fl; i < m_img_height+fl; i++, y++)
  {
    for (x=0, j = fl; j < m_img_width+fl; j++, x++)
    {
      int varInd = m_varImg[y / var_step_size_h][x / var_step_size_w];
      int *coef = m_filterCoeffPrevSelected[varInd];

      pixelInt  = m_filterCoeffPrevSelected[varInd][sqrFiltLength-1];

      if( filtType == ALF_FILTER_SYM_5 )
      {
        pixelInt += coef[22]* (imgY_rec[(i-fl+2)*srcStride + j-fl]+imgY_rec[(i-fl-2)*srcStride + j-fl]);

        pixelInt += coef[30]* (imgY_rec[(i-fl+1)*srcStride + j-fl+1]+imgY_rec[(i-fl-1)*srcStride + j-fl-1]);
        pixelInt += coef[31]* (imgY_rec[(i-fl+1)*srcStride + j-fl]  +imgY_rec[(i-fl-1)*srcStride + j-fl]);
        pixelInt += coef[32]* (imgY_rec[(i-fl+1)*srcStride + j-fl-1]+imgY_rec[(i-fl-1)*srcStride + j-fl+1]);

        pixelInt += coef[38]* (imgY_rec[(i-fl)*srcStride + j-fl-2]+imgY_rec[(i-fl)*srcStride + j-fl+2]);
        pixelInt += coef[39]* (imgY_rec[(i-fl)*srcStride + j-fl-1]+imgY_rec[(i-fl)*srcStride + j-fl+1]);
        pixelInt += coef[40]* (imgY_rec[(i-fl)*srcStride + j-fl]);
      }
      else if (filtType == ALF_FILTER_SYM_7)
      {
        pixelInt += coef[13]* (imgY_rec[(i-fl+3)*srcStride + j-fl]+imgY_rec[(i-fl-3)*srcStride + j-fl]);

        p_imgY_pad = imgY_rec + (i-fl+2)*srcStride;
        p_imgY_pad0 = imgY_rec + (i-fl-2)*srcStride;
        pixelInt += coef[21]* (p_imgY_pad[j-fl+1]+p_imgY_pad0[j-fl-1]);
        pixelInt += coef[22]* (p_imgY_pad[j-fl]+p_imgY_pad0[j-fl]);
        pixelInt += coef[23]* (p_imgY_pad[j-fl-1]+p_imgY_pad0[j-fl+1]);

        p_imgY_pad = imgY_rec + (i-fl+1)*srcStride;
        p_imgY_pad0 = imgY_rec + (i-fl-1)*srcStride;
        pixelInt += coef[29]* (p_imgY_pad[j-fl+2]+p_imgY_pad0[j-fl-2]);
        pixelInt += coef[30]* (p_imgY_pad[j-fl+1]+p_imgY_pad0[j-fl-1]);
        pixelInt += coef[31]* (p_imgY_pad[j-fl]+p_imgY_pad0[j-fl]);
        pixelInt += coef[32]* (p_imgY_pad[j-fl-1]+p_imgY_pad0[j-fl+1]);
        pixelInt += coef[33]* (p_imgY_pad[j-fl-2]+p_imgY_pad0[j-fl+2]);

        p_imgY_pad = imgY_rec + (i-fl)*srcStride;
        pixelInt += coef[37]* (p_imgY_pad[j-fl+3]+p_imgY_pad[j-fl-3]);
        pixelInt += coef[38]* (p_imgY_pad[j-fl+2]+p_imgY_pad[j-fl-2]);
        pixelInt += coef[39]* (p_imgY_pad[j-fl+1]+p_imgY_pad[j-fl-1]);
        pixelInt += coef[40]* (p_imgY_pad[j-fl]);

      }
      else if( filtType == ALF_FILTER_SYM_9 )
      {
        p_imgY_pad = imgY_rec + (i-fl+3)*srcStride;
        p_imgY_pad0 = imgY_rec + (i-fl-3)*srcStride;
        pixelInt += coef[12]* (p_imgY_pad[j-fl+1]+p_imgY_pad0[j-fl-1]);
        pixelInt += coef[13]* (p_imgY_pad[j-fl]+p_imgY_pad0[j-fl]);
        pixelInt += coef[14]* (p_imgY_pad[j-fl-1]+p_imgY_pad0[j-fl+1]);

        p_imgY_pad = imgY_rec + (i-fl+2)*srcStride;
        p_imgY_pad0 = imgY_rec + (i-fl-2)*srcStride;
        pixelInt += coef[20]* (p_imgY_pad[j-fl+2]+p_imgY_pad0[j-fl-2]);
        pixelInt += coef[21]* (p_imgY_pad[j-fl+1]+p_imgY_pad0[j-fl-1]);
        pixelInt += coef[22]* (p_imgY_pad[j-fl]+p_imgY_pad0[j-fl]);
        pixelInt += coef[23]* (p_imgY_pad[j-fl-1]+p_imgY_pad0[j-fl+1]);
        pixelInt += coef[24]* (p_imgY_pad[j-fl-2]+p_imgY_pad0[j-fl+2]);

        p_imgY_pad = imgY_rec + (i-fl+1)*srcStride;
        p_imgY_pad0 = imgY_rec + (i-fl-1)*srcStride;
        pixelInt += coef[28]* (p_imgY_pad[j-fl+3]+p_imgY_pad0[j-fl-3]);
        pixelInt += coef[29]* (p_imgY_pad[j-fl+2]+p_imgY_pad0[j-fl-2]);
        pixelInt += coef[30]* (p_imgY_pad[j-fl+1]+p_imgY_pad0[j-fl-1]);
        pixelInt += coef[31]* (p_imgY_pad[j-fl]+p_imgY_pad0[j-fl]);
        pixelInt += coef[32]* (p_imgY_pad[j-fl-1]+p_imgY_pad0[j-fl+1]);
        pixelInt += coef[33]* (p_imgY_pad[j-fl-2]+p_imgY_pad0[j-fl+2]);
        pixelInt += coef[34]* (p_imgY_pad[j-fl-3]+p_imgY_pad0[j-fl+3]);

        p_imgY_pad = imgY_rec + (i-fl)*srcStride;
        pixelInt += coef[36]* (p_imgY_pad[j-fl+4]+p_imgY_pad[j-fl-4]);
        pixelInt += coef[37]* (p_imgY_pad[j-fl+3]+p_imgY_pad[j-fl-3]);
        pixelInt += coef[38]* (p_imgY_pad[j-fl+2]+p_imgY_pad[j-fl-2]);
        pixelInt += coef[39]* (p_imgY_pad[j-fl+1]+p_imgY_pad[j-fl-1]);
        pixelInt += coef[40]* (p_imgY_pad[j-fl]);
      }
      else
      {
        THROW( "Filter Type not known!" );
      }

      pixelInt=(int)((pixelInt+offset) >> (m_NUM_BITS - 1));
      dstBuf[y*dstStride + x] = ClipPel(pixelInt, clpRng);  
    }
  }
}

Void EncAdaptiveLoopFilter::xfindBestFilterPredictor(
#if JVET_C0038_NO_PREV_FILTERS
  Double ***E_temp, Double**y_temp, Double *pixAcc_temp, Int filtType, const Pel* ImgOrg, const Pel* ImgDec, Int orgStride, Int recStride,
  Int* usePrevFiltBest, Int sqrFiltLength, Int iFixedFilters
#endif
  )
{
#if JVET_C0038_NO_PREV_FILTERS
  Int    varInd, i, j, k, yLocal, filterNo;
  Int    ELocal[m_MAX_SQR_FILT_LENGTH];
  Int    usePrevFilt[m_NO_VAR_BINS];

  Double errorMin, error;

  Int noVarBins = m_NO_VAR_BINS;
  Int fl = m_flTab[filtType];
  Int factor = (1 << (AdaptiveLoopFilter::m_NUM_BITS - 1));
  memset(ELocal, 0, sqrFiltLength*sizeof(Int));

  if (bFindBestFixedFilter == false || m_updateMatrix)
  {

    for (varInd = 0; varInd<noVarBins; ++varInd)
    {
      errorMin = 0;
      for (i = 0; i<iFixedFilters; i++)
      {
        filterNo = varInd*iFixedFilters + i;
        error = xTestFixedFilterFast(E_temp, y_temp, pixAcc_temp, m_filterCoeffPrev[filterNo], m_filterCoeffDefault, varInd);
        if (error<errorMin || i == 0)
        {
          errorMin = error;
          usePrevFiltBest[varInd] = i;
        }
      }
    }
    for (varInd = 0; varInd<noVarBins; ++varInd)
    {
      for (i = 0; i < (m_MAX_SQR_FILT_LENGTH / 2 + 1); i++)
      {
        filterNo = (Int)(varInd * iFixedFilters + usePrevFiltBest[varInd]);
        m_filterCoeffFinal[varInd][i] = (Int)(m_filterCoeffPrev[filterNo][i] * factor);
      }
    }

    error = xTestFixedFilter(ImgDec, ImgOrg, ImgDec, usePrevFilt, noVarBins, orgStride, recStride, filtType);
    xPreFilterFr(m_imgY_preFilter, ImgDec, ImgOrg, ImgDec, usePrevFilt, recStride, filtType);

    for (varInd = 0; varInd<noVarBins; ++varInd)
    {
      if (usePrevFilt[varInd] == 1)
      {
        usePrevFiltBest[varInd]++;
      }
      else
      {
        usePrevFiltBest[varInd] = 0;
        for (i = 0; i < AdaptiveLoopFilter::m_MAX_SQT_FILT_SYM_LENGTH; i++)
        {
          m_filterCoeffFinal[varInd][i] = (Int)(m_filterCoeffDefault[i] * factor);
        }
      }
    }
    bFindBestFixedFilter = true;
  }


  // If frNo>0 pixAcc and yGlobalSym have to be calculated again
  if (iFixedFilters)
  {
    if (m_updateMatrix)
    {
      for (varInd = 0; varInd<noVarBins; varInd++)
      {
        pixAcc_temp[varInd] = 0;
        for (k = 0; k<sqrFiltLength; k++)
        {
          y_temp[varInd][k] = 0;
        }
      }
      Int transpose;
      Int flV = AdaptiveLoopFilter::ALFFlHToFlV(fl);
      for (i = fl; i < m_img_height + fl; i++)
      {
        for (j = fl; j < m_img_width + fl; j++)
        {
          if (m_maskBuf.at(j - fl, i - fl))
          {
            memset(ELocal, 0, sqrFiltLength*sizeof(Int));
            varInd = selectTransposeVarInd(m_varImg[i - fl][j - fl], &transpose);
            Int pos = (i - fl)*orgStride + (j - fl);
            yLocal = ImgOrg[pos] - m_imgY_preFilter[i - fl][j - fl];
            calcMatrixE(ELocal, ImgDec, m_patternTab[filtType], i - fl, j - fl, flV, fl, transpose, recStride);
            for (k = 0; k<sqrFiltLength; k++)
            {
              y_temp[varInd][k] += (Double)(ELocal[k] * yLocal);
            }
            pixAcc_temp[varInd] += (yLocal*yLocal);
          }
        }
      }
    }
  }
#else
  Int varInd, i;
  Int factor = (1 << (AdaptiveLoopFilter::m_NUM_BITS - 1));

  for (varInd = 0; varInd<m_NO_VAR_BINS; ++varInd)
  {
    for (i = 0; i < AdaptiveLoopFilter::m_MAX_SQT_FILT_SYM_LENGTH; i++)
    {
      m_filterCoeffFinal[varInd][i] = (Int)(m_filterCoeffDefault[i] * factor);
    }
  }
#endif
}

//********************************
// CHOLESKY
//********************************

#define ROUND(a)  (((a) < 0)? (int)((a) - 0.5) : (int)((a) + 0.5))
#define REG              0.0001
#define REG_SQR          0.0000001

//Find filter coeff related
Int EncAdaptiveLoopFilter::gnsCholeskyDec(double **inpMatr, double outMatr[m_MAX_SQR_FILT_LENGTH][m_MAX_SQR_FILT_LENGTH], int noEq)
{
  int
  i, j, k;     /* Looping Variables */
  double
  scale;       /* scaling factor for each row */
  double
  invDiag[m_MAX_SQR_FILT_LENGTH];  /* Vector of the inverse of diagonal entries of outMatr */


  /*
   *  Cholesky decomposition starts
   */

  for(i = 0; i < noEq; i++)
  {
    for(j = i; j < noEq; j++)
    {
      /* Compute the scaling factor */
      scale=inpMatr[i][j];
      if ( i > 0) for( k = i - 1 ; k >= 0 ; k--)
        scale -= outMatr[k][j] * outMatr[k][i];

      /* Compute i'th row of outMatr */
      if(i==j)
      {
        if(scale <= REG_SQR ) // if(scale <= 0 )  /* If inpMatr is singular */
        {
          return(0);
        }
        else              /* Normal operation */
          invDiag[i] =  1.0/(outMatr[i][i]=sqrt(scale));
      }
      else
      {
        outMatr[i][j] = scale*invDiag[i]; /* Upper triangular part          */
        outMatr[j][i] = 0.0;              /* Lower triangular part set to 0 */
      }
    }
  }
  return(1); /* Signal that Cholesky factorization is successfully performed */
}

Void EncAdaptiveLoopFilter::gnsTransposeBacksubstitution(double U[m_MAX_SQR_FILT_LENGTH][m_MAX_SQR_FILT_LENGTH], double rhs[], double x[], int order)
{
  int
  i,j;              /* Looping variables */
  double
  sum;              /* Holds backsubstitution from already handled rows */

  /* Backsubstitution starts */
  x[0] = rhs[0]/U[0][0];               /* First row of U'                   */
  for (i = 1; i < order; i++)
  {         /* For the rows 1..order-1           */

    for (j = 0, sum = 0.0; j < i; j++) /* Backsubst already solved unknowns */
      sum += x[j]*U[j][i];

    x[i]=(rhs[i] - sum)/U[i][i];       /* i'th component of solution vect.  */
  }
}



Void EncAdaptiveLoopFilter::gnsBacksubstitution(double R[m_MAX_SQR_FILT_LENGTH][m_MAX_SQR_FILT_LENGTH], double z[m_MAX_SQR_FILT_LENGTH], int R_size, double A[m_MAX_SQR_FILT_LENGTH])
{
  int i, j;
  double sum;

  R_size--;

  A[R_size] = z[R_size] / R[R_size][R_size];

  for (i = R_size-1; i >= 0; i--)
  {
    for (j = i+1, sum = 0.0; j <= R_size; j++)
      sum += R[i][j] * A[j];

    A[i] = (z[i] - sum) / R[i][i];
  }
}

Int EncAdaptiveLoopFilter::gnsSolveByChol( Double **LHS, Double *rhs, Double *x, Int noEq)
{
  double aux[m_MAX_SQR_FILT_LENGTH];     /* Auxiliary vector */
  double U[m_MAX_SQR_FILT_LENGTH][m_MAX_SQR_FILT_LENGTH];    /* Upper triangular Cholesky factor of LHS */
  int  i, singular;          /* Looping variable */

  /* The equation to be solved is LHSx = rhs */

  /* Compute upper triangular U such that U'*U = LHS */
  if(gnsCholeskyDec(LHS, U, noEq)) /* If Cholesky decomposition has been successful */
  {
    singular=1;
    /* Now, the equation is  U'*U*x = rhs, where U is upper triangular
     * Solve U'*aux = rhs for aux
     */
    gnsTransposeBacksubstitution(U, rhs, aux, noEq);

    /* The equation is now U*x = aux, solve it for x (new motion coefficients) */
    gnsBacksubstitution(U, aux, noEq, x);

  }
  else /* LHS was singular */
  {
    singular=0;

    /* Regularize LHS */
    for(i=0; i<noEq; i++)
      LHS[i][i] += REG;
    /* Compute upper triangular U such that U'*U = regularized LHS */
    singular = gnsCholeskyDec(LHS, U, noEq);
    /* Solve  U'*aux = rhs for aux */
    gnsTransposeBacksubstitution(U, rhs, aux, noEq);

    /* Solve U*x = aux for x */
    gnsBacksubstitution(U, aux, noEq, x);
  }
  return(singular);
}


//////////////////////////////////////////////////////////////////////////////////////////

Void EncAdaptiveLoopFilter::add_A_galf(Double **Amerged, Double ***A, Int interval[], Int filtNo, Int size)
{
  Int i, j, ind;

  for (i = 0; i < size; i++)
  {
    for (j = 0; j < size; j++)
    {
      Amerged[i][j] = 0;
      for (ind = 0; ind < m_NO_VAR_BINS; ind++)
      {
        if (interval[ind] == filtNo)
        {
          Amerged[i][j] += A[ind][i][j];
        }
      }
    }
  }
}

Void EncAdaptiveLoopFilter::add_b_galf(Double *bmerged, Double **b, Int interval[], Int filtNo, Int size)
{
  Int i, ind;

  for (i = 0; i < size; i++)
  {
    bmerged[i] = 0;
    for (ind = 0; ind < m_NO_VAR_BINS; ind++)
    {
      if (interval[ind] == filtNo)
      {
        bmerged[i] += b[ind][i];
      }
    }
  }
}

Void EncAdaptiveLoopFilter::add_A(double **Amerged, double ***A, int start, int stop, int size)
{
  int
  i, j, ind;          /* Looping variable */

  for (i=0; i<size; i++)
  {
    for (j=0; j<size; j++)
    {
      Amerged[i][j]=0;
      for (ind=start; ind<=stop; ind++)
      {
        Amerged[i][j]+=A[ind][i][j];
      }
    }
  }
}

Void EncAdaptiveLoopFilter::add_b(double *bmerged, double **b, int start, int stop, int size)
{
  int
  i, ind;          /* Looping variable */

  for (i=0; i<size; i++)
  {
    bmerged[i]=0;
    for (ind=start; ind<=stop; ind++)
    {
      bmerged[i]+=b[ind][i];
    }
  }
}

Double EncAdaptiveLoopFilter::xCalcErrorForGivenWeights(Double **E, Double *y, Double *w, Int size )
{
  Int i, j;
  Double error, sum=0;

  error=0;
  for (i=0; i<size; i++)   //diagonal
  {
    sum=0;
    for (j=i+1; j<size; j++)
      sum+=(E[j][i]+E[i][j])*w[j];
    error+=(E[i][i]*w[i]+sum-2*y[i])*w[i];
  }

  return(error);
}

double EncAdaptiveLoopFilter::calculateErrorAbs(double **A, double *b, double y, int size)
{
  int i;
  double error, sum;
  double c[m_MAX_SQR_FILT_LENGTH];

  gnsSolveByChol(A, b, c, size);

  sum=0;
  for (i=0; i<size; i++)
  {
    sum+=c[i]*b[i];
  }
  error=y-sum;

  return(error);
}



Int EncAdaptiveLoopFilter::xCheckFilterPredictionMode( Int **pDiffQFilterCoeffIntPP, Int filters_per_group, AlfFilterType filtType, Int& predMode )
{
  Int sqrFiltLength = m_sqrFiltLengthTab[ filtType ];
  Int bit_ct0 = xcodeFilterCoeff( pDiffQFilterCoeffIntPP, filters_per_group, filtType );
  for( Int ind = 0; ind < filters_per_group; ++ind )
  {
    if( ind == 0)
    {
      for( Int i = 0; i < sqrFiltLength; i++)
      {
        m_diffFilterCoeffQuant[ind][i] = pDiffQFilterCoeffIntPP[ind][i];
      }
    }
    else
    {
      for( Int i = 0; i < sqrFiltLength; i++)
      {
        m_diffFilterCoeffQuant[ind][i] = pDiffQFilterCoeffIntPP[ind][i] - pDiffQFilterCoeffIntPP[ind-1][i];
      }
    }
  }
  Int bit_pred1  = xcodeFilterCoeff( m_diffFilterCoeffQuant, filters_per_group, filtType);
  predMode = !!( bit_pred1 < bit_ct0 );
  return 1 + (predMode ? bit_pred1 :  bit_ct0);
}

#if FORCE0
Int EncAdaptiveLoopFilter::xCalcBitsForce0(Int **pDiffQFilterCoeffIntPP, Int filters_per_group, AlfFilterType filtType, Int *codedVarBins)
{
  Int sqrFiltLength = m_sqrFiltLengthTab[filtType];
  int ind, i, j, len;
  i = 0;
  for (ind = 0; ind < filters_per_group; ind++)
  {
    if (codedVarBins[ind] == 1)
    {
      for (j = 0; j < sqrFiltLength; j++)

        m_FilterCoeffQuantTemp[ind][j] = pDiffQFilterCoeffIntPP[ind][j];

      i++;
    }
    else
    {
      for (j = 0; j < sqrFiltLength; j++)
      {
        m_FilterCoeffQuantTemp[ind][j] = 0;
      }
    }
  }

  len = 1;
  len += xcodeFilterCoeffForce0(m_FilterCoeffQuantTemp, filters_per_group, filtType, codedVarBins);
  return (len);
}
#endif

#if COM16_C806_ALF_TEMPPRED_NUM
Void EncAdaptiveLoopFilter::xcopyFilterCoeff(int filtNo, int **filterCoeff)
{
  int varInd, i, k;
  const int *patternMap = m_patternMapTab[filtNo];
  for (varInd = 0; varInd<m_NO_VAR_BINS; ++varInd)
  {
    k = 0;
    for (i = 0; i < m_MAX_SQR_FILT_LENGTH; i++)
    {
      if (patternMap[i]>0)
      {
        m_filterCoeffPrevSelected[varInd][i] = filterCoeff[varInd][k];
        k++;
      }
      else
      {
        m_filterCoeffPrevSelected[varInd][i] = 0;
      }
    }
  }
}
#endif

Int EncAdaptiveLoopFilter::xcodeFilterCoeff(Int **pDiffQFilterCoeffIntPP, Int filters_per_group, AlfFilterType filtType)
{
  int i, k, kMin, kStart, minBits, ind, scanPos, maxScanVal, coeffVal, len = 0,
    kMinTab[m_MAX_SQR_FILT_LENGTH], bitsCoeffScan[m_MAX_SCAN_VAL][m_MAX_EXP_GOLOMB],
    minKStart, minBitsKStart, bitsKStart;

  const Int * pDepthInt = AdaptiveLoopFilter::m_pDepthIntTab[filtType];
  Int sqrFiltLength = m_sqrFiltLengthTab[filtType];
  sqrFiltLength = sqrFiltLength - !!m_isGALF;

  maxScanVal = 0;
  for(i = 0; i < sqrFiltLength; i++)
  {
    maxScanVal = std::max(maxScanVal, pDepthInt[i]);
  }

  // vlc for all
  memset(bitsCoeffScan, 0, m_MAX_SCAN_VAL * m_MAX_EXP_GOLOMB * sizeof(int));
  Int sumCoeffs = 0;
  for(ind=0; ind<filters_per_group; ++ind)
  {
    for(i = 0; i < sqrFiltLength; i++)
    {
      scanPos=pDepthInt[i]-1;
      coeffVal=abs(pDiffQFilterCoeffIntPP[ind][i]);
      sumCoeffs+=abs(pDiffQFilterCoeffIntPP[ind][i]);
      for (k=1; k<15; k++)
      {
        bitsCoeffScan[scanPos][k]+=lengthGolomb(coeffVal, k);
      }
    }
  }

  minBitsKStart = 0;
  minKStart = -1;
  for(k = 1; k < 8; k++)
  {
    bitsKStart = 0;
    kStart = k;
    for(scanPos = 0; scanPos < maxScanVal; scanPos++)
    {
      kMin = kStart;
      minBits = bitsCoeffScan[scanPos][kMin];

      if(bitsCoeffScan[scanPos][kStart+1] < minBits)
      {
        kMin = kStart + 1;
        minBits = bitsCoeffScan[scanPos][kMin];
      }
      kStart = kMin;
      bitsKStart += minBits;
    }
    if((bitsKStart < minBitsKStart) || (k == 1))
    {
      minBitsKStart = bitsKStart;
      minKStart = k;
    }
  }

  kStart = minKStart;
  for(scanPos = 0; scanPos < maxScanVal; scanPos++)
  {
    kMin = kStart;
    minBits = bitsCoeffScan[scanPos][kMin];

    if(bitsCoeffScan[scanPos][kStart+1] < minBits)
    {
      kMin = kStart + 1;
      minBits = bitsCoeffScan[scanPos][kMin];
    }

    kMinTab[scanPos] = kMin;
    kStart = kMin;
  }

  // Coding parameters
  //  len += lengthFilterCodingParams(minKStart, maxScanVal, kMinTab, createBitstream);
  len += (3 + maxScanVal);

  // Filter coefficients
  len += lengthFilterCoeffs(sqrFiltLength, filters_per_group, pDepthInt, pDiffQFilterCoeffIntPP, kMinTab );

  return len;
}

#if FORCE0
Int   EncAdaptiveLoopFilter::xcodeFilterCoeffForce0(Int **pDiffQFilterCoeffIntPP, Int filters_per_group, AlfFilterType filtType, Int codedVarBins[])
{
  int i, k, kMin, kStart, minBits, ind, scanPos, maxScanVal, coeffVal, len = 0,
    kMinTab[m_MAX_SQR_FILT_LENGTH], bitsCoeffScan[m_MAX_SCAN_VAL][m_MAX_EXP_GOLOMB],
    minKStart, minBitsKStart, bitsKStart;

  const Int * pDepthInt = AdaptiveLoopFilter::m_pDepthIntTab[filtType];
  Int sqrFiltLength = m_sqrFiltLengthTab[filtType] - 1;
  maxScanVal = 0;
  for (i = 0; i < sqrFiltLength; i++)
  {
    maxScanVal = std::max(maxScanVal, pDepthInt[i]);
  }

  // vlc for all
  memset(bitsCoeffScan, 0, m_MAX_SCAN_VAL * m_MAX_EXP_GOLOMB * sizeof(int));
  for (ind = 0; ind<filters_per_group; ++ind)
  {
    if (!codedVarBins[ind])
    {
      continue;
    }
    for (i = 0; i < sqrFiltLength; i++)
    {
      scanPos = pDepthInt[i] - 1;
      coeffVal = abs(pDiffQFilterCoeffIntPP[ind][i]);
      for (k = 1; k<15; k++)
      {
        bitsCoeffScan[scanPos][k] += lengthGolomb(coeffVal, k);
      }
    }
  }

  minBitsKStart = 0;
  minKStart = -1;
  for (k = 1; k < 8; k++)
  {
    bitsKStart = 0;
    kStart = k;
    for (scanPos = 0; scanPos < maxScanVal; scanPos++)
    {
      kMin = kStart;
      minBits = bitsCoeffScan[scanPos][kMin];

      if (bitsCoeffScan[scanPos][kStart + 1] < minBits)
      {
        kMin = kStart + 1;
        minBits = bitsCoeffScan[scanPos][kMin];
      }
      kStart = kMin;
      bitsKStart += minBits;
    }
    if ((bitsKStart < minBitsKStart) || (k == 1))
    {
      minBitsKStart = bitsKStart;
      minKStart = k;
    }
  }

  kStart = minKStart;
  for (scanPos = 0; scanPos < maxScanVal; scanPos++)
  {
    kMin = kStart;
    minBits = bitsCoeffScan[scanPos][kMin];

    if (bitsCoeffScan[scanPos][kStart + 1] < minBits)
    {
      kMin = kStart + 1;
      minBits = bitsCoeffScan[scanPos][kMin];
    }

    kMinTab[scanPos] = kMin;
    kStart = kMin;
  }

  // Coding parameters
  len += (3 + maxScanVal);
  len += filters_per_group;
  // Filter coefficients
  for (ind = 0; ind<filters_per_group; ++ind)
  {
    if (codedVarBins[ind] == 1)
    {
      for (i = 0; i < sqrFiltLength; i++)
      {
        scanPos = pDepthInt[i] - 1;
        len += lengthGolomb(abs(pDiffQFilterCoeffIntPP[ind][i]), kMinTab[scanPos]);
      }
    }
  }

  return len;
}
#endif


Int EncAdaptiveLoopFilter::lengthGolomb(int coeffVal, int k)
{
  int m = 2 << (k - 1);
  int q = coeffVal / m;
  if(coeffVal != 0)
    return(q + 2 + k);
  else
    return(q + 1 + k);
}


Int EncAdaptiveLoopFilter::lengthFilterCoeffs( int sqrFiltLength, int filters_per_group, const int pDepthInt[],
                                               int **FilterCoeff, int kMinTab[])
{
  int ind, scanPos, i;
  int bit_cnt = 0;

  for(ind = 0; ind < filters_per_group; ++ind)
  {
    for(i = 0; i < sqrFiltLength; i++)
    {
      scanPos = pDepthInt[i] - 1;
      bit_cnt += lengthGolomb(abs(FilterCoeff[ind][i]), kMinTab[scanPos]);
    }
  }
  return bit_cnt;
}


//###################################
// Filter Type Decsion
//###################################

Void EncAdaptiveLoopFilter::xFilterTypeDecision(  CodingStructure& cs, const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf, UInt64& ruiMinRate, UInt64& ruiMinDist, Double& rdMinCost, const Slice*  slice)
{

  // restriction for non-referenced B-slice
  if (m_eSliceType == B_SLICE && m_iPicNalReferenceIdc == 0)
  {
    return;
  }

  UInt64 uiRate, uiDist;
  Double dCost;

  Bool bChanged = false;
  const int maxListSize = 3;
  const static int filterTypeList[2][maxListSize] = {{ ALF_FILTER_SYM_5, ALF_FILTER_SYM_7, ALF_FILTER_SYM_9 }, { ALF_FILTER_SYM_9, ALF_FILTER_SYM_7, ALF_FILTER_SYM_5 }};

  for( int n = 0; n < maxListSize; n++) 
  {
    Int filtTypeIdx = filterTypeList[!!m_isGALF][n];
    
    if( m_isGALF )
    {
      if( m_eSliceType == I_SLICE && filtTypeIdx == ALF_FILTER_SYM_9)
      {
        continue;
      }
      if (filtTypeIdx != ALF_FILTER_SYM_9 && !m_pcTempAlfParam->cu_control_flag)
      {
        m_updateMatrix = false;
      }
    }
    AlfFilterType filtType = (AlfFilterType) filtTypeIdx;
    copyALFParam(m_pcTempAlfParam, m_pcBestAlfParam);
    m_pcTempAlfParam->filterType = filtType;
    m_pcTempAlfParam->tapH      = m_mapTypeToNumOfTaps[filtType];
    m_pcTempAlfParam->tapV      = AdaptiveLoopFilter::ALFTapHToTapV    (m_pcTempAlfParam->tapH);
    m_pcTempAlfParam->num_coeff = AdaptiveLoopFilter::ALFTapHToNumCoeff(m_pcTempAlfParam->tapH);

    if (m_pcTempAlfParam->cu_control_flag)
    {
       xReDesignFilterCoeff( orgUnitBuf, recExtBuf, m_tempPelBuf,
#if COM16_C806_ALF_TEMPPRED_NUM
        m_pcTempAlfParam,
#endif
        cs.slice->clpRng(COMPONENT_Y));
       xSetCUAlfCtrlFlags( cs, orgUnitBuf, recExtBuf, m_tempPelBuf, uiDist, m_pcTempAlfParam->alf_max_depth, m_pcTempAlfParam );
       xCalcRDCostLuma( orgUnitBuf,  m_tempPelBuf, m_pcTempAlfParam, uiRate, uiDist, dCost );
    }
    else
    {
      m_maskBuf.fill(1); //TODO why is this needed?
      xReDesignFilterCoeff( orgUnitBuf, recExtBuf, m_tempPelBuf, 
#if COM16_C806_ALF_TEMPPRED_NUM
        m_pcTempAlfParam,
#endif
        cs.slice->clpRng(COMPONENT_Y));
      xCalcRDCostLuma( orgUnitBuf,  m_tempPelBuf, m_pcTempAlfParam, uiRate, uiDist, dCost );
    }

    if (dCost < rdMinCost )
    {
      rdMinCost = dCost;
      ruiMinDist = uiDist;
      ruiMinRate = uiRate;
      m_bestPelBuf.get(COMPONENT_Y).copyFrom( m_tempPelBuf.get(COMPONENT_Y));
      copyALFParam(m_pcBestAlfParam, m_pcTempAlfParam);
      bChanged = true;
    }
  }

  if( bChanged )
  {
   recUnitBuf.get(COMPONENT_Y).copyFrom( m_bestPelBuf.get(COMPONENT_Y) );
  }
  copyALFParam(m_pcTempAlfParam, m_pcBestAlfParam);
}

//redesign using m_pcTempAlfParameter
Void EncAdaptiveLoopFilter::xReDesignFilterCoeff( const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& dstUnitBuf, 
#if COM16_C806_ALF_TEMPPRED_NUM
  ALFParam* alfParam,
#endif
  const ClpRng& clpRng )
{
  AlfFilterType filtType = m_pcTempAlfParam->filterType;
  xStoreInBlockMatrix(orgUnitBuf, recExtBuf, filtType);
//  xFindFilterCoeffsLuma(orgUnitBuf, recExtBuf, filtType);

  if( m_isGALF )
  {
    xCheckFilterMergingGalf( *m_pcTempAlfParam
  #if JVET_C0038_NO_PREV_FILTERS
      , orgUnitBuf, recExtBuf
  #endif
      );
    xFilterFrame_enGalf(dstUnitBuf, recExtBuf, filtType,
   #if COM16_C806_ALF_TEMPPRED_NUM
      alfParam, true,
  #endif
     clpRng );
  }
  else
  {
    xCheckFilterMergingAlf( *m_pcTempAlfParam
  #if JVET_C0038_NO_PREV_FILTERS
      , orgUnitBuf, recExtBuf
  #endif
      );
    xFilterFrame_enAlf(dstUnitBuf, recExtBuf, filtType,
   #if COM16_C806_ALF_TEMPPRED_NUM
      alfParam, true,
  #endif
     clpRng );
  }
}


Void EncAdaptiveLoopFilter::xCalcRDCost( const UInt64 uiDist, ALFParam* pAlfParam, UInt64& ruiRate,  Double& rdCost )
{
  if(pAlfParam != NULL)
  {
    m_CABACEstimator->initCtxModels( *m_pSlice, m_CABACEncoder );
    m_CABACEstimator->resetBits();
    m_CABACEstimator->alf( *pAlfParam, m_uiMaxTotalCUDepth, m_eSliceType, m_isGALF );
    ruiRate = m_CABACEstimator->getEstFracBits() >> SCALE_BITS;
  }
  else
  {
    ruiRate = 1;
  }
  rdCost = (Double)(ruiRate) * m_dLambdaLuma + (Double)(uiDist);
}


Void EncAdaptiveLoopFilter::xCalcRDCostLuma( const CPelUnitBuf& orgUnitBuf, const CPelUnitBuf& recBuf, ALFParam* pAlfParam, UInt64& ruiRate, UInt64& ruiDist, Double& rdCost )
{
  if(pAlfParam != NULL)
  {
    m_CABACEstimator->initCtxModels( *m_pSlice, m_CABACEncoder );
    m_CABACEstimator->resetBits();
    m_CABACEstimator->alf( *pAlfParam, m_uiMaxTotalCUDepth, m_eSliceType, m_isGALF );
    ruiRate = m_CABACEstimator->getEstFracBits() >> SCALE_BITS;
  }
  else
  {
    ruiRate = 1;
  }
  ruiDist = xCalcSSD( orgUnitBuf, recBuf, COMPONENT_Y);
  rdCost  = (Double)(ruiRate) * m_dLambdaLuma + (Double)(ruiDist);
}

UInt64 EncAdaptiveLoopFilter::xCalcSSD(const CPelUnitBuf& OrgBuf, const CPelUnitBuf& CmpBuf,  const ComponentID compId)
{
  return xCalcSSD(OrgBuf.get(compId), CmpBuf.get(compId));
}

UInt64 EncAdaptiveLoopFilter::xCalcSSD(const CPelBuf& refBuf, const CPelBuf& cmpBuf)
{
  Int iWidth = refBuf.width;
  Int iHeight = refBuf.height;
  Int orgStride = refBuf.stride;
  Int cmpStride = cmpBuf.stride;
  const Pel* pOrg = refBuf.buf;
  const Pel* pCmp = cmpBuf.buf;

  UInt64 uiSSD = 0;
  Int x, y;

  UInt uiShift = m_nBitIncrement<<1;
  Int iTemp;

  for( y = 0; y < iHeight; y++ )
  {
    for( x = 0; x < iWidth; x++ )
    {
      iTemp = pOrg[x] - pCmp[x]; uiSSD += ( iTemp * iTemp ) >> uiShift;
    }
    pOrg += orgStride;
    pCmp += cmpStride;
  }
  return uiSSD;;
}



//#####################################
//   CHROMA RELATED
//####################################

Void EncAdaptiveLoopFilter::xCalcRDCostChroma( const CPelUnitBuf& orgUnitBuf, const CPelUnitBuf& recBuf, ALFParam* pAlfParam, UInt64& ruiRate, UInt64& ruiDist, Double& rdCost )
{
  if( pAlfParam->chroma_idc )
  {
    Int* piTmpCoef = nullptr;
    if( ! m_isGALF )
    {
      piTmpCoef = new Int[m_ALF_MAX_NUM_COEF_C];
      memcpy(piTmpCoef, pAlfParam->coeff_chroma, sizeof(Int)*pAlfParam->num_coeff_chroma);
      predictALFCoeffChroma(pAlfParam); //TODO!!!!
    }

    m_CABACEstimator->initCtxModels( *m_pSlice, m_CABACEncoder );
    m_CABACEstimator->resetBits();
    m_CABACEstimator->alf( *pAlfParam, m_uiMaxTotalCUDepth, m_eSliceType, m_isGALF );

    ruiRate = m_CABACEstimator->getEstFracBits() >> SCALE_BITS;
    if( ! m_isGALF )
    {
      memcpy(pAlfParam->coeff_chroma, piTmpCoef, sizeof(int)*pAlfParam->num_coeff_chroma);
      delete[] piTmpCoef;
      piTmpCoef = NULL;
    }
  }

  ruiDist = 0;
  ruiDist += xCalcSSD(orgUnitBuf, recBuf, COMPONENT_Cb);
  ruiDist += xCalcSSD(orgUnitBuf, recBuf, COMPONENT_Cr);
  rdCost  = (Double)(ruiRate) * m_dLambdaChroma + (Double)(ruiDist);
}


Void EncAdaptiveLoopFilter::xFilteringFrameChroma(  const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf )
{
  Int tap;
  Int* qh;

  tap  = m_pcTempAlfParam->tap_chroma;
  qh   = m_pcTempAlfParam->coeff_chroma;

  if( m_isGALF )
  {
    if (m_pcTempAlfParam->chroma_idc)
    {
      Int filtType = 0;
      if (tap == 9) filtType = 2;
      else if (tap == 7) filtType = 1;

      xstoreInBlockMatrixForChroma(orgUnitBuf, recExtBuf, tap, m_pcTempAlfParam->chroma_idc);
      //static double pixAcc_temp;
      Int sqrFiltLength = m_sqrFiltLengthTab[filtType];
      const Int* weights = m_weightsTab[filtType];
      Int  bit_depth = m_NUM_BITS;
      // Find coeffcients
      xSolveAndQuant(m_filterCoeff, qh, m_EGlobalSym[filtType][0], m_yGlobalSym[filtType][0], sqrFiltLength, weights, bit_depth, true);
  #if COM16_C806_ALF_TEMPPRED_NUM
      memcpy(m_pcTempAlfParam->alfCoeffChroma, qh, sizeof(Int)*sqrFiltLength);
  #endif
    }
  }
  else
  {
    Int N = m_pcTempAlfParam->num_coeff_chroma;
    // initialize correlation
    for(int i=0; i<N; i++)
      memset(m_ppdAlfCorr[i], 0, sizeof(Double)*(N+1));

    if ((m_pcTempAlfParam->chroma_idc>>1)&0x01)
    {
      xCalcCorrelationFunc( orgUnitBuf.get(COMPONENT_Cb), recExtBuf.get(COMPONENT_Cb), tap );
    }
    if ((m_pcTempAlfParam->chroma_idc)&0x01)
    {
      xCalcCorrelationFunc( orgUnitBuf.get(COMPONENT_Cr), recExtBuf.get(COMPONENT_Cb), tap );
    }

    int err_code = xGauss(m_ppdAlfCorr, N);

    if(err_code)
    {
      xClearFilterCoefInt(qh, N);
    }
    else
    {
      for(int i=0; i<N; i++)
        m_pdDoubleAlfCoeff[i] = m_ppdAlfCorr[i][N];

      xQuantFilterCoefChroma(m_pdDoubleAlfCoeff, qh, tap, m_nInputBitDepth + m_nBitIncrement);
    }
  }

  if( m_isGALF )
  {
    if (m_pcTempAlfParam->chroma_idc)
    {
      initVarForChroma(m_pcTempAlfParam, true);
    }
    if ((m_pcTempAlfParam->chroma_idc >> 1) & 0x01)
    {
      xFrameChromaGalf(m_pcTempAlfParam, recExtBuf, recUnitBuf, COMPONENT_Cb);
    }
    if ((m_pcTempAlfParam->chroma_idc) & 0x01)
    {
      xFrameChromaGalf(m_pcTempAlfParam, recExtBuf, recUnitBuf, COMPONENT_Cr);
    }
  }
  else
  {
    if ((m_pcTempAlfParam->chroma_idc >> 1) & 0x01)
    {
      xFrameChromaAlf(recExtBuf, recUnitBuf, qh, tap, COMPONENT_Cb);
    }
    if ((m_pcTempAlfParam->chroma_idc) & 0x01)
    {
      xFrameChromaAlf(recExtBuf, recUnitBuf, qh, tap, COMPONENT_Cr);
    }
  }

  if (m_pcTempAlfParam->chroma_idc<3)
  {
    if (m_pcTempAlfParam->chroma_idc==1)
    {
      recUnitBuf.get(COMPONENT_Cb).copyFrom(recExtBuf.get(COMPONENT_Cb));
    }
    if (m_pcTempAlfParam->chroma_idc==2)
    {
      recUnitBuf.get(COMPONENT_Cr).copyFrom(recExtBuf.get(COMPONENT_Cr));
    }
  }

}

#if FORCE0
Void EncAdaptiveLoopFilter::xcollectStatCodeFilterCoeffForce0(int **pDiffQFilterCoeffIntPP, int filtType, int sqrFiltLength,
  int filters_per_group, int bitsVarBin[])
{
  int i, k, kMin, kStart, minBits, ind, scanPos, maxScanVal, coeffVal,
    kMinTab[m_MAX_SQR_FILT_LENGTH], bitsCoeffScan[m_MAX_SCAN_VAL][m_MAX_EXP_GOLOMB],
    minKStart, minBitsKStart, bitsKStart;

  const Int * pDepthInt = AdaptiveLoopFilter::m_pDepthIntTab[filtType];

  sqrFiltLength = sqrFiltLength - !!m_isGALF;

  maxScanVal = 0;
  for (i = 0; i<sqrFiltLength; i++)
  {
    maxScanVal = std::max(maxScanVal, pDepthInt[i]);
  }

  // vlc for all
  memset(bitsCoeffScan, 0, m_MAX_SCAN_VAL * m_MAX_EXP_GOLOMB * sizeof(int));
  for (ind = 0; ind<filters_per_group; ++ind)
  {
    for (i = 0; i < sqrFiltLength; i++)
    {
      scanPos = pDepthInt[i] - 1;
      coeffVal = abs(pDiffQFilterCoeffIntPP[ind][i]);
      for (k = 1; k<15; k++)
      {
        bitsCoeffScan[scanPos][k] += lengthGolomb(coeffVal, k);
      }
    }
  }

  minBitsKStart = 0;
  minKStart = -1;
  for (k = 1; k<8; k++)
  {
    bitsKStart = 0; kStart = k;
    for (scanPos = 0; scanPos<maxScanVal; scanPos++)
    {
      kMin = kStart; minBits = bitsCoeffScan[scanPos][kMin];

      if (bitsCoeffScan[scanPos][kStart + 1]<minBits)
      {
        kMin = kStart + 1; minBits = bitsCoeffScan[scanPos][kMin];
      }
      kStart = kMin;
      bitsKStart += minBits;
    }
    if (bitsKStart<minBitsKStart || k == 1)
    {
      minBitsKStart = bitsKStart;
      minKStart = k;
    }
  }

  kStart = minKStart;
  for (scanPos = 0; scanPos<maxScanVal; scanPos++)
  {
    kMin = kStart; minBits = bitsCoeffScan[scanPos][kMin];

    if (bitsCoeffScan[scanPos][kStart + 1]<minBits)
    {
      kMin = kStart + 1;
      minBits = bitsCoeffScan[scanPos][kMin];
    }

    kMinTab[scanPos] = kMin;
    kStart = kMin;
  }

  for (ind = 0; ind<filters_per_group; ++ind)
  {
    bitsVarBin[ind] = 0;
    for (i = 0; i < sqrFiltLength; i++)
    {
      scanPos = pDepthInt[i] - 1;
      bitsVarBin[ind] += lengthGolomb(abs(pDiffQFilterCoeffIntPP[ind][i]), kMinTab[scanPos]);
    }
  }
}

Void EncAdaptiveLoopFilter::xdecideCoeffForce0(Int codedVarBins[m_NO_VAR_BINS], Double *distForce0, Double errorForce0CoeffTab[m_NO_VAR_BINS][2], Int bitsVarBin[m_NO_VAR_BINS], Double lambda, Int filters_per_fr)
{
  int filtNo;
  int ind;

  *distForce0 = 0;
  for (ind = 0; ind< m_NO_VAR_BINS; ind++)
  {
    codedVarBins[ind] = 0;
  }

  for (filtNo = 0; filtNo<filters_per_fr; filtNo++)
  {
    Double lagrangianDiff = errorForce0CoeffTab[filtNo][0] - (errorForce0CoeffTab[filtNo][1] + lambda*bitsVarBin[filtNo]);
    codedVarBins[filtNo] = (lagrangianDiff>0) ? 1 : 0;
    *distForce0 += errorForce0CoeffTab[filtNo][codedVarBins[filtNo] ? 1 : 0];
  }
}
#endif

Void EncAdaptiveLoopFilter::xCalcCorrelationFunc(const CPelBuf& rcOrgBuf, const CPelBuf& rcCmpBuf, Int iTap)
{
  Int iTapV   = AdaptiveLoopFilter::ALFTapHToTapV(iTap);
  Int N       = (iTap * iTapV + 1) >> 1;
  Int offsetV = iTapV >> 1;
  Int offset = iTap>>1;

  const Int* pFiltPos;

  switch(iTap)
  {
    case 5:
      pFiltPos = m_aiSymmetricArray5x5;
      break;
    case 7:
      pFiltPos = m_aiSymmetricArray7x7;
      break;
    case 9:
      pFiltPos = m_aiSymmetricArray9x7;
      break;
    default:
      pFiltPos = m_aiSymmetricArray9x7;
      THROW( "ALF: wrong number of taps for chroma" );
      break;
  }

  Pel* pTerm = new Pel[N];

  const Pel* pCmp = rcCmpBuf.buf;
  const Pel* pOrg = rcOrgBuf.buf;
  const Int iWidth  = rcOrgBuf.width;
  const Int iHeight = rcOrgBuf.height;
  const Int iOrgStride = rcOrgBuf.stride;
  const Int iCmpStride = rcCmpBuf.stride;
  Int i, j;
  for (Int y = 0; y < iHeight; y++)
  {
    for (Int x = 0; x < iWidth; x++)
    {
      i = 0;
      ::memset(pTerm, 0, sizeof(Pel)*N);
      for (Int yy = y - offsetV; yy <= y + offsetV; yy++)
      {
        for(Int xx=x-offset; xx<=x+offset; xx++)
        {
          pTerm[pFiltPos[i]] += pCmp[xx + yy*iCmpStride];
          i++;
        }
      }

      for(j=0; j<N; j++)
      {
        m_ppdAlfCorr[j][j] += pTerm[j]*pTerm[j];
        for(i=j+1; i<N; i++)
          m_ppdAlfCorr[j][i] += pTerm[j]*pTerm[i];

        // DC offset
        m_ppdAlfCorr[j][N]   += pTerm[j];
        m_ppdAlfCorr[j][N+1] += pOrg[x+y*iOrgStride]*pTerm[j];
      }
      // DC offset
      for(i=0; i<N; i++)
        m_ppdAlfCorr[N][i] += pTerm[i];
      m_ppdAlfCorr[N][N]   += 1;
      m_ppdAlfCorr[N][N+1] += pOrg[x+y*iOrgStride];
    }
  }
  for(j=0; j<N-1; j++)
  {
    for(i=j+1; i<N; i++)
      m_ppdAlfCorr[i][j] = m_ppdAlfCorr[j][i];
  }

  delete[] pTerm;
  pTerm = NULL;
}

Int EncAdaptiveLoopFilter::xGauss(Double **a, Int N)
{
  Int i, j, k;
  Double t;

  for(k=0; k<N; k++)
  {
    if (a[k][k] <0.000001)
      return 1;
  }

  for(k=0; k<N-1; k++)
  {
    for(i=k+1;i<N; i++)
    {
      t=a[i][k]/a[k][k];
      for(j=k+1; j<=N; j++)
      {
        a[i][j] -= t * a[k][j];
        if(i==j && fabs(a[i][j])<0.000001) return 1;
      }
    }
  }
  for(i=N-1; i>=0; i--)
  {
    t = a[i][N];
    for(j=i+1; j<N; j++)
      t -= a[i][j] * a[j][N];
    a[i][N] = t / a[i][i];
  }
  return 0;
}

Void EncAdaptiveLoopFilter::xClearFilterCoefInt(Int* qh, Int N)
{
  // clear
  memset( qh, 0, sizeof( Int ) * N );

  // center pos
  qh[N-2]  = 1<<m_ALF_NUM_BIT_SHIFT;
}


Void EncAdaptiveLoopFilter::xQuantFilterCoefChroma(Double* h, Int* qh, Int tap, int bit_depth)
{
  Int i, N;
  Int max_value, min_value;
  Double dbl_total_gain;
  Int total_gain, q_total_gain;
  Int upper, lower;
  Double *dh;
  Int    *nc;
  const Int    *pFiltMag;

  switch(tap)
  {
    case 5:
      pFiltMag = m_aiSymmetricMag5x5;
      break;
    case 7:
      pFiltMag = m_aiSymmetricMag7x7;
      break;
    case 9:
      pFiltMag = m_aiSymmetricMag9x7;
      break;
    default:
      pFiltMag = m_aiSymmetricMag9x7;
      THROW( "ALF:CHROMA: wrong number of taps" );
      break;
  }

  Int tapV = AdaptiveLoopFilter::ALFTapHToTapV(tap);
  N = (tap * tapV + 1) >> 1;

  dh = new Double[N];
  nc = new Int[N];

  max_value =   (1<<(1+m_ALF_NUM_BIT_SHIFT))-1;
  min_value = 0-(1<<(1+m_ALF_NUM_BIT_SHIFT));

  dbl_total_gain=0.0;
  q_total_gain=0;
  for(i=0; i<N; i++)
  {
    if(h[i]>=0.0)
      qh[i] =  (Int)( h[i]*(1<<m_ALF_NUM_BIT_SHIFT)+0.5);
    else
      qh[i] = -(Int)(-h[i]*(1<<m_ALF_NUM_BIT_SHIFT)+0.5);

    dh[i] = (Double)qh[i]/(Double)(1<<m_ALF_NUM_BIT_SHIFT) - h[i];
    dh[i]*=pFiltMag[i];
    dbl_total_gain += h[i]*pFiltMag[i];
    q_total_gain   += qh[i]*pFiltMag[i];
    nc[i] = i;
  }

  // modification of quantized filter coefficients
  total_gain = (Int)(dbl_total_gain*(1<<m_ALF_NUM_BIT_SHIFT)+0.5);

  if( q_total_gain != total_gain )
  {
    xFilterCoefQuickSort(dh, nc, 0, N-1);
    if( q_total_gain > total_gain )
    {
      upper = N-1;
      while( q_total_gain > total_gain+1 )
      {
        i = nc[upper%N];
        qh[i]--;
        q_total_gain -= pFiltMag[i];
        upper--;
      }
      if( q_total_gain == total_gain+1 )
      {
        if(dh[N-1]>0)
          qh[N-1]--;
        else
        {
          i=nc[upper%N];
          qh[i]--;
          qh[N-1]++;
        }
      }
    }
    else if( q_total_gain < total_gain )
    {
      lower = 0;
      while( q_total_gain < total_gain-1 )
      {
        i=nc[lower%N];
        qh[i]++;
        q_total_gain += pFiltMag[i];
        lower++;
      }
      if( q_total_gain == total_gain-1 )
      {
        if(dh[N-1]<0)
          qh[N-1]++;
        else
        {
          i=nc[lower%N];
          qh[i]++;
          qh[N-1]--;
        }
      }
    }
  }

  // set of filter coefficients
  for(i=0; i<N; i++)
  {
    qh[i] = std::max(min_value,std::min(max_value, qh[i]));
  }

  // DC offset
  max_value = std::min(  (1<<(3+m_nInputBitDepth + m_nBitIncrement))-1, (1<<14)-1);
  min_value = std::max( -(1<<(3+m_nInputBitDepth + m_nBitIncrement)),  -(1<<14)  );

  qh[N] =  (h[N]>=0.0)? (Int)( h[N]*(1<<(m_ALF_NUM_BIT_SHIFT-bit_depth+8)) + 0.5) : -(Int)(-h[N]*(1<<(m_ALF_NUM_BIT_SHIFT-bit_depth+8)) + 0.5);
  qh[N] = std::max(min_value,std::min(max_value, qh[N]));

  delete[] dh;
  dh = NULL;

  delete[] nc;
  nc = NULL;
}

Void EncAdaptiveLoopFilter::xFilterCoefQuickSort( Double *coef_data, Int *coef_num, Int upper, Int lower )
{
  Double mid, tmp_data;
  Int i, j, tmp_num;

  i = upper;
  j = lower;
  mid = coef_data[(lower+upper)>>1];
  do
  {
    while( coef_data[i] < mid ) i++;
    while( mid < coef_data[j] ) j--;
    if( i <= j )
    {
      tmp_data = coef_data[i];
      tmp_num  = coef_num[i];
      coef_data[i] = coef_data[j];
      coef_num[i]  = coef_num[j];
      coef_data[j] = tmp_data;
      coef_num[j]  = tmp_num;
      i++;
      j--;
    }
  } while( i <= j );
  if ( upper < j ) xFilterCoefQuickSort(coef_data, coef_num, upper, j);
  if ( i < lower ) xFilterCoefQuickSort(coef_data, coef_num, i, lower);
}

#if COM16_C806_ALF_TEMPPRED_NUM
Bool EncAdaptiveLoopFilter::xFilteringLumaChroma(CodingStructure& cs, ALFParam *pAlfParam, const PelUnitBuf& orgUnitBuf, const PelUnitBuf& recExtBuf, PelUnitBuf& recUnitBuf, UInt64& ruiMinRate, UInt64& ruiMinDist, Double& rdMinCost, Int uiIndex, const Slice* pSlice)
{
  UInt64 uiRate, uiDist = 0;
  Double dCost;

  //UInt   uiTmpMaxDepth = pAlfParam->alf_max_depth;
  //UInt   uiTmpAlfCtrlFlag = pAlfParam->cu_control_flag;
  m_pcTempAlfParam->temporalPredFlag = true;
  copyALFParam(m_pcTempAlfParam, pAlfParam);
  xcopyFilterCoeff(m_pcTempAlfParam->filterType, m_pcTempAlfParam->alfCoeffLuma);
  m_pcTempAlfParam->cu_control_flag = 0;
  m_pcTempAlfParam->prevIdx = uiIndex;
  m_pcTempAlfParam->alf_flag = 1;
  m_pcTempAlfParam->chroma_idc = 0;
  m_pcBestAlfParam->temporalPredFlag = false;
  m_varImg = m_varImgMethods;

  if( m_isGALF )
  {
    xFilterFrame_enGalf(recUnitBuf, recExtBuf, m_pcTempAlfParam->filterType, m_pcTempAlfParam, false, cs.slice->clpRng(COMPONENT_Y));
  }
  else
  {
    xFilterFrame_enAlf(recUnitBuf, recExtBuf, m_pcTempAlfParam->filterType,  m_pcTempAlfParam, false, cs.slice->clpRng(COMPONENT_Y));
  }

  uiDist = xCalcSSD(orgUnitBuf, recUnitBuf, COMPONENT_Y);
  xCalcRDCost(uiDist, m_pcTempAlfParam, uiRate, dCost);
  if (dCost < rdMinCost)
  {
    rdMinCost = dCost;
    ruiMinDist = uiDist;
    ruiMinRate = uiRate;
    m_pcBestAlfParam->temporalPredFlag = true;
    copyALFParam(m_pcBestAlfParam, m_pcTempAlfParam);
    m_pcBestAlfParam->prevIdx = uiIndex;
    m_pcBestAlfParam->alf_flag = 1;
    m_pcBestAlfParam->cu_control_flag = 0;

    //if (m_pcBestAlfParam->tap >= m_ALF_MIN_NUM_TAP) // fixed a bug with HM-3
    {
      m_bestPelBuf.get(COMPONENT_Y).copyFrom(recUnitBuf.get(COMPONENT_Y));
    }
  }

  m_pcTempAlfParam->cu_control_flag = 1;
  UInt uiBestDepth = 0;
  Bool bChanged = false;

  for (UInt uiDepth = 0; uiDepth < pSlice->getSPS()->getMaxCodingDepth(); uiDepth++)
  {
    m_pcTempAlfParam->alf_max_depth = uiDepth;
    m_tempPelBuf.get(COMPONENT_Y).copyFrom(recUnitBuf.get(COMPONENT_Y));
    UInt64 uiTmpRate, uiTmpDist;
    Double dTmpCost;
    //m_pcPicYuvTmp: filtered signal, pcPicDec: orig reconst
    xSetCUAlfCtrlFlags(cs, orgUnitBuf, recExtBuf, m_tempPelBuf, uiTmpDist, uiDepth, m_pcTempAlfParam);
    xCalcRDCost(uiTmpDist, m_pcTempAlfParam, uiTmpRate, dTmpCost);
    if (dTmpCost < rdMinCost)
    {
      bChanged = true;
      uiBestDepth = uiDepth;
      rdMinCost = dTmpCost;
      ruiMinDist = uiTmpDist;
      ruiMinRate = uiTmpRate;
      m_bestPelBuf.get(COMPONENT_Y).copyFrom(m_tempPelBuf.get(COMPONENT_Y));
      m_pcBestAlfParam->temporalPredFlag = false;
      copyALFParam(m_pcBestAlfParam, m_pcTempAlfParam);
      m_pcBestAlfParam->temporalPredFlag = true;
    }
  }

  if (bChanged)
  {
    m_pcBestAlfParam->alf_flag = true;
    m_pcBestAlfParam->prevIdx = uiIndex;
    m_pcBestAlfParam->cu_control_flag = true;
    m_pcBestAlfParam->alf_max_depth = uiBestDepth;
  }

  if (!m_pcBestAlfParam->temporalPredFlag)
  {
    return false;
  }

  m_pcBestAlfParam->chroma_idc = 0;

  if (pAlfParam->chroma_idc != 0)
  {
    memcpy(pAlfParam->coeff_chroma, pAlfParam->alfCoeffChroma, sizeof(Int)*m_ALF_MAX_NUM_COEF_C);
    initVarForChroma(pAlfParam, true);
    xFrameChromaGalf(pAlfParam, recExtBuf, recUnitBuf, COMPONENT_Cb);
    xFrameChromaGalf(pAlfParam, recExtBuf, recUnitBuf, COMPONENT_Cr);

    UInt64 uiDistOrg;

    uiDist    = xCalcSSD(orgUnitBuf, recUnitBuf, COMPONENT_Cb);
    uiDistOrg = xCalcSSD(orgUnitBuf, recExtBuf, COMPONENT_Cb);

    if (uiDist < uiDistOrg)
    {
      m_pcBestAlfParam->chroma_idc |= 2;
      m_bestPelBuf.get(COMPONENT_Cb).copyFrom(recUnitBuf.get(COMPONENT_Cb));
    }
    else
    {
      m_bestPelBuf.get(COMPONENT_Cb).copyFrom(recExtBuf.get(COMPONENT_Cb));
    }
    uiDist    = xCalcSSD(orgUnitBuf, recUnitBuf,COMPONENT_Cr);
    uiDistOrg = xCalcSSD(orgUnitBuf, recExtBuf, COMPONENT_Cr);

    if (uiDist < uiDistOrg)
    {
      m_pcBestAlfParam->chroma_idc |= 1;
      m_bestPelBuf.get(COMPONENT_Cr).copyFrom(recUnitBuf.get(COMPONENT_Cr));
    }
    else
    {
      m_bestPelBuf.get(COMPONENT_Cr).copyFrom(recExtBuf.get(COMPONENT_Cr));
    }
  }
  else
  {
    m_bestPelBuf.get(COMPONENT_Cb).copyFrom(recExtBuf.get(COMPONENT_Cb));
    m_bestPelBuf.get(COMPONENT_Cr).copyFrom(recExtBuf.get(COMPONENT_Cr));
  }

  return true;
}
#endif

Void EncAdaptiveLoopFilter::xDeriveGlobalEyFromLgrTapFilter(Double **E0, Double *y0, Double **E1, Double *y1, const Int *pattern0, const Int *pattern1)
{
  Int i, j, l, k = 0;
  for (i = 0; i < m_MAX_SQR_FILT_LENGTH; i++)
  {
    if (pattern0[i] > 0)
    {
      if (pattern1[i] > 0)
      {
        l = 0;
        y1[pattern1[i] - 1] = y0[k];
        for (j = 0; j < m_MAX_SQR_FILT_LENGTH; j++)
        {
          if (pattern0[j] > 0)
          {
            if (pattern1[j] > 0)
            {
              E1[pattern1[i] - 1][pattern1[j] - 1] = E0[k][l];
            }
            l++;
          }
        }
      }
      k++;
    }
  }
}
Void EncAdaptiveLoopFilter::xDeriveLocalEyFromLgrTapFilter(Double *y0, Double *y1, const Int *pattern0, const Int *pattern1)
{
  Int i, k = 0;
  for (i = 0; i < m_MAX_SQR_FILT_LENGTH; i++)
  {
    if (pattern0[i] > 0)
    {
      if (pattern1[i] > 0)
      {
        y1[pattern1[i] - 1] = y0[k];
      }
      k++;
    }
  }
}

