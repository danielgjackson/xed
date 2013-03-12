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

// NOTE: Must be some way the stream index data is chained together?
// NOTE: xed_stream_header1_t & xed_stream_header3_t are index entries so are actually normal frames, the 292 length in header 1 maybe covers the size of the additional data
// NOTE: What is in the xed_index_location_entry_t._information for stream 0?
// NOTE: How many timestamp ticks per second (around 50000000?)


#ifndef XED_H
#define XED_H

#include <stdio.h>
#include <stdint.h>


// IMPORTANT: These structures are for information only -- they require proper packing and will have the wrong 'endian' on some platforms.

// [Example file mentioned in comments was depth-only data, file length: 1275696986, 69194 msec, ]

// File header
typedef struct
{
    uint8_t  tag[8];            // @ 0 "EVENTS1\0"
    uint32_t _version;          // @ 8 (version?) = 3
    uint32_t numStreams;        // @12 (number of streams?) = 5
    uint64_t indexFileOffset;   // @16 File offset of xed_index_location_t;
                                // @24 <end>, followed by:
    // struct { xed_stream_header1_t h1, xed_stream_header2_t h2, xed_stream_header3_t h3 } streamHeader[numTracks]; // numTracks * (xed_stream_header1_t, xed_stream_header2_t, xed_stream_header3_t) triplets
} xed_file_header_t;


// stream header 1 (24 bytes)
typedef struct
{
    uint16_t streamId;          // @ 0 Stream ID (= 0/1/2/3/4)
    uint16_t packetType;        // @ 2 ? Packet type = 0x00
    uint32_t _length;           // @ 4 ? (length of xed_stream_header2_t?) = 292
    uint64_t _timestamp;        // @ 8 ? = 0
    uint32_t _unknown1;         // @16 ? value? ( = 0x1450002d / 0x19f60033 / 0x1d580035 / 0x1f9c0037 / 0x2ae00041)
    uint32_t _length2;          // @20 ? (length of xed_stream_header2_t?) = 292
                                // @24 <end>, followed by xed_stream_header2_t
} xed_stream_header1_t;

// stream header 2 (292 bytes)
typedef struct
{
    uint16_t streamId;          // @ 0 Stream ID (= 0/1/2/3/4)
    uint16_t packetType;        // @ 2 ? Packet type = 0x10 [have seen 0x11 on trimmed file]
    uint16_t _flags;            // @ 4 ? (flags?) = 0x00/0x04/0x06/0x07/0x10
    uint8_t  _unknown1[276];    // @ 6 ? = {0}
    uint16_t _unknown2;         // @282 (? length of a structure) = 24
    uint32_t _maxIndexEntries;  // @284 ? max index entries? = 1024
    uint32_t _unknown3;         // @288 ? = 0
                                // @292 <end>, followed by xed_stream_header3_t
} xed_stream_header2_t;

// stream header 3 (24 bytes) [pointed to by xed_index_entry_t.frameFileOffset for unused streams]
typedef struct
{
    uint16_t streamId;          // @ 0 Stream ID (= 0/1/2/3/4)
    uint16_t packetType;        // @ 2 ? Packet type = 0x00 [have seen 0x01 on trimmed file]
    uint32_t _length;           // @ 4 ? length = 0 [have seen 0x08 on trimmed file]
    uint64_t _timestamp;        // @ 8 ? = 0
    uint32_t _unknown1;         // @16 ? value? = 1 [have seen 0x03e2007e on trimmed file]
    uint32_t _length2;          // @20 ? length2 = 0
                                // @24 <end>
    // [on trimmed file saw 8-byte addition with: dword:0x00030178,dword:0x01000000; or word:376,word:3,word:0;word:256]
} xed_stream_header3_t;


// Frame information (24 bytes)
typedef struct
{
    uint16_t _unknown1;         // @ 0 <big-endian> ? = 1
    uint16_t _unknown2;         // @ 2 <big-endian> ? = 0
    uint16_t _unknown3;         // @ 4 <big-endian> ? = 1
    uint16_t _unknown4;         // @ 6 <big-endian> ? = 1
    uint16_t width;             // @ 8 <big-endian> Width (= 640)
    uint16_t height;            // @10 <big-endian> Height (= 480)
    uint32_t sequenceNumber;    // @12 <big-endian> Frame sequence number (e.g. = 0x00000f86 / 0x00000f87 / 0x00000f88 / ... / 0x000017a1 = 6049)
    uint32_t _unknown5;         // @14 <big-endian> ? = 0
    uint32_t timestamp;         // @20 <big-endian> Timestamp (e.g. = 0x0e26da91 / 0x0e275775 / 0x0e275c5d / ... / 0x1246ac60)
                                // @24 <end>
} xed_frame_info_t;


// Frames [Frame #0 @1724, Frame #1 @616172, Frame #2 @1230620, ..., Frame #N @127507676] (each is 48 + 614400 bytes)
typedef struct
{
    uint16_t streamId;          // @ 0 ? Stream ID = 0
    uint16_t _packetType;       // @ 2 ? Packet type = 0x00
    uint32_t length;            // @ 4 Length of data (e.g. = 614400, number of bytes in a 16-bit depth image at 640x480 = 640*480*2)
    uint64_t timestamp;         // @ 8 (e.g. = 0x00000002c1d1d500 / 0x00000002c1ea29eb / 0x00000002c2040e9e)
    uint32_t _unknown1;         // @16 ? value? = {0xfe, 0x4e, 0x44, 0xbc} / {0x20, 0x1e, 0x1e, 0xed} / {0x9b, 0xc3, 0x18, 0x7b} / ...
    uint32_t length2;           // @20 Length of data (e.g. = 614400, number of bytes in a 16-bit depth image at 640x480 = 640*480*2)
    xed_frame_info_t frameInfo; // @24 Frame information (24 bytes)
    uint8_t frameData[];        // @48 Frame data (e.g. 16-bit big-endian depth values, size=614400 bytes)
} xed_stream_frame_t;


// Index [e.g. first @627967580 = 0x256e065c | second @1257211508 = 0x4aef8674 | ... | last @1275694124 = 0x4c098c2c] (24 bytes), remaining index entries for each stream written at the end of the file, followed by an index location
typedef struct
{
    uint16_t packetType;        // @0 = 0xffff
    uint16_t _unknown1;         // @2 = 0
    uint32_t numEntries;        // @4 (e.g. = 1024 | 1024 | ... | 30 / 2 / 2 / 2 / 2)
    uint32_t _unknown2;         // @8 (e.g. = 0xf934b72c | 0xe418b73d | ... | 0x1ea8f030 / 0x6f970162 / 0xa75d020c / 0x37c900b8 / 0x6f8f0162)
    uint32_t _unknown3;         // @12 = 0
    uint32_t _unknown4;         // @16 = 0
    uint32_t _unknown5;         // @20 = 0
                                // @24 <end>, followed by
    // xed_index_entry_t indexEntries[numEntries];
    // xed_frame_info_t frameInfo[numEntries];  // zero if none (e.g. first two frames)
} xed_index_t;


// Index entry (24 bytes)
typedef struct
{
    uint64_t frameFileOffset;   // @ 0 (e.g. 0x000000004c002bfc, can point to first frame)
    uint64_t frameTimestamp;    // @ 8 (e.g. 0x000000038f84d534, or 0 if none)
    uint32_t dataSize;          // @16 (e.g. 614400)
    uint32_t dataSize2;         // @20 (e.g. 614400)
                                // @24 <end>
} xed_index_entry_t;


// Index Location [e.g. @1275696068] (? bytes)
typedef struct
{
    uint16_t numberOfIndexLocations; // @0 = 0x0005
                                     // @2 <end>, followed by:
    // xed_index_location_entry_t indexLocations[numberOfIndexLocations];
} xed_index_location_t;


// Index Location Entry [e.g. @1275696070 1 * 196-byte structure and @1275696266 4 * 180-byte structures] (Contains a pointer to last index)
// [Have seen trimmed file where stream 0 is 180-byte, and stream 1 is 132-byte, and streams 3,4,5 are 180-byte]
typedef struct
{
    uint16_t _unknown1;         // @  0 = 0xffff
    uint16_t _unknown2;         // @  2 = 0xffff
    uint16_t streamNumber;      // @  4 = 0/1/2/3/4
    uint16_t _unknown3;         // @  6 Length of xed_frame_info_t in index = 24 [have seen trimmed file with length 0, with no xed_frame_info_t entries in the index]
    uint32_t _unknown4;         // @  8 = 2078 / 2
    uint32_t frameSize;         // @ 12 Size of frame (e.g. = 614400 / 0 / 0 / 0 / 0)
    uint32_t _maxIndexEntries;  // @ 16 max index entries = 1024
    uint32_t _unknown5;         // @ 20 = 3 / 1 / 1 / 1 / 1 <does bit flag 0x02 decide 128 byte instead of 112 byte data?>
    uint64_t fileOffsetHeader1; // @ 24 File offset of xed_stream_header1_t (e.g. = 0x0018 / 0x016c / 0x02c0 / 0x0414 / 0x0568  <+=340>)
    uint32_t _unknown6;         // @ 32 = 0 
    uint32_t _unknown7;         // @ 36 = 0 
    uint32_t _unknown8;         // @ 40 ? Size of xed_stream_header2_t? (= 292)
    uint32_t _unknown9;         // @ 44 ? Size of xed_stream_header2_t? (= 292)
    uint64_t fileOffsetHeader3; // @ 48 File offset of xed_stream_header3_t (e.g. = 0x0154 / 0x02a8 / 0x03fc / 0x0550 / 0x06a4  <+=340>)
    uint8_t  _information[128 / 112]; // @ 56 EITHER: <128 bytes of data>, OR: <112 bytes>   <or 64 bytes seen in a trimmed stream channel 1> = {0}
    uint64_t _fileOffsetIndex;  // @184/168 File offset of (last?) xed_index_t structure (e.g. = 0x4c098c2c / 0x4c0991e4 / 0x4c0925c / 0x4c0992d4 / 0x4c09934c)
    uint32_t _unknown11;        // @192/176 ? timestamp/flags ? (e.g. = 0x8ad51914 / 0x965f0748 / 0xefc8076c / 0x3a400691 / 0x93a906b5)
                                // @196/180 <end>
} xed_index_location_entry_t;




struct xed_reader;

// Return codes
//#define XED_TRUE                1
//#define XED_FALSE               0
#define XED_OK                  0
#define XED_E_FAIL              -1
#define XED_E_UNEXPECTED        -2
#define XED_E_NOT_VALID_STATE   -3
#define XED_E_OUT_OF_MEMORY     -4
#define XED_E_INVALID_ARG       -5
#define XED_E_POINTER           -6
#define XED_E_NOT_IMPLEMENTED   -7
#define XED_E_ABORT             -8
#define XED_E_ACCESS_DENIED     -9
#define XED_E_INVALID_DATA      -10
#define XED_SUCCEEDED(value)    ((value) >= 0)
#define XED_FAILED(value)       ((value) < 0)


struct xed_reader *XedNewReader(const char *filename);
int XedCloseReader(struct xed_reader *reader);
int XedReadFileHeader(struct xed_reader *reader, xed_file_header_t *header);
int XedReadFrame(struct xed_reader *reader, xed_stream_frame_t *frame, xed_frame_info_t *frameInfo, void *buffer, size_t bufferSize);


#endif

