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

// XED File Format Parser
// Dan Jackson, 2013

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xed/xed.h"


// Utility methods
static uint16_t fget_uint16(FILE *fp) { uint16_t v = 0; v |= ((uint16_t)fgetc(fp)); v |= (((uint16_t)fgetc(fp)) << 8); return v; }
static uint32_t fget_uint32(FILE *fp) { uint32_t v = 0; v |= ((uint32_t)fgetc(fp)); v |= (((uint32_t)fgetc(fp)) << 8); v |= (((uint32_t)fgetc(fp)) << 16); v |= (((uint32_t)fgetc(fp)) << 24); return v; }
static uint64_t fget_uint64(FILE *fp) { uint64_t v = 0; v |= ((uint64_t)fgetc(fp)); v |= (((uint64_t)fgetc(fp)) << 8); v |= (((uint64_t)fgetc(fp)) << 16); v |= (((uint64_t)fgetc(fp)) << 24); v |= (((uint64_t)fgetc(fp)) << 32); v |= (((uint64_t)fgetc(fp)) << 40); v |= (((uint64_t)fgetc(fp)) << 48); v |= (((uint64_t)fgetc(fp)) << 56); return v; }
static uint16_t fget_uint16_be(FILE *fp) { uint16_t v = 0; v |= ((uint16_t)fgetc(fp) << 8); v |= (((uint16_t)fgetc(fp))); return v; }
static uint32_t fget_uint32_be(FILE *fp) { uint32_t v = 0; v |= ((uint32_t)fgetc(fp) << 24); v |= (((uint32_t)fgetc(fp)) << 16); v |= (((uint32_t)fgetc(fp)) << 8); v |= (((uint32_t)fgetc(fp))); return v; }


// Reader state structure
typedef struct xed_reader
{
    FILE *fp;
    int numStreams;
} xed_reader_t;


// Open an XED input file and create a new reader structure
xed_reader_t *XedNewReader(const char *filename)
{
    // Create new reader structure
    xed_reader_t *reader = (xed_reader_t *)malloc(sizeof(xed_reader_t));
    if (reader == NULL) { return NULL; }                    // XED_E_OUT_OF_MEMORY
    memset(reader, 0, sizeof(xed_reader_t));

    // Open input file
    reader->fp = fopen(filename, "rb");
    if (reader->fp == NULL) { free(reader); return NULL; }  // XED_E_ACCESS_DENIED

    return reader;
}

// Close the file and free the reader structure
int XedCloseReader(xed_reader_t *reader)
{
    if (reader == NULL) { return XED_E_POINTER; }
    if (reader->fp != NULL)
    {
        fclose(reader->fp);
    }
    free(reader);
    return XED_OK;
}

// Read the file header
int XedReadFileHeader(xed_reader_t *reader, xed_file_header_t *header)
{
    uint32_t _version;          // @ 8 (version?) = 3
    uint32_t _numStreams;       // @12 (number of streams?) = 5
    uint64_t _indexFileOffset;  // @16 File offset of xed_index_location_t;
                                // @24 <end>, followed by:

    if (reader == NULL) { return XED_E_POINTER; }
    if (reader->fp == NULL) { return XED_E_NOT_VALID_STATE; }
    if (header == NULL) { return XED_E_POINTER; }
    if (ftell(reader->fp) != 0) { return XED_E_NOT_VALID_STATE; }

    memset(header, 0, sizeof(xed_file_header_t));

    // Read header
    if (fread(header->tag, 1, sizeof(header->tag), reader->fp) != sizeof(header->tag)) { return XED_E_ACCESS_DENIED; }
    header->_version = fget_uint32(reader->fp);
    header->numStreams = fget_uint32(reader->fp);
    header->indexFileOffset = fget_uint64(reader->fp);

    // Check header
    if (header->tag[0] != 'E' || header->tag[1] != 'V' || header->tag[2] != 'E' || header->tag[3] != 'N' || header->tag[4] != 'T' || header->tag[5] != 'S' || header->tag[6] != '1' || header->tag[7] != '\0')
    {
        return XED_E_INVALID_DATA;
    }

    reader->numStreams = header->numStreams;

    return XED_OK;
}


// Read a frame
int XedReadFrame(xed_reader_t *reader, xed_stream_frame_t *frame, xed_frame_info_t *frameInfo, void *buffer, size_t bufferSize)
{
    size_t size;

    if (reader == NULL) { return XED_E_POINTER; }
    if (reader->fp == NULL) { return XED_E_NOT_VALID_STATE; }
    if (bufferSize > 0 && buffer == NULL) { return XED_E_POINTER; }

printf("<@%ld>", ftell(reader->fp));

    frame->streamId = fget_uint16(reader->fp);
    frame->_packetType = fget_uint16(reader->fp);
    frame->length = fget_uint32(reader->fp);
    frame->timestamp = fget_uint64(reader->fp);
    frame->_unknown1 = fget_uint32(reader->fp);
    frame->length2 = fget_uint32(reader->fp);

    // Assume the payload size is the length specified
    size = frame->length;
    //if (frame->timestamp != 0) { size = frame->length2; }

    // If this is an index, modify for the size of the index
    if (frame->streamId == 0xffff)
    {
        size *= (24 + 24);
    }
    else if (frame->streamId == reader->numStreams)
    {
        // Probably the index location packet, stop parsing
return XED_E_ABORT;
    }
    else if (frame->streamId > reader->numStreams)
    {
        // Unexpected stream number
        return XED_E_INVALID_DATA;
    }
    else if (frame->timestamp != 0)
    {
        // If we have a timestamp, read the frame info first
        frameInfo->_unknown1 = fget_uint16_be(reader->fp);
        frameInfo->_unknown2 = fget_uint16_be(reader->fp);
        frameInfo->_unknown3 = fget_uint16_be(reader->fp);
        frameInfo->_unknown4 = fget_uint16_be(reader->fp);
        frameInfo->width = fget_uint16_be(reader->fp);
        frameInfo->height = fget_uint16_be(reader->fp);
        frameInfo->sequenceNumber = fget_uint32_be(reader->fp);
        frameInfo->_unknown5 = fget_uint32_be(reader->fp);
        frameInfo->timestamp = fget_uint32_be(reader->fp);
    }

printf("<%d|%d=%d>", frame->length, frame->length2, size);
printf("=%d.%d;", frame->streamId, frame->_packetType);

    // Read buffer
    {
        size_t readSize = size;
        if (size > bufferSize) { size = bufferSize; }
        if (fread(buffer, 1, readSize, reader->fp) != readSize) { return XED_E_ACCESS_DENIED; }
        // Skip any remaining bytes
        if (readSize < size)
        {
            fseek(reader->fp, size - readSize, SEEK_CUR);
        }
    }

    return XED_OK;
}
