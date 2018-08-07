/*
 YCoCgDXT.c
 Hap Codec

 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved. 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


    Based on code by J.M.P. van Waveren / id Software, Inc.
    and changes by Chris Sidhall / Electronic Arts

    My changes are trivial:
      - Remove dependencies on other EAWebKit files
      - Mark unexported functions as static
      - Refactor to eliminate use of a global variable
      - Correct spelling of NVIDIA_7X_HARDWARE_BUG_FIX macro
      - Remove single usage of an assert macro


 Copyright (C) 2009-2011 Electronic Arts, Inc.  All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 
 1.  Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 2.  Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
 its contributors may be used to endorse or promote products derived
 from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Valentin Schmidt 2018: decoding code removed for HapEncoder

///////////////////////////////////////////////////////////////////////////////
// BCImageCompressionEA.cpp
// File created by Chrs Sidhall
// Also please see Copyright (C) 2007 Id Software, Inc used in this file.
///////////////////////////////////////////////////////////////////////////////

#include "YCoCgDXT.h"
#include <string.h>
#include <stdlib.h>

/* ALWAYS_INLINE */
/* Derived from EAWebKit's AlwaysInline.h, losing some of its support for other compilers */

#ifndef ALWAYS_INLINE
#if (defined(__GNUC__) || defined(__clang__)) && !defined(DEBUG)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(_MSC_VER) && defined(NDEBUG)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif
#endif

// CSidhall Note: The compression code is directly from http://developer.nvidia.com/object/real-time-ycocg-dxt-compression.html
// It was missing some Emit functions but have tried to keep it as close as possible to the orignal version.
// Also removed some alpha handling which was never used and added a few overloaded functions (like ExtractBlock).


/*
 Real-Time YCoCg DXT Compression
 Copyright (C) 2007 Id Software, Inc.
 Written by J.M.P. van Waveren
 
 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 */

/*
 * This code was modified by Electronic Arts Inc Copyright � 2009
 */

#ifndef word
typedef unsigned short  word;
#endif 

#ifndef dword
typedef unsigned int    dword;
#endif

#define INSET_COLOR_SHIFT       4       // inset color bounding box
#define INSET_ALPHA_SHIFT       5       // inset alpha bounding box

#define C565_5_MASK             0xF8    // 0xFF minus last three bits
#define C565_6_MASK             0xFC    // 0xFF minus last two bits

#define NVIDIA_G7X_HARDWARE_BUG_FIX     // keep the colors sorted as: max, min

#if defined(__LITTLE_ENDIAN__) || defined(_WIN32)
#define EA_SYSTEM_LITTLE_ENDIAN
#endif

static ALWAYS_INLINE word ColorTo565( const byte *color ) {
    return ( ( color[ 0 ] >> 3 ) << 11 ) | ( ( color[ 1 ] >> 2 ) << 5 ) | ( color[ 2 ] >> 3 );
}

static ALWAYS_INLINE void EmitByte( byte b, byte **outData ) {
    (*outData)[0] = b;
    *outData += 1;
}

static ALWAYS_INLINE void EmitUInt( unsigned int s, byte **outData ){
    (*outData)[0] = ( s >>  0 ) & 255;
    (*outData)[1] = ( s >>  8 ) & 255;
    (*outData)[2] = ( s >>  16 ) & 255;
    (*outData)[3] = ( s >>  24 ) & 255;
    *outData += 4;
}

static ALWAYS_INLINE void EmitUShort( unsigned short s, byte **outData ){
    (*outData)[0] = ( s >>  0 ) & 255;
    (*outData)[1] = ( s >>  8 ) & 255;
    *outData += 2;
}

static ALWAYS_INLINE void EmitWord( word s, byte **outData ) {
    (*outData)[0] = ( s >>  0 ) & 255;
    (*outData)[1] = ( s >>  8 ) & 255;
    *outData += 2;
}

static ALWAYS_INLINE void EmitDoubleWord( dword i, byte **outData ) {
    (*outData)[0] = ( i >>  0 ) & 255;
    (*outData)[1] = ( i >>  8 ) & 255;
    (*outData)[2] = ( i >> 16 ) & 255;
    (*outData)[3] = ( i >> 24 ) & 255;
    (*outData) += 4;
}

static ALWAYS_INLINE void ExtractBlock( const byte *inPtr, const int stride, byte *colorBlock ) {
    for ( int j = 0; j < 4; j++ ) {
        memcpy( &colorBlock[j*4*4], inPtr, 4*4 );
        inPtr += stride;
    }
}

// This box extract replicates the last rows and columns if the row or columns are not 4 texels aligned
// This is so we don't get random pixels which could affect the color interpolation
static void ExtractBlock( const byte *inPtr, const int stride, const int widthRemain, const int heightRemain, byte *colorBlock ) {
    int *pBlock32 = (int *) colorBlock;  // Since we are using ARGA, we assume 4 byte alignment is already being used
    int *pSource32 = (int*) inPtr; 
    
    int hIndex=0;
    for(int j =0; j < 4; j++) {
        int wIndex = 0;
        for(int i=0; i < 4; i++) {
            pBlock32[i] = pSource32[wIndex];    
            // Set up offset for next column source (keep existing if we are at the end)         
            if(wIndex < (widthRemain - 1)) {
                wIndex++;
            }
        }
        
        // Set up offset for next texel row source (keep existing if we are at the end)
        pBlock32 +=4;    
        hIndex++;
        if(hIndex < (heightRemain-1)) {
            pSource32 +=(stride >> 2);
        }
    }
}

static void GetMinMaxYCoCg( byte *colorBlock, byte *minColor, byte *maxColor ) {
    minColor[0] = minColor[1] = minColor[2] = minColor[3] = 255;
    maxColor[0] = maxColor[1] = maxColor[2] = maxColor[3] = 0;
    
    for ( int i = 0; i < 16; i++ ) {
        if ( colorBlock[i*4+0] < minColor[0] ) {
            minColor[0] = colorBlock[i*4+0];
        }
        if ( colorBlock[i*4+1] < minColor[1] ) {
            minColor[1] = colorBlock[i*4+1];
        }
        // Note: the alpha is not used so no point in checking for it
        //        if ( colorBlock[i*4+2] < minColor[2] ) {
        //            minColor[2] = colorBlock[i*4+2];
        //        }
        if ( colorBlock[i*4+3] < minColor[3] ) {
            minColor[3] = colorBlock[i*4+3];
        }
        if ( colorBlock[i*4+0] > maxColor[0] ) {
            maxColor[0] = colorBlock[i*4+0];
        }
        if ( colorBlock[i*4+1] > maxColor[1] ) {
            maxColor[1] = colorBlock[i*4+1];
        }
        // Note: the alpha is not used so no point in checking for it
        //        if ( colorBlock[i*4+2] > maxColor[2] ) {
        //            maxColor[2] = colorBlock[i*4+2];
        //        }
        if ( colorBlock[i*4+3] > maxColor[3] ) {
            maxColor[3] = colorBlock[i*4+3];
        }
    }
}

// EA/Alex Mole: abs isn't inlined and gets called a *lot* in this code :)
// Let's make us an inlined one!
static ALWAYS_INLINE int absEA( int liArg )
{
    return ( liArg >= 0 ) ? liArg : -liArg;
}

static void ScaleYCoCg( byte *colorBlock, byte *minColor, byte *maxColor ) {
    int m0 = absEA( minColor[0] - 128 );      // (the 128 is to center to color to grey (128,128) )
    int m1 = absEA( minColor[1] - 128 );
    int m2 = absEA( maxColor[0] - 128 );
    int m3 = absEA( maxColor[1] - 128 );
    
    if ( m1 > m0 ) m0 = m1;
    if ( m3 > m2 ) m2 = m3;
    if ( m2 > m0 ) m0 = m2;
    
    const int s0 = 128 / 2 - 1;
    const int s1 = 128 / 4 - 1;
    
    int mask0 = -( m0 <= s0 );
    int mask1 = -( m0 <= s1 );
    int scale = 1 + ( 1 & mask0 ) + ( 2 & mask1 );
    
    minColor[0] = ( minColor[0] - 128 ) * scale + 128;
    minColor[1] = ( minColor[1] - 128 ) * scale + 128;
    minColor[2] = ( scale - 1 ) << 3;
    
    maxColor[0] = ( maxColor[0] - 128 ) * scale + 128;
    maxColor[1] = ( maxColor[1] - 128 ) * scale + 128;
    maxColor[2] = ( scale - 1 ) << 3;
    
    for ( int i = 0; i < 16; i++ ) {
        colorBlock[i*4+0] = ( colorBlock[i*4+0] - 128 ) * scale + 128;
        colorBlock[i*4+1] = ( colorBlock[i*4+1] - 128 ) * scale + 128;
    }
}

static void InsetYCoCgBBox( byte *minColor, byte *maxColor ) {
    int inset[4];
    int mini[4];
    int maxi[4];
    
    inset[0] = ( maxColor[0] - minColor[0] ) - ((1<<(INSET_COLOR_SHIFT-1))-1);
    inset[1] = ( maxColor[1] - minColor[1] ) - ((1<<(INSET_COLOR_SHIFT-1))-1);
    inset[3] = ( maxColor[3] - minColor[3] ) - ((1<<(INSET_ALPHA_SHIFT-1))-1);
    
    mini[0] = ( ( minColor[0] << INSET_COLOR_SHIFT ) + inset[0] ) >> INSET_COLOR_SHIFT;
    mini[1] = ( ( minColor[1] << INSET_COLOR_SHIFT ) + inset[1] ) >> INSET_COLOR_SHIFT;
    mini[3] = ( ( minColor[3] << INSET_ALPHA_SHIFT ) + inset[3] ) >> INSET_ALPHA_SHIFT;
    
    maxi[0] = ( ( maxColor[0] << INSET_COLOR_SHIFT ) - inset[0] ) >> INSET_COLOR_SHIFT;
    maxi[1] = ( ( maxColor[1] << INSET_COLOR_SHIFT ) - inset[1] ) >> INSET_COLOR_SHIFT;
    maxi[3] = ( ( maxColor[3] << INSET_ALPHA_SHIFT ) - inset[3] ) >> INSET_ALPHA_SHIFT;
    
    mini[0] = ( mini[0] >= 0 ) ? mini[0] : 0;
    mini[1] = ( mini[1] >= 0 ) ? mini[1] : 0;
    mini[3] = ( mini[3] >= 0 ) ? mini[3] : 0;
    
    maxi[0] = ( maxi[0] <= 255 ) ? maxi[0] : 255;
    maxi[1] = ( maxi[1] <= 255 ) ? maxi[1] : 255;
    maxi[3] = ( maxi[3] <= 255 ) ? maxi[3] : 255;
    
    minColor[0] = ( mini[0] & C565_5_MASK ) | ( mini[0] >> 5 );
    minColor[1] = ( mini[1] & C565_6_MASK ) | ( mini[1] >> 6 );
    minColor[3] = mini[3];
    
    maxColor[0] = ( maxi[0] & C565_5_MASK ) | ( maxi[0] >> 5 );
    maxColor[1] = ( maxi[1] & C565_6_MASK ) | ( maxi[1] >> 6 );
    maxColor[3] = maxi[3];
}

static void SelectYCoCgDiagonal( const byte *colorBlock, byte *minColor, byte *maxColor ) {
    byte mid0 = ( (int) minColor[0] + maxColor[0] + 1 ) >> 1;
    byte mid1 = ( (int) minColor[1] + maxColor[1] + 1 ) >> 1;
    
    byte side = 0;
    for ( int i = 0; i < 16; i++ ) {
        byte b0 = colorBlock[i*4+0] >= mid0;
        byte b1 = colorBlock[i*4+1] >= mid1;
        side += ( b0 ^ b1 );
    }
    
    byte mask = -( side > 8 );
    
#ifdef NVIDIA_G7X_HARDWARE_BUG_FIX
    mask &= -( minColor[0] != maxColor[0] );
#endif
    
    byte c0 = minColor[1];
    byte c1 = maxColor[1];
    
    // PlayStation 3 compiler warning fix:    
    // c0 ^= c1 ^= mask &= c0 ^= c1;    // Orignial code
    byte c2 = c0 ^ c1;
    c0 = c2;
    c0 ^= c1 ^= mask &=c2;
    
    minColor[1] = c0;
    maxColor[1] = c1;
}

static void EmitAlphaIndices( const byte *colorBlock, const byte minAlpha, const byte maxAlpha, byte **outData ) {
        
    const int ALPHA_RANGE = 7;
    
    byte mid, ab1, ab2, ab3, ab4, ab5, ab6, ab7;
    byte indexes[16];
    
    mid = ( maxAlpha - minAlpha ) / ( 2 * ALPHA_RANGE );
    
    ab1 = minAlpha + mid;
    ab2 = ( 6 * maxAlpha + 1 * minAlpha ) / ALPHA_RANGE + mid;
    ab3 = ( 5 * maxAlpha + 2 * minAlpha ) / ALPHA_RANGE + mid;
    ab4 = ( 4 * maxAlpha + 3 * minAlpha ) / ALPHA_RANGE + mid;
    ab5 = ( 3 * maxAlpha + 4 * minAlpha ) / ALPHA_RANGE + mid;
    ab6 = ( 2 * maxAlpha + 5 * minAlpha ) / ALPHA_RANGE + mid;
    ab7 = ( 1 * maxAlpha + 6 * minAlpha ) / ALPHA_RANGE + mid;
    
    for ( int i = 0; i < 16; i++ ) {
        byte a = colorBlock[i*4+3]; // Here it seems to be using the Y (luna) for the alpha
        int b1 = ( a <= ab1 );
        int b2 = ( a <= ab2 );
        int b3 = ( a <= ab3 );
        int b4 = ( a <= ab4 );
        int b5 = ( a <= ab5 );
        int b6 = ( a <= ab6 );
        int b7 = ( a <= ab7 );
        int index = ( b1 + b2 + b3 + b4 + b5 + b6 + b7 + 1 ) & 7;
        indexes[i] = index ^ ( 2 > index );
    }
    
    EmitByte( (indexes[ 0] >> 0) | (indexes[ 1] << 3) | (indexes[ 2] << 6),  outData );
    EmitByte( (indexes[ 2] >> 2) | (indexes[ 3] << 1) | (indexes[ 4] << 4) | (indexes[ 5] << 7), outData );
    EmitByte( (indexes[ 5] >> 1) | (indexes[ 6] << 2) | (indexes[ 7] << 5), outData );
    
    EmitByte( (indexes[ 8] >> 0) | (indexes[ 9] << 3) | (indexes[10] << 6), outData );
    EmitByte( (indexes[10] >> 2) | (indexes[11] << 1) | (indexes[12] << 4) | (indexes[13] << 7), outData );
    EmitByte( (indexes[13] >> 1) | (indexes[14] << 2) | (indexes[15] << 5), outData );
}

static void EmitColorIndices( const byte *colorBlock, const byte *minColor, const byte *maxColor, byte **outData ) {
    word colors[4][4];
    unsigned int result = 0;
    
    colors[0][0] = ( maxColor[0] & C565_5_MASK ) | ( maxColor[0] >> 5 );
    colors[0][1] = ( maxColor[1] & C565_6_MASK ) | ( maxColor[1] >> 6 );
    colors[0][2] = ( maxColor[2] & C565_5_MASK ) | ( maxColor[2] >> 5 );
    colors[0][3] = 0;
    colors[1][0] = ( minColor[0] & C565_5_MASK ) | ( minColor[0] >> 5 );
    colors[1][1] = ( minColor[1] & C565_6_MASK ) | ( minColor[1] >> 6 );
    colors[1][2] = ( minColor[2] & C565_5_MASK ) | ( minColor[2] >> 5 );
    colors[1][3] = 0;
    colors[2][0] = ( 2 * colors[0][0] + 1 * colors[1][0] ) / 3;
    colors[2][1] = ( 2 * colors[0][1] + 1 * colors[1][1] ) / 3;
    colors[2][2] = ( 2 * colors[0][2] + 1 * colors[1][2] ) / 3;
    colors[2][3] = 0;
    colors[3][0] = ( 1 * colors[0][0] + 2 * colors[1][0] ) / 3;
    colors[3][1] = ( 1 * colors[0][1] + 2 * colors[1][1] ) / 3;
    colors[3][2] = ( 1 * colors[0][2] + 2 * colors[1][2] ) / 3;
    colors[3][3] = 0;
    
    for ( int i = 15; i >= 0; i-- ) {
        int c0, c1;
        
        c0 = colorBlock[i*4+0];
        c1 = colorBlock[i*4+1];
        
        int d0 = absEA( colors[0][0] - c0 ) + absEA( colors[0][1] - c1 );
        int d1 = absEA( colors[1][0] - c0 ) + absEA( colors[1][1] - c1 );
        int d2 = absEA( colors[2][0] - c0 ) + absEA( colors[2][1] - c1 );
        int d3 = absEA( colors[3][0] - c0 ) + absEA( colors[3][1] - c1 );
        
        bool b0 = d0 > d3;
        bool b1 = d1 > d2;
        bool b2 = d0 > d2;
        bool b3 = d1 > d3;
        bool b4 = d2 > d3;
        
        int x0 = b1 & b2;
        int x1 = b0 & b3;
        int x2 = b0 & b4;
        
        int indexFinal =  ( x2 | ( ( x0 | x1 ) << 1 ) ) << ( i << 1 );
        result |= indexFinal;
    }
    
    EmitUInt( result, outData );
}

/*F*************************************************************************************************/
/*!
 \Function    CompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height, const int stride ) 
 
 \Description        This is the C version of the YcoCgDXT5.  
 
 Input data needs to be converted from ARGB to YCoCg before calling this function.
 
 Does not support alpha at all since it uses the alpha channel to store the Y (luma).
 
 The output size is 4:1 but will be based on rounded up texture sizes on 4 texel boundaries 
 So for example if the source texture is 33 x 32, the compressed size will be 36x32. 
 
 The DXT5 compresses groups of 4x4 texels into 16 bytes (4:1 saving) 
 
 The compressed format:
 2 bytes of min and max Y luma values (these are used to rebuild an 8 element Luma table)
 6 bytes of indexes into the luma table
 3 bits per index so 16 indexes total 
 2 shorts of min and max color values (these are used to rebuild a 4 element chroma table)
 5 bits Co
 6 bits Cg
 5 bits Scale. The scale can only be 1, 2 or 4. 
 4 bytes of indexes into the Chroma CocG table 
 2 bits per index so 16 indexes total
 
 \Input              const byte *inBuf   Input buffer of the YCoCG textel data
 \Input              const byte *outBuf  Output buffer for the compressed data
 \Input              int width           in source width 
 \Input              int height          in source height
 \Input              int stride          in source in buffer stride in bytes
 
 \Output             int ouput size
 
 \Version    1.1     CSidhall 01/12/09 modified to account for non aligned textures
 1.2     1/10/10 Added stride
 */
/*************************************************************************************************F*/
extern "C" int CompressYCoCgDXT5( const byte *inBuf, byte *outBuf, const int width, const int height , const int stride) {
    
    int outputBytes =0;
    
    byte block[64];
    byte minColor[4];
    byte maxColor[4];
    
    byte *outData = outBuf;
    
    int blockLineSize = stride * 4;  // 4 lines per loop
    
    for ( int j = 0; j < height; j += 4, inBuf +=blockLineSize ) {
        int heightRemain = height - j;    
        for ( int i = 0; i < width; i += 4 ) {
            
            // Note: Modified from orignal source so that it can handle the edge blending better with non aligned 4x textures
            int widthRemain = width - i;
            if ((heightRemain < 4) || (widthRemain < 4) ) {
                ExtractBlock( inBuf + i * 4, stride, widthRemain, heightRemain,  block );  
            }
            else {
                ExtractBlock( inBuf + i * 4, stride, block );
            }
            // A simple min max extract for each color channel including alpha             
            GetMinMaxYCoCg( block, minColor, maxColor );
            ScaleYCoCg( block, minColor, maxColor );    // Sets the scale in the min[2] and max[2] offset
            InsetYCoCgBBox( minColor, maxColor );
            SelectYCoCgDiagonal( block, minColor, maxColor );
            
            EmitByte( maxColor[3], &outData );    // Note: the luma is stored in the alpha channel
            EmitByte( minColor[3], &outData );
            
            EmitAlphaIndices( block, minColor[3], maxColor[3], &outData );
            
            EmitUShort( ColorTo565( maxColor ), &outData );
            EmitUShort( ColorTo565( minColor ), &outData );
            
            EmitColorIndices( block, minColor, maxColor, &outData );
        }
    }
    
    outputBytes = (int)(outData - outBuf);
    
    return outputBytes;
}
