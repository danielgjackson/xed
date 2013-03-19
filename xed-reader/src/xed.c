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
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define fopen64 fopen
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
typedef long long off64_t;
#endif

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
    xed_file_header_t header;
    xed_end_stream_info_t streamInfo[XED_MAX_STREAMS];
    xed_index_t *streamIndex[XED_MAX_STREAMS];
    int totalEvents;
    xed_index_t **globalIndex;
} xed_reader_t;


// Read an index entry (xed_index_entry_t)
static int XedReadIndexEntry(xed_reader_t *reader, xed_index_entry_t *indexEntry)
{
    // If reading to nowhere, skip entry
    if (indexEntry == NULL)
    {
        fseeko64(reader->fp, 24, SEEK_CUR); 
        return XED_OK;
    }

    indexEntry->frameFileOffset = fget_uint64(reader->fp);  // @ 0 (e.g. 0x000000004c002bfc, can point to first frame)
    indexEntry->frameTimestamp = fget_uint64(reader->fp);   // @ 8 (e.g. 0x000000038f84d534, or 0 if none)
    indexEntry->dataSize= fget_uint32(reader->fp);          // @16 (e.g. 614400)
    indexEntry->dataSize2 = fget_uint32(reader->fp);        // @20 (e.g. 614400)
    return XED_OK;
}

// Read a frame information entry (xed_frame_info_t)
static int XedReadFrameInfo(xed_reader_t *reader, xed_frame_info_t *indexEntry, size_t frameInfoSize)
{
    size_t len;

    // Clear current value
    if (indexEntry != NULL)
    {
        memset(indexEntry, 0, sizeof(xed_frame_info_t));
    }

    // Work out how much to read
    len = 0;
    if (indexEntry != NULL)
    {
        len = sizeof(xed_frame_info_t);
        if (frameInfoSize < len) { len = frameInfoSize; }
        if (len > 0)
        { 
            fread(indexEntry, 1, len, reader->fp); 
        }
    }

    // Skip any unread data
    if (frameInfoSize > len) 
    { 
        fseeko64(reader->fp, frameInfoSize - len, SEEK_CUR); 
    }

    return XED_OK;
}

// Read the file header, end information and indexes
static int XedReadFileMetadata(xed_reader_t *reader)
{
    int i, numEndStreamInfo;

    if (reader == NULL) { return XED_E_POINTER; }
    if (reader->fp == NULL) { return XED_E_NOT_VALID_STATE; }
    
    memset(&reader->header, 0, sizeof(xed_file_header_t));

    // Read header
    if (fseeko64(reader->fp, 0, SEEK_SET) != 0) { return XED_E_ACCESS_DENIED; }
    if (fread(reader->header.fileType, 1, sizeof(reader->header.fileType), reader->fp) != sizeof(reader->header.fileType)) { return XED_E_ACCESS_DENIED; }
    reader->header._version = fget_uint32(reader->fp);
    reader->header.numStreams = fget_uint32(reader->fp);
    reader->header.indexFileOffset = fget_uint64(reader->fp);

    // Check header
    if (reader->header.fileType[0] != 'E' || reader->header.fileType[1] != 'V' || reader->header.fileType[2] != 'E' || reader->header.fileType[3] != 'N' || reader->header.fileType[4] != 'T' || reader->header.fileType[5] != 'S' || reader->header.fileType[6] != '1' || reader->header.fileType[7] != '\0')
    {
        fprintf(stderr, "ERROR: File header not the expected \"EVENTS1\", was: \"%08s\".\n", reader->header.fileType);
        return XED_E_INVALID_DATA;
    }

    // Read end of file information
    if (reader->header.indexFileOffset == 0) { return XED_E_INVALID_DATA; }
    if (fseeko64(reader->fp, reader->header.indexFileOffset, SEEK_SET) != 0) { return XED_E_ACCESS_DENIED; }
    numEndStreamInfo = fget_uint16(reader->fp);
    if (numEndStreamInfo != reader->header.numStreams) { fprintf(stderr, "WARNING: Number of end stream information blocks (%d) not the same as the number of blocks (%d)\n", numEndStreamInfo, reader->header.numStreams);  }

    // Read xed_end_stream_info_t
    for (i = 0; i < numEndStreamInfo; i++)
    {
        // Parse xed_end_stream_info_t
        xed_end_stream_info_t endStreamInfo = {0};

        endStreamInfo._unknown1 = fget_uint16(reader->fp);          // @  0 = 0xffff
        endStreamInfo._unknown2 = fget_uint16(reader->fp);          // @  2 = 0xffff

        if (endStreamInfo._unknown1 != 0xffff || endStreamInfo._unknown2 != 0xffff) { fprintf(stderr, "ERROR: End stream info #%d does not start with expected 0xffff 0xffff\n", i); return XED_E_INVALID_DATA; }

        endStreamInfo.streamNumber = fget_uint16(reader->fp);       // @  4 = 0/1/2/3/4
        if (endStreamInfo.streamNumber != i) { fprintf(stderr, "WARNING: End stream info #%d is not for the expected stream. (=%d)\n", i, endStreamInfo.streamNumber);  }

        endStreamInfo.extraPerIndexEntry = fget_uint16(reader->fp); // @  6 Length of xed_frame_info_t in index = 24 [have seen trimmed file with length 0, with no xed_frame_info_t entries in the index]
        endStreamInfo.totalIndexEntries = fget_uint32(reader->fp);  // @  8 Total number of frames (index entries) in the file = 2078 / 2
        endStreamInfo.frameSize = fget_uint32(reader->fp);          // @ 12 Size of frame (e.g. = 614400 / 0 / 0 / 0 / 0)
        endStreamInfo.maxIndexEntries = fget_uint32(reader->fp);    // @ 16 max entries per index = 1024
        endStreamInfo.numIndexes = fget_uint32(reader->fp);         // @ 20 number of indexes = 3 / 1 / 1 / 1 / 1

        XedReadIndexEntry(reader, &endStreamInfo.event0);           // @ 24 Index entry for event 0 (xed_initial_data_t) information
        XedReadIndexEntry(reader, &endStreamInfo.event1);           // @ 48 Index entry for event 1 (xed_event_empty_t) information

        fread(endStreamInfo._unknownEvent0, 1, sizeof(endStreamInfo._unknownEvent0), reader->fp);   // @72
        fread(endStreamInfo._unknownEvent1, 1, sizeof(endStreamInfo._unknownEvent1), reader->fp);   // @96

        XedReadFrameInfo(reader, NULL, endStreamInfo.extraPerIndexEntry);   // @120 <only if extraPerIndexEntry=24, missing if =0 >
        XedReadFrameInfo(reader, NULL, endStreamInfo.extraPerIndexEntry);   // @148 <only if extraPerIndexEntry=24, missing if =0 >

        // Only load the index entries for streams we will store
        if (endStreamInfo.streamNumber < XED_MAX_STREAMS && endStreamInfo.streamNumber < reader->header.numStreams)
        {
            unsigned long 
            int j;
            off64_t offset;

            if (reader->streamIndex[endStreamInfo.streamNumber] != NULL)
            {
                fprintf(stderr, "ERROR: Stream already indexed %d\n", endStreamInfo.streamNumber, endStreamInfo.totalIndexEntries);
                return XED_E_INVALID_DATA;
            }

            // Allocate space for index buffers
            reader->streamIndex[endStreamInfo.streamNumber] = (xed_index_t *)malloc(sizeof(xed_index_t) * endStreamInfo.totalIndexEntries);
            if (reader->streamIndex[endStreamInfo.streamNumber] == NULL)
            {
                fprintf(stderr, "ERROR: Problem allocating index entries for stream %d: %d\n", endStreamInfo.streamNumber, endStreamInfo.totalIndexEntries);
                return XED_E_OUT_OF_MEMORY;
            }

            // Clear entries
            memset(reader->streamIndex[endStreamInfo.streamNumber], 0, sizeof(xed_index_t) * endStreamInfo.totalIndexEntries);

            // @~168 <@120 in trimmed> (numIndexes *) File offset of xed_stream_index_t structures (e.g. = 0x4c098c2c / 0x4c0991e4 / 0x4c0925c / 0x4c0992d4 / 0x4c09934c)
            offset = ftello64(reader->fp);
            for (j = 0; j < endStreamInfo.numIndexes; j++)
            {
                uint64_t indexOffset;
                int indexBase;
                xed_stream_index_t index;
                unsigned int k;

                // Get address of index
                fseeko64(reader->fp, offset + j * sizeof(uint64_t), SEEK_SET);
                indexOffset = fget_uint64(reader->fp);
                fseeko64(reader->fp, indexOffset, SEEK_SET);

                index.packetType = fget_uint16(reader->fp);     // @0 = 0xffff
                if (index.packetType != 0xffff) { fprintf(stderr, "ERROR: Index #%d for stream #%d does not start with expected 0xffff\n", j, i); return XED_E_INVALID_DATA; }
                index._unknown1 = fget_uint16(reader->fp);      // @2 = 0
                index.numEntries = fget_uint32(reader->fp);     // @4 (e.g. = 1024 | 1024 | ... | 30 / 2 / 2 / 2 / 2)
                index._unknown2 = fget_uint32(reader->fp);      // @8 (e.g. = 0xf934b72c | 0xe418b73d | ... | 0x1ea8f030 / 0x6f970162 / 0xa75d020c / 0x37c900b8 / 0x6f8f0162)
                index._unknown3 = fget_uint32(reader->fp);      // @12 = 0
                index._unknown4 = fget_uint32(reader->fp);      // @16 = 0
                index._unknown5 = fget_uint32(reader->fp);      // @20 = 0

                // Read index entries
                indexBase = j * endStreamInfo.maxIndexEntries;
                if (indexBase + index.numEntries > endStreamInfo.totalIndexEntries)
                {
                    fprintf(stderr, "ERROR: Index #%d for stream #%d exceeds total index entries (%d).\n", j, i, endStreamInfo.totalIndexEntries); 
                    return XED_E_INVALID_DATA;
                }

                // xed_index_entry_t indexEntries[numEntries];
                for (k = 0; k < index.numEntries; k++)
                {
                    // Set stream id
                    reader->streamIndex[endStreamInfo.streamNumber][indexBase + k].streamId = endStreamInfo.streamNumber;
                    // Read index entry
                    XedReadIndexEntry(reader, &reader->streamIndex[endStreamInfo.streamNumber][indexBase + k].indexEntry);
                }

                // xed_frame_info_t frameInfo[numEntries];  // <does this depend on _additionalLength or extraPerIndexEntry?>; {0} if none (e.g. first two frames)
                if (endStreamInfo.extraPerIndexEntry > 0)
                {
                    // Read additional frame information
                    for (k = 0; k < index.numEntries; k++)
                    {
                        XedReadFrameInfo(reader, &reader->streamIndex[endStreamInfo.streamNumber][indexBase + k].frameInfo, endStreamInfo.extraPerIndexEntry);
                    }
                }
                
            }

            // Seek to after last index
            fseeko64(reader->fp, offset + endStreamInfo.numIndexes * sizeof(uint64_t), SEEK_SET);

        }
        else
        {
            fseeko64(reader->fp, sizeof(uint64_t) * endStreamInfo.numIndexes, SEEK_CUR);   // @~168 <@120 in trimmed> (numIndexes *) File offset of xed_stream_index_t structures (e.g. = 0x4c098c2c / 0x4c0991e4 / 0x4c0925c / 0x4c0992d4 / 0x4c09934c)
        }

        endStreamInfo._unknown11 = fget_uint32(reader->fp);     // @~192/176 ? timestamp/flags ? (e.g. = 0x8ad51914 / 0x965f0748 / 0xefc8076c / 0x3a400691 / 0x93a906b5)

        // Copy end stream info structure
        if (endStreamInfo.streamNumber < XED_MAX_STREAMS && endStreamInfo.streamNumber < reader->header.numStreams)
        {
            reader->streamInfo[endStreamInfo.streamNumber] = endStreamInfo;
        }
        else
        {
            fprintf(stderr, "WARNING: Ignoring end stream information for stream number %d as file maximum was %d and compiled-in maximum was %d\n", endStreamInfo.streamNumber, reader->header.numStreams, XED_MAX_STREAMS);
        }

    }

    // Seek back to first event
    if (fseeko64(reader->fp, 24, SEEK_SET) != 0) { return XED_E_INVALID_DATA; }

    // Create a super index of all events
    {
        int maxEvents = 0;
        int indexEntry[XED_MAX_STREAMS] = {0};
        int numStreams = reader->header.numStreams;
        if (numStreams > XED_MAX_STREAMS) { numStreams = XED_MAX_STREAMS; }

        // Count the total number of index entries
        maxEvents = 0;
        for (i = 0; i < numStreams; i++)
        {
            maxEvents += reader->streamInfo[i].totalIndexEntries;
        }

        // Allocate global index
        reader->globalIndex = (xed_index_t **)malloc(sizeof(xed_index_t *) * maxEvents);
        if (reader->globalIndex == NULL)
        {
            fprintf(stderr, "ERROR: Problem allocating global index entries (%d)\n", maxEvents);
            return XED_E_OUT_OF_MEMORY;
        }

        // Create global index
        reader->totalEvents = 0;
        for (;;)
        {
            int j;
            int streamId = -1;
            uint64_t nextOffset = 0;

            // Find the next event from any of the streams
            for (j = 0; j < numStreams; j++)
            {
                // If we still have more events in the stream
                if (indexEntry[j] < (int)reader->streamInfo[j].totalIndexEntries)
                {
                    uint64_t offs = reader->streamIndex[j][indexEntry[j]].indexEntry.frameFileOffset;
                    if (streamId < 0 || offs < nextOffset)
                    {
                        streamId = j;
                        nextOffset = offs;
                    }
                }
            }

            // Exit when no more entries
            if (streamId < 0) { break; }

            // Check if we're trying to overflow the global index (shouldn't be possible)
            if (reader->totalEvents >= maxEvents)
            {
                fprintf(stderr, "WARNING: Tried to overflow global index (%d)\n", maxEvents);
                break;
            }

            // Assign next global index entry to this stream's index entry
            reader->globalIndex[reader->totalEvents] = &reader->streamIndex[streamId][indexEntry[streamId]];
            reader->totalEvents++;      // Increment global index
            indexEntry[streamId]++;     // Increment stream index
        }

        // Check we filled the global index (should be impossible not to)
        if (reader->totalEvents != maxEvents)
        {
            fprintf(stderr, "WARNING: Global index only has %d / %d entries\n", reader->totalEvents, maxEvents);
        }
    }

    return XED_OK;
}


// Open an XED input file and create a new reader structure
xed_reader_t *XedNewReader(const char *filename)
{
    // Create new reader structure
    xed_reader_t *reader = (xed_reader_t *)malloc(sizeof(xed_reader_t));
    if (reader == NULL) { return NULL; }                    // XED_E_OUT_OF_MEMORY
    memset(reader, 0, sizeof(xed_reader_t));

    // Open input file
    reader->fp = fopen64(filename, "rb");
    if (reader->fp == NULL) { free(reader); return NULL; }  // XED_E_ACCESS_DENIED

    // Read metadata
    if (XedReadFileMetadata(reader) != XED_OK)
    {
        fprintf(stderr, "ERROR: Problem parsing file: %s\n", filename); 
        XedCloseReader(reader);
        free(reader); 
        return NULL;
    }

    return reader;
}

// Close the file and free the reader structure
int XedCloseReader(xed_reader_t *reader)
{
    int i;

    if (reader == NULL) { return XED_E_POINTER; }

    if (reader->fp != NULL)
    {
        fclose(reader->fp);
        reader->fp = NULL;
    }

    if (reader->globalIndex != NULL)
    {
        free(reader->globalIndex);
        reader->globalIndex = NULL;
        reader->totalEvents = 0;
    }

    for (i = 0; i < XED_MAX_STREAMS; i++)
    {
        if (reader->streamIndex[i] != NULL)
        {
            free(reader->streamIndex[i]);
            reader->streamIndex[i] = NULL;
        }
    }

    free(reader);
    return XED_OK;
}


// Get the number of events
int XedGetNumEvents(xed_reader_t *reader, int stream)
{
    if (reader == NULL) { return XED_E_POINTER; }
    if (stream == XED_STREAM_ALL)
    {
        return reader->totalEvents;
    }
    else if (stream >= 0 && stream < (int)reader->header.numStreams && stream < XED_MAX_STREAMS)
    {
        return reader->streamInfo[stream].totalIndexEntries;
    }
    else
    {
        return XED_E_INVALID_ARG;
    }
}

// Get an event index
const xed_index_t *XedGetIndexEntry(xed_reader_t *reader, int stream, int index)
{
    if (reader == NULL) { return NULL; } // XED_E_POINTER
    if (stream == XED_STREAM_ALL)
    {
        if (index >= 0 && index < reader->totalEvents)
        {
            return reader->globalIndex[index];
        }
        else
        {
            return NULL; // XED_E_INVALID_ARG
        }
    }
    else if (stream >= 0 && stream < (int)reader->header.numStreams && stream < XED_MAX_STREAMS)
    {
        if (index >= 0 && index < (int)reader->streamInfo[stream].totalIndexEntries)
        {
            return &reader->streamIndex[stream][index];
        }
        else
        {
            return NULL; // XED_E_INVALID_ARG
        }
    }
    else
    {
        return NULL;    // XED_E_INVALID_ARG;
    }
}

// Read an event
int XedReadEvent(xed_reader_t *reader, int stream, int index, xed_event_t *event, xed_frame_info_t *frameInfo, void *buffer, size_t bufferSize)
{
    size_t size;
    const xed_index_t *indexEntry; 

    if (reader == NULL) { return XED_E_POINTER; }
    if (reader->fp == NULL) { return XED_E_NOT_VALID_STATE; }
    if (bufferSize > 0 && buffer == NULL) { return XED_E_POINTER; }
    
    indexEntry = XedGetIndexEntry(reader, stream, index);
    if (indexEntry == NULL) { return XED_E_INVALID_ARG; }

    fseeko64(reader->fp, indexEntry->indexEntry.frameFileOffset, SEEK_SET);

fprintf(stderr, "<@%ld>", ftell(reader->fp));

    event->streamId = fget_uint16(reader->fp);
    event->_flags = fget_uint16(reader->fp);
    event->length = fget_uint32(reader->fp);
    event->timestamp = fget_uint64(reader->fp);
    event->_unknown1 = fget_uint32(reader->fp);
    event->length2 = fget_uint32(reader->fp);

    // Assume the payload size is the length specified
    size = event->length;
    //if (event->timestamp != 0) { size = event->length2; }

    // If this is an index, modify for the size of the index
    if (event->streamId == 0xffff)
    {
        int additional = 0;
        additional = 24;
fprintf(stderr, "NOTE: Unexpected index (0x%04x.%d) -- skipping assuming has 24-bytes additional data %d/%d entries\n", event->streamId, event->_flags, event->length, event->length2);
        size *= (24 + additional);
    }
    else if (event->streamId == reader->header.numStreams)
    {
        // Probably the index location packet, stop parsing
fprintf(stderr, "ERROR: Unexpected stream number (probably the index location packet) %d.\n", event->streamId);
return XED_E_ABORT;
    }
    else if (event->streamId > reader->header.numStreams)
    {
        // Unexpected stream number
fprintf(stderr, "ERROR: Unexpected stream number %d.", event->streamId);
        return XED_E_INVALID_DATA;
    }
    else if (event->timestamp != 0)
    {
        // If we have a timestamp, read the event info first
        frameInfo->_unknown1 = fget_uint16_be(reader->fp);
        frameInfo->_unknown2 = fget_uint16_be(reader->fp);
        frameInfo->_unknown3 = fget_uint16_be(reader->fp);
        frameInfo->_unknown4 = fget_uint16_be(reader->fp);
        frameInfo->width = fget_uint16_be(reader->fp);
        frameInfo->height = fget_uint16_be(reader->fp);
        frameInfo->sequenceNumber = fget_uint32_be(reader->fp);
        frameInfo->_unknown5 = fget_uint32_be(reader->fp);
        frameInfo->timestamp = fget_uint32_be(reader->fp);
    } else { memset(frameInfo, 0, sizeof(xed_frame_info_t)); }

fprintf(stderr, "<%d|%d=%d>", event->length, event->length2, size);
fprintf(stderr, "=%d.%d;", event->streamId, event->_flags);

    // Read buffer
    {
        size_t readSize = size;
        if (size > bufferSize) { size = bufferSize; }
        if (fread(buffer, 1, readSize, reader->fp) != readSize) { return XED_E_ACCESS_DENIED; }
        // Skip any remaining bytes
        if (readSize < size)
        {
            fseeko64(reader->fp, size - readSize, SEEK_CUR);
        }
    }

    return XED_OK;
}
