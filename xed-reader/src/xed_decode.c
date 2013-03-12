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
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xed/xed.h"


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
        xed_stream_frame_t frame;
        xed_frame_info_t frameInfo;

        ret = XedReadFrame(reader, &frame, &frameInfo, buffer, bufferSize);
        //printf("%d;", frame.streamId);
        if (ret != XED_OK)
        {
            if (ret == XED_E_ABORT) { fprintf(stderr, "NOTE: Stopped reading file (%d frames in stream 0, %0.2fs @ 30 Hz)\n", count, count / 30.0f); }
            else { fprintf(stderr, "ERROR: Problem reading file (%d)\n", ret); }
            break;
        }

        if (frame.streamId == 0) { count++; }

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
