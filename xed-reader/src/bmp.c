/* 
 * Copyright (c) 2013, Dan Jackson.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 */

// BMP File
// Dan Jackson, 2013



#ifdef _WIN32
//#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include "xed/bmp.h"


// Endian-independent short/long read/write
static void fputshort(short v, FILE *fp) { fputc((char)((v >> 0) & 0xff), fp); fputc((char)((v >> 8) & 0xff), fp); }
static void fputlong(long v, FILE *fp) { fputc((char)((v >> 0) & 0xff), fp); fputc((char)((v >> 8) & 0xff), fp); fputc((char)((v >> 16) & 0xff), fp); fputc((char)((v >> 24) & 0xff), fp); }


int BitmapWrite(const char *filename, const void *buffer, int bitsPerPixel, int width, int inputStride, int height)
{
    FILE *fp;
    int p;
    int stride;
    int paletteEntries;
    int y;

    if (bitsPerPixel <= 8) { paletteEntries = (1 << bitsPerPixel); } else { paletteEntries = 0; }
    bitsPerPixel = ((bitsPerPixel + 7) / 8) * 8;
    stride = ((((width * bitsPerPixel) + 31) & ~31) >> 3);

    if (filename == NULL || filename[0] == '\0') { return -1; }

    fp = fopen(filename, "wb");
    if (fp == NULL) { return -1; }
    
    fputc('B', fp); fputc('M', fp); // bfType
    
    fputlong((height * stride) + 54 + (paletteEntries * 4), fp);     // bfSize
    fputshort(0, fp);              // bfReserved1
    fputshort(0, fp);              // bfReserved2
    fputlong(54 + (paletteEntries * 4), fp);  // bfOffBits

    fputlong(40, fp);              // biSize
    fputlong(width, fp);           // biWidth
    fputlong(height, fp);          // biHeight
    fputshort(1, fp);              // biPlanes
    fputshort(bitsPerPixel, fp);   // biBitCount
    fputlong(0, fp);               // biCompression
    fputlong(height * stride, fp); // biSizeImage
    fputlong(5000, fp);            // biXPelsPerMeter
    fputlong(5000, fp);            // biYPelsPerMeter
    fputlong(0, fp);               // biClrUsed
    fputlong(0, fp);               // biClrImportant

    // Palette Entries
    for (p = 0; p < paletteEntries; p++)
    {
        unsigned char v = p * 256 / paletteEntries;
        fputc(v, fp); fputc(v, fp); fputc(v, fp); fputc(0x00, fp); 
    }
    
    // Bitmap data
    for (y = height - 1; y >= 0; y--)
    {
        int len = stride;
        if (len > inputStride) { len = inputStride; }
        fwrite((const unsigned char *)buffer + y * inputStride, 1, len, fp);

        // End of line padding
        { 
            int padding = stride - len;
            while (padding--) { fputc(0x00, fp); }
        }
    }

    fclose(fp);
    return 0;
}
