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

// XED File Format Parser Example
// Dan Jackson, 2013

#ifdef _WIN32
#define strcasecmp _stricmp
//#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xed/xed.h"
#include "xed/bmp.h"


int xed_decode(const char *filename)
{
    size_t bufferSize = 1024 * 768 * 3;
    void *buffer;
    struct xed_reader *reader;
    xed_file_header_t header;
    int count = 0;

    // Create reader
    reader = XedNewReader(filename);
    if (reader == NULL)
    { 
        fprintf(stderr, "ERROR: Problem opening reader for file: %s\n", filename); 
        return 1;
    }

    // Read header
    if (XedReadFileHeader(reader, &header) != XED_OK)
    {
        fprintf(stderr, "ERROR: Problem parsing header of file: %s\n", filename); 
        XedCloseReader(reader);
        return 2;
    }

    // Allocate buffer
    buffer = malloc(bufferSize);
    if (buffer == NULL) { fprintf(stderr, "ERROR: Out of memory.\n"); XedCloseReader(reader); return -2; }

    // Read packets
    for (;;)
    {
        int ret;
        xed_event_t frame;
        xed_frame_info_t frameInfo;

        ret = XedReadFrame(reader, &frame, &frameInfo, buffer, bufferSize);
        //printf("%d;", frame.streamId);
        if (ret != XED_OK)
        {
            if (ret == XED_E_ABORT) { fprintf(stderr, "NOTE: Stopped reading file (%d frames in stream 0, %0.2fs @ 30 Hz)\n", count, count / 30.0f); }
            else { fprintf(stderr, "ERROR: Problem reading file (%d)\n", ret); }
            break;
        }

        if (frame.streamId == 0)
        { 
            // Save snapshots
            if ((count % 30 % 3) == 0 && frameInfo.width > 0 && frameInfo.height > 0)
            {
                int width = frameInfo.width, height = frameInfo.height;

#if 1
                // Arrange 16-bit buffer
                {
                    int y;
                    for (y = 0; y < height; y++)
                    {
                        unsigned char *p = (unsigned char *)buffer + (y * (width * 2)); 
                        int x;
                        for (x = 0; x < width; x++)
                        {
                            unsigned short v = ((unsigned short)p[0] << 8) | p[1]; // Read (big endian)

                            // Mask for depth-only
                            v &= 0x0fff;

// Stretch
if (v < 850) { v = 0; } else { v = (unsigned short)((int)(v - 850) * 4096 / (4000 - 850)); if (v >= 4096) { v = 4095; } }

// Band
//if (y < 16) { v = (unsigned short)((int)4096 * x / width); }

                            // Simple greyscale
                            //r = g = b = (unsigned char)(v >> 4);

#if 1
                            {
                                // Convert to RGB
                                unsigned char r, g, b;
                                unsigned char z;

                                //const int DEPTH_UNKNOWN = 0;
                                //const int DEPTH_MIN = 800;  // 850;
                                //const int DEPTH_MAX = 4100; // 4000;

                                #define RGB_MAX 255
                                #define V_MAX 4096

                                z = (unsigned char)(RGB_MAX * ((int)v % (V_MAX / 6 + 1)) / (V_MAX / 6 + 1));
                                if      (v < (1 * V_MAX / 6)) { r = RGB_MAX;     g = z;           b = 0; }
                                else if (v < (2 * V_MAX / 6)) { r = RGB_MAX - z; g = RGB_MAX;     b = 0; }
                                else if (v < (3 * V_MAX / 6)) { r = 0;           g = RGB_MAX;     b = z; }
                                else if (v < (4 * V_MAX / 6)) { r = 0;           g = RGB_MAX - z; b = RGB_MAX; }
                                else if (v < (5 * V_MAX / 6)) { r = z;           g = 0;           b = RGB_MAX; }
                                //else                          { r = RGB_MAX;     g = 0;           b = RGB_MAX - z; }
                                else                          { r = RGB_MAX;     g = z;           b = RGB_MAX; }

                                // Convert to RGB555
                                v = ((unsigned short)(r >> 3) << 10) | ((unsigned short)(g >> 3) << 5) | ((unsigned short)(b >> 3) << 0);
                            }
#endif

                            // Write back (little endian)
                            p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);

                            p += 2;

                        }
                    }
                }

                // Write image
                {
                    char filename[32];
                    sprintf(filename, "out16-%0d.bmp", count / 30);
                    BitmapWrite(filename, buffer, 16, width, width * 2, height);
                }
#endif


            }
            count++;
        }

    }

    // Close reader
    XedCloseReader(reader);

    free(buffer);

    return 0;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    char help = 0;
    int positional;
    int i;
    const char *infile = NULL;
    
    fprintf(stderr, "XED File Format Parser\n");
    fprintf(stderr, "2013, Dan Jackson\n");
    fprintf(stderr, "\n");
        
    positional = 0;
    for (i = 1; i < argc; i++)
    {
        if (!strcasecmp(argv[i], "--help")) { help = 1; break; }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "ERROR: Unknown option: %s\n", argv[i]); 
            help = 1;
            break;
        }
        else 
        {
            positional++;
            if (positional == 1)
            {
                infile = argv[i];
            }
            else
            {
                fprintf(stderr, "ERROR: Unknown positional parameter: %s\n", argv[i]); 
                help = 1;
                break;
            }
        }
    }
    
    if (infile == NULL) { fprintf(stderr, "ERROR: Input file not specified.\n"); help = 1; }
    
    if (help)
    {
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage: xed_decode <input.xed>\n");
        fprintf(stderr, "\n");
        ret = -1;
    }
    else
    {
        fprintf(stderr, "NOTE: Processing: %s\n", infile); 
        ret = xed_decode(infile);
        fprintf(stderr, "NOTE: End processing\n"); 
    }
   
#if defined(_WIN32) && defined(_DEBUG)
    getchar();
#endif
    return ret;
}
