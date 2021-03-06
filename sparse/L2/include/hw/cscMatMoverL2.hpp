/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file cscMatMoverL2.hpp
 * @brief SPARSE Level 2 template function implementation for loading NNNz and row indices.
 *
 * This file is part of Vitis SPARSE Library.
 */

#ifndef XF_SPARSE_CSCMATMOVERL2_HPP
#define XF_SPARSE_CSCMATMOVERL2_HPP

#ifndef __cplusplus
#error "SPARSE Library only works with C++."
#endif

#include <cstdint>
#include "ap_int.h"
#include "hls_stream.h"
#ifndef __SYNTHESIS__
#include <iostream>
#endif

namespace xf {
namespace sparse {
using namespace xf::blas;

template <unsigned int t_ParEntries, unsigned int t_MemBits, unsigned int t_DataBits, unsigned int t_IndexBits>
void loadNnzIdx(const ap_uint<t_MemBits>* p_aNnzIdx,
                const unsigned int p_memBlocks,
                hls::stream<ap_uint<t_DataBits * t_ParEntries> >& p_nnzStr,
                hls::stream<ap_uint<t_IndexBits * t_ParEntries> >& p_idxStr) {
#ifndef __SYNTHESIS__
    assert(t_MemBits / 2 == t_DataBits * t_ParEntries);
    assert(t_MemBits / 2 == t_IndexBits * t_ParEntries);
#endif

    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE
        ap_uint<t_MemBits> l_memVal = p_aNnzIdx[i];
        ap_uint<t_DataBits* t_ParEntries> l_datVal = l_memVal.range(t_MemBits - 1, t_MemBits / 2);
        ap_uint<t_IndexBits* t_ParEntries> l_idxVal = l_memVal.range(t_MemBits / 2 - 1, 0);
        p_nnzStr.write(l_datVal);
        p_idxStr.write(l_idxVal);
    }
}

template <unsigned int t_MemBits>
void loadMemBlocks(const ap_uint<t_MemBits>* p_memPtr,
                   const unsigned int p_memBlocks,
                   hls::stream<ap_uint<t_MemBits> >& p_memStr) {
    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE
        ap_uint<t_MemBits> l_memVal = p_memPtr[i];
        p_memStr.write(l_memVal);
    }
}

template <unsigned int t_MemBits>
void loadColValPtrBlocks(const ap_uint<t_MemBits>* p_memColVal,
                         const ap_uint<t_MemBits>* p_memColPtr,
                         const unsigned int p_memBlocks,
                         hls::stream<ap_uint<t_MemBits> >& p_memStr) {
    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE
        ap_uint<t_MemBits> l_colVal = p_memColVal[i];
        p_memStr.write(l_colVal);
    }

    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE
        ap_uint<t_MemBits> l_colPtr = p_memColPtr[i];
        p_memStr.write(l_colPtr);
    }
}

template <unsigned int t_MaxColMemBlocks, unsigned int t_MemBits>
void bufferTransCols(unsigned int p_memBlocks,
                     unsigned int p_numTrans,
                     hls::stream<ap_uint<t_MemBits> >& p_inColStr,
                     hls::stream<ap_uint<t_MemBits> >& p_combColStr) {
    ap_uint<t_MemBits> l_colValBuf[t_MaxColMemBlocks];
    ap_uint<t_MemBits> l_colPtrBuf[t_MaxColMemBlocks];
    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE
        l_colValBuf[i] = p_inColStr.read();
    }

    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE
        l_colPtrBuf[i] = p_inColStr.read();
    }

    for (unsigned int i = 0; i < p_numTrans; ++i) {
#pragma HLS PIPELINE II = 2
        ap_uint<t_MemBits> l_colVal = l_colValBuf[i];
        ap_uint<t_MemBits> l_colPtr = l_colPtrBuf[i];
        ap_uint<t_MemBits> l_valOut;
        l_valOut.range(t_MemBits / 2 - 1, 0) = l_colVal.range(t_MemBits / 2 - 1, 0);
        l_valOut.range(t_MemBits - 1, t_MemBits / 2) = l_colPtr.range(t_MemBits / 2 - 1, 0);
        p_combColStr.write(l_valOut);

        l_valOut.range(t_MemBits / 2 - 1, 0) = l_colVal.range(t_MemBits - 1, t_MemBits / 2);
        l_valOut.range(t_MemBits - 1, t_MemBits / 2) = l_colPtr.range(t_MemBits - 1, t_MemBits / 2);
        p_combColStr.write(l_valOut);
    }
}

template <unsigned int t_ParEntries, unsigned int t_MemBits, unsigned int t_DataBits>
void memStr2DatStr(hls::stream<ap_uint<t_MemBits> >& p_memStr,
                   const unsigned int p_memBlocks,
                   hls::stream<ap_uint<t_DataBits * t_ParEntries> >& p_datStr) {
#ifndef __SYNTHESIS__
    assert(t_MemBits > (t_DataBits * t_ParEntries));
    assert(t_MemBits % (t_DataBits * t_ParEntries) == 0);
#endif

    const unsigned int t_DataWords = t_MemBits / (t_DataBits * t_ParEntries);
    const unsigned int t_DataWordBits = t_DataBits * t_ParEntries;

    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE II = t_DataWords
        ap_uint<t_MemBits> l_memVal = p_memStr.read();
        for (unsigned int j = 0; j < t_DataWords; ++j) {
            ap_uint<t_DataWordBits> l_datVal = l_memVal.range((j + 1) * t_DataWordBits - 1, j * t_DataWordBits);
            p_datStr.write(l_datVal);
        }
    }
}

template <unsigned int t_ParEntries, unsigned int t_MemBits, unsigned int t_DataBits>
void datStr2MemStr(hls::stream<ap_uint<t_DataBits * t_ParEntries> >& p_datStr,
                   const unsigned int p_memBlocks,
                   hls::stream<ap_uint<t_MemBits> >& p_memStr) {
#ifndef __SYNTHESIS__
    assert(t_MemBits > (t_DataBits * t_ParEntries));
    assert(t_MemBits % (t_DataBits * t_ParEntries) == 0);
#endif

    const unsigned int t_DataWords = t_MemBits / (t_DataBits * t_ParEntries);
    const unsigned int t_DataWordBits = t_DataBits * t_ParEntries;

    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE II = t_DataWords
        ap_uint<t_MemBits> l_memVal;
        for (unsigned int j = 0; j < t_DataWords; ++j) {
            ap_uint<t_DataWordBits> l_datVal = p_datStr.read();
            l_memVal.range((j + 1) * t_DataWordBits - 1, j * t_DataWordBits) = l_datVal;
        }
        p_memStr.write(l_memVal);
    }
}

template <unsigned int t_MemBits>
void storeMemBlocks(hls::stream<ap_uint<t_MemBits> >& p_memStr,
                    const unsigned int p_memBlocks,
                    ap_uint<t_MemBits>* p_memPtr) {
    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE
        ap_uint<t_MemBits> l_memVal = p_memStr.read();
        p_memPtr[i] = l_memVal;
    }
}

template <unsigned int t_MemBits,
          unsigned int t_ParEntries,
          unsigned int t_DataBits,
          unsigned int t_IndexBits,
          typename t_DataPktType,
          typename t_IndexPktType>
void memStr2ColPtrValStr(hls::stream<ap_uint<t_MemBits> >& p_memStr,
                         const unsigned int p_memBlocks,
                         hls::stream<t_DataPktType>& p_datPktStr,
                         hls::stream<t_IndexPktType>& p_idxPktStr) {
    const unsigned int t_HalfMemBits = t_MemBits / 2;
    const unsigned int t_DataBusBits = t_DataBits * t_ParEntries;
    const unsigned int t_IndexBusBits = t_IndexBits * t_ParEntries;
    const unsigned int t_DataWords = t_HalfMemBits / t_DataBusBits;
    const unsigned int t_IndexWords = t_HalfMemBits / t_IndexBusBits;

#ifndef __SYNTHESIS__
    assert(t_MemBits % 2 == 0);
    assert(t_DataBusBits == t_IndexBusBits);
    assert((t_MemBits / 2) % t_DataBusBits == 0);
#endif

    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE II = t_DataWords
        ap_uint<t_MemBits> l_memVal = p_memStr.read();
        for (unsigned int j = 0; j < t_DataWords; ++j) {
            ap_uint<t_DataBusBits> l_datVal = l_memVal.range((j + 1) * t_DataBusBits - 1, j * t_DataBusBits);
            ap_uint<t_IndexBusBits> l_idxVal =
                l_memVal.range((j + 1) * t_DataBusBits + t_HalfMemBits - 1, j * t_DataBusBits + t_HalfMemBits);
            t_DataPktType l_datPkt;
            t_IndexPktType l_idxPkt;
            l_datPkt.data = l_datVal;
            l_idxPkt.data = l_idxVal;
            p_datPktStr.write(l_datPkt);
            p_idxPktStr.write(l_idxPkt);
        }
    }
}

template <typename t_MemPktType, unsigned int t_ParEntries, unsigned int t_MemBits, unsigned int t_DataBits>
void memPktStr2DatStr(hls::stream<t_MemPktType>& p_memPktStr,
                      const unsigned int p_memBlocks,
                      hls::stream<ap_uint<t_DataBits * t_ParEntries> >& p_datStr) {
#ifndef __SYNTHESIS__
    assert(t_MemBits > (t_DataBits * t_ParEntries));
    assert(t_MemBits % (t_DataBits * t_ParEntries) == 0);
#endif

    const unsigned int t_DataWords = t_MemBits / (t_DataBits * t_ParEntries);
    const unsigned int t_DataWordBits = t_DataBits * t_ParEntries;

    for (unsigned int i = 0; i < p_memBlocks; ++i) {
#pragma HLS PIPELINE II = t_DataWords
        t_MemPktType l_memPkt = p_memPktStr.read();
        ap_uint<t_MemBits> l_memVal = l_memPkt.data;
        for (unsigned int j = 0; j < t_DataWords; ++j) {
            ap_uint<t_DataWordBits> l_datVal = l_memVal.range((j + 1) * t_DataWordBits - 1, j * t_DataWordBits);
            p_datStr.write(l_datVal);
        }
    }
}

template <typename t_ParamPktType,
          typename t_MemPktType,
          unsigned int t_HbmChannels,
          unsigned int t_ParEntries,
          unsigned int t_MemBits,
          unsigned int t_DataBits>
void readNnzColPkt(hls::stream<t_ParamPktType>& p_nnzColParamPktStr,
                   hls::stream<t_MemPktType>& p_nnzColPktStr,
                   hls::stream<ap_uint<32> >& p_paramStr,
                   hls::stream<ap_uint<t_DataBits * t_ParEntries> >& p_datStr) {
    const unsigned int t_ParBlocks = t_MemBits / (t_DataBits * t_ParEntries);
    ap_uint<32> l_chBlocks[t_HbmChannels];

    unsigned int l_memBlocks = 0;
    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
        t_ParamPktType l_paramPkt;
#pragma HLS PIPELINE
        l_paramPkt = p_nnzColParamPktStr.read();
        l_chBlocks[i] = l_paramPkt.data;
        l_memBlocks += l_paramPkt.data;
        p_paramStr.write(l_chBlocks[i] * t_ParBlocks);
    }

    memPktStr2DatStr<t_MemPktType, t_ParEntries, t_MemBits, t_DataBits>(p_nnzColPktStr, l_memBlocks, p_datStr);
}

template <typename t_ParamPktType,
          typename t_MemPktType,
          unsigned int t_HbmChannels,
          unsigned int t_ParEntries,
          unsigned int t_MemBits,
          unsigned int t_DataBits>
void readColVecPkt(hls::stream<t_ParamPktType>& p_colVecParamPktStr,
                   hls::stream<t_MemPktType>& p_colVecPktStr,
                   hls::stream<ap_uint<32> >& p_paramStr,
                   hls::stream<ap_uint<t_DataBits * t_ParEntries> >& p_datStr) {
    const unsigned int t_ParBlocks = t_MemBits / (t_DataBits * t_ParEntries);
    ap_uint<32> l_vecBlocks;
    ap_uint<32> l_parVecBlocks;
    ap_uint<32> l_chBlocks[t_HbmChannels];
    ap_uint<32> l_colMinIdx[t_HbmChannels];
    ap_uint<32> l_colMaxIdx[t_HbmChannels];

    t_ParamPktType l_paramPkt;
    l_paramPkt = p_colVecParamPktStr.read();
    l_vecBlocks = l_paramPkt.data;
    l_parVecBlocks = l_vecBlocks * t_ParBlocks;
    p_paramStr.write(l_parVecBlocks);
    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
#pragma HLS PIPELINE II = 3
        l_paramPkt = p_colVecParamPktStr.read();
        l_chBlocks[i] = l_paramPkt.data;
        p_paramStr.write(l_chBlocks[i] * t_ParBlocks);
        l_paramPkt = p_colVecParamPktStr.read();
        l_colMinIdx[i] = l_paramPkt.data;
        p_paramStr.write(l_colMinIdx[i]);
        l_paramPkt = p_colVecParamPktStr.read();
        l_colMaxIdx[i] = l_paramPkt.data;
        p_paramStr.write(l_colMaxIdx[i]);
    }

    memPktStr2DatStr<t_MemPktType, t_ParEntries, t_MemBits, t_DataBits>(p_colVecPktStr, l_vecBlocks, p_datStr);
}

template <typename t_DataType,
          unsigned int t_HbmChannels,
          unsigned int t_ParEntries,
          unsigned int t_MemBits,
          unsigned int t_DataBits>
void shiftDatStr2MemStr(hls::stream<ap_uint<32> >& p_paramStr,
                        hls::stream<ap_uint<t_DataBits * t_ParEntries> >& p_datStr,
                        hls::stream<ap_uint<32> >& p_paramOutStr,
                        hls::stream<ap_uint<t_MemBits> >& p_datOutStr) {
#ifndef __SYNTHESIS__
    assert(t_MemBits % (t_DataBits * t_ParEntries) == 0);
#endif
    const static unsigned int t_MemWordWidth = t_MemBits / t_DataBits;
    const static unsigned int t_ParWords = t_MemBits / (t_DataBits * t_ParEntries);
    const static unsigned int t_ParWordBits = t_DataBits * t_ParEntries;

    ap_uint<32> l_datBlocks[t_HbmChannels];
#pragma HLS ARRAY_PARTITION variable = l_datBlocks dim = 1
    ap_uint<32> l_memBlocks[t_HbmChannels];
#pragma HLS ARRAY_PARTITION variable = l_memBlocks dim = 1
    ap_uint<32> l_rowMinIdx[t_HbmChannels];
#pragma HLS ARRAY_PARTITION variable = l_rowMinIdx dim = 1
    ap_uint<32> l_rowMinMemIdx[t_HbmChannels];
#pragma HLS ARRAY_PARTITION variable = l_rowMinMemIdx dim = 1

    ap_uint<8> l_rowMinIdxMod[t_HbmChannels];
#pragma HLS ARRAY_PARTITION variable = l_rowMinIdxMod dim = 1
    ap_uint<8> l_rowMinIdxModPar[t_HbmChannels];
#pragma HLS ARRAY_PARTITION variable = l_rowMinIdxModPar dim = 1
    ap_uint<8> l_datBlocksMod[t_HbmChannels];
#pragma HLS ARRAY_PARTITION variable = l_datBlocksMod dim = 1

    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
#pragma HLS PIPELINE II = 2
        l_datBlocks[i] = p_paramStr.read();
        l_rowMinIdx[i] = p_paramStr.read();
    }
    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
#pragma HLS PIPELINE
        l_rowMinMemIdx[i] = l_rowMinIdx[i] / t_MemWordWidth;
        l_rowMinIdxMod[i] = l_rowMinIdx[i] % t_MemWordWidth;
        l_datBlocksMod[i] = l_datBlocks[i] % t_ParWords;
        l_rowMinIdxModPar[i] = l_rowMinIdxMod[i] % t_ParEntries;
        unsigned int l_totalEntries = l_rowMinIdxMod[i] + l_datBlocks[i] * t_ParEntries;
        unsigned int l_totalBlocks = l_totalEntries / t_MemWordWidth;
        l_memBlocks[i] =
            (l_totalEntries % t_MemWordWidth == 0) ? (ap_uint<32>)(l_totalBlocks) : (ap_uint<32>)(l_totalBlocks + 1);
    }
    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
#pragma HLS PIPELINE II = 2
        p_paramOutStr.write(l_memBlocks[i]);
        p_paramOutStr.write(l_rowMinMemIdx[i]);
    }

    // shift and pack data words into memory words
    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
        ap_uint<8> l_memWordIdx = l_rowMinIdxMod[i] / t_ParEntries;
        ap_uint<8> l_datWordIdx = l_rowMinIdxModPar[i];
        ap_uint<t_MemBits> l_zeroBits(0);
        WideType<ap_uint<t_DataBits * t_ParEntries>, t_ParWords> l_curMemWordVal(l_zeroBits);
        ap_uint<t_ParWordBits> l_curDatValBits(0);
        unsigned int l_blocksMem = l_memBlocks[i];
        unsigned int l_blocksDat = l_datBlocks[i];
        WideType<t_DataType, t_ParEntries> l_curDatVal(l_curDatValBits);
#pragma HLS ARRAY_PARTITION variable = l_curDatVal
        while (l_blocksDat > 0) {
#pragma HLS PIPELINE
            ap_uint<t_ParWordBits> l_nextDatValBits = p_datStr.read();
            WideType<t_DataType, t_ParEntries> l_nextDatVal(l_nextDatValBits);
            WideType<t_DataType, t_ParEntries> l_outDatVal;
#pragma HLS ARRAY_PARTITION variable = l_nextDatVal
#pragma HLS ARRAY_PARTITION variable = l_outDatVal
            for (unsigned int j = 0; j < t_ParEntries; ++j) {
#pragma HLS UNROLL
                const uint8_t l_curValIdx = t_ParEntries - l_datWordIdx;
                l_outDatVal[j] = (j < l_datWordIdx) ? l_curDatVal[j + l_curValIdx] : l_nextDatVal[j - l_datWordIdx];
            }
            l_curDatVal = l_nextDatVal;
            l_blocksDat--;
            ap_uint<t_ParWordBits> l_outDatValBits = l_outDatVal;
            l_curMemWordVal.unshift(l_outDatValBits);
            l_memWordIdx = (l_memWordIdx + 1) % t_ParWords;
            if (l_memWordIdx == 0) {
                p_datOutStr.write(l_curMemWordVal);
                l_blocksMem--;
            }
        }
        while (l_memWordIdx != 0) {
#pragma HLS PIPELINE
            ap_uint<t_ParWordBits> l_nextDatValBits = 0;
            WideType<t_DataType, t_ParEntries> l_nextDatVal(l_nextDatValBits);
            WideType<t_DataType, t_ParEntries> l_outDatVal;
#pragma HLS ARRAY_PARTITION variable = l_nextDatVal
#pragma HLS ARRAY_PARTITION variable = l_outDatVal
            for (unsigned int j = 0; j < t_ParEntries; ++j) {
#pragma HLS UNROLL
                const uint8_t l_curValIdx = t_ParEntries - l_datWordIdx;
                l_outDatVal[j] = (j < l_datWordIdx) ? l_curDatVal[j + l_curValIdx] : l_nextDatVal[j - l_datWordIdx];
            }
            l_curDatVal = l_nextDatVal;
            l_curMemWordVal.unshift(l_outDatVal);
            l_memWordIdx = (l_memWordIdx + 1) % t_ParWords;
            if (l_memWordIdx == 0) {
                p_datOutStr.write(l_curMemWordVal);
                l_blocksMem--;
            }
        }
    }
}

template <typename t_DataType,
          unsigned int t_MaxRowBlocks,
          unsigned int t_HbmChannels,
          unsigned int t_ParEntries,
          unsigned int t_MemBits,
          unsigned int t_DataBits>
void writeMem(hls::stream<ap_uint<32> >& p_paramStr,
              hls::stream<ap_uint<t_MemBits> >& p_datStr,
              ap_uint<t_MemBits>* p_memPtr) {
    const unsigned int t_MemWordWidth = t_MemBits / t_DataBits;
#ifndef __SYNTHESIS__
    assert(t_MemBits % t_DataBits == 0);
    assert(t_MaxRowBlocks % t_MemWordWidth == 0);
#endif
    const unsigned int t_MaxRowMemBlocks = t_MaxRowBlocks / t_MemWordWidth;

    ap_uint<t_MemBits> l_rowStore[t_MaxRowMemBlocks * t_HbmChannels];
    ap_uint<32> l_memBlocks[t_HbmChannels];
    ap_uint<32> l_rowMinBlockIdx[t_HbmChannels];

    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
#pragma HLS PIPELINE II = 2
        l_memBlocks[i] = p_paramStr.read();
        l_rowMinBlockIdx[i] = p_paramStr.read();
    }

    // read mem
    unsigned int l_base = 0;
    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
        unsigned int l_blocks = l_memBlocks[i];
        unsigned int l_offset = l_rowMinBlockIdx[i];
        for (unsigned int j = 0; j < l_blocks; ++j) {
#pragma HLS PIPELINE
            l_rowStore[l_base + j] = p_memPtr[l_offset + j];
        }
        l_base += l_blocks;
    }

    // write mem
    l_base = 0;
    for (unsigned int i = 0; i < t_HbmChannels; ++i) {
        unsigned int l_blocks = l_memBlocks[i];
        unsigned int l_offset = l_rowMinBlockIdx[i];
        for (unsigned int j = 0; j < l_blocks; ++j) {
#pragma HLS PIPELINE
            ap_uint<t_MemBits> l_inValBits = p_datStr.read();
            ap_uint<t_MemBits> l_storeValBits = l_rowStore[l_base + j];
            WideType<t_DataType, t_MemWordWidth> l_inVal(l_inValBits);
            WideType<t_DataType, t_MemWordWidth> l_storeVal(l_storeValBits);
            WideType<t_DataType, t_MemWordWidth> l_memVal;
            for (unsigned int k = 0; k < t_MemWordWidth; ++k) {
#pragma HLS UNROLL
                l_memVal[k] = l_inVal[k] + l_storeVal[k];
            }
            ap_uint<t_MemBits> l_memValBits = l_memVal;
            p_memPtr[l_offset + j] = l_memValBits;
        }
        l_base += l_blocks;
    }
}

} // end namespace sparse
} // end namespace xf
#endif
