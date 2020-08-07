/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: interface_stats.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "interface_stats.pb-c.h"
void   intf__stats__observation_point__init
                     (Intf__Stats__ObservationPoint         *message)
{
  static Intf__Stats__ObservationPoint init_value = INTF__STATS__OBSERVATION_POINT__INIT;
  *message = init_value;
}
size_t intf__stats__observation_point__get_packed_size
                     (const Intf__Stats__ObservationPoint *message)
{
  assert(message->base.descriptor == &intf__stats__observation_point__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t intf__stats__observation_point__pack
                     (const Intf__Stats__ObservationPoint *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &intf__stats__observation_point__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t intf__stats__observation_point__pack_to_buffer
                     (const Intf__Stats__ObservationPoint *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &intf__stats__observation_point__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Intf__Stats__ObservationPoint *
       intf__stats__observation_point__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Intf__Stats__ObservationPoint *)
     protobuf_c_message_unpack (&intf__stats__observation_point__descriptor,
                                allocator, len, data);
}
void   intf__stats__observation_point__free_unpacked
                     (Intf__Stats__ObservationPoint *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &intf__stats__observation_point__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   intf__stats__intf_stats__init
                     (Intf__Stats__IntfStats         *message)
{
  static Intf__Stats__IntfStats init_value = INTF__STATS__INTF_STATS__INIT;
  *message = init_value;
}
size_t intf__stats__intf_stats__get_packed_size
                     (const Intf__Stats__IntfStats *message)
{
  assert(message->base.descriptor == &intf__stats__intf_stats__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t intf__stats__intf_stats__pack
                     (const Intf__Stats__IntfStats *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &intf__stats__intf_stats__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t intf__stats__intf_stats__pack_to_buffer
                     (const Intf__Stats__IntfStats *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &intf__stats__intf_stats__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Intf__Stats__IntfStats *
       intf__stats__intf_stats__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Intf__Stats__IntfStats *)
     protobuf_c_message_unpack (&intf__stats__intf_stats__descriptor,
                                allocator, len, data);
}
void   intf__stats__intf_stats__free_unpacked
                     (Intf__Stats__IntfStats *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &intf__stats__intf_stats__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   intf__stats__observation_window__init
                     (Intf__Stats__ObservationWindow         *message)
{
  static Intf__Stats__ObservationWindow init_value = INTF__STATS__OBSERVATION_WINDOW__INIT;
  *message = init_value;
}
size_t intf__stats__observation_window__get_packed_size
                     (const Intf__Stats__ObservationWindow *message)
{
  assert(message->base.descriptor == &intf__stats__observation_window__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t intf__stats__observation_window__pack
                     (const Intf__Stats__ObservationWindow *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &intf__stats__observation_window__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t intf__stats__observation_window__pack_to_buffer
                     (const Intf__Stats__ObservationWindow *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &intf__stats__observation_window__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Intf__Stats__ObservationWindow *
       intf__stats__observation_window__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Intf__Stats__ObservationWindow *)
     protobuf_c_message_unpack (&intf__stats__observation_window__descriptor,
                                allocator, len, data);
}
void   intf__stats__observation_window__free_unpacked
                     (Intf__Stats__ObservationWindow *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &intf__stats__observation_window__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   intf__stats__intf_report__init
                     (Intf__Stats__IntfReport         *message)
{
  static Intf__Stats__IntfReport init_value = INTF__STATS__INTF_REPORT__INIT;
  *message = init_value;
}
size_t intf__stats__intf_report__get_packed_size
                     (const Intf__Stats__IntfReport *message)
{
  assert(message->base.descriptor == &intf__stats__intf_report__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t intf__stats__intf_report__pack
                     (const Intf__Stats__IntfReport *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &intf__stats__intf_report__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t intf__stats__intf_report__pack_to_buffer
                     (const Intf__Stats__IntfReport *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &intf__stats__intf_report__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Intf__Stats__IntfReport *
       intf__stats__intf_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Intf__Stats__IntfReport *)
     protobuf_c_message_unpack (&intf__stats__intf_report__descriptor,
                                allocator, len, data);
}
void   intf__stats__intf_report__free_unpacked
                     (Intf__Stats__IntfReport *message,
                      ProtobufCAllocator *allocator)
{
  assert(message->base.descriptor == &intf__stats__intf_report__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor intf__stats__observation_point__field_descriptors[2] =
{
  {
    "nodeId",
    1,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Intf__Stats__ObservationPoint, nodeid),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "locationId",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Intf__Stats__ObservationPoint, locationid),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned intf__stats__observation_point__field_indices_by_name[] = {
  1,   /* field[1] = locationId */
  0,   /* field[0] = nodeId */
};
static const ProtobufCIntRange intf__stats__observation_point__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor intf__stats__observation_point__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "intf.stats.ObservationPoint",
  "ObservationPoint",
  "Intf__Stats__ObservationPoint",
  "intf.stats",
  sizeof(Intf__Stats__ObservationPoint),
  2,
  intf__stats__observation_point__field_descriptors,
  intf__stats__observation_point__field_indices_by_name,
  1,  intf__stats__observation_point__number_ranges,
  (ProtobufCMessageInit) intf__stats__observation_point__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor intf__stats__intf_stats__field_descriptors[6] =
{
  {
    "ifName",
    1,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Intf__Stats__IntfStats, ifname),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "txBytes",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(Intf__Stats__IntfStats, has_txbytes),
    offsetof(Intf__Stats__IntfStats, txbytes),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rxBytes",
    3,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(Intf__Stats__IntfStats, has_rxbytes),
    offsetof(Intf__Stats__IntfStats, rxbytes),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "txPackets",
    4,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(Intf__Stats__IntfStats, has_txpackets),
    offsetof(Intf__Stats__IntfStats, txpackets),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rxPackets",
    5,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(Intf__Stats__IntfStats, has_rxpackets),
    offsetof(Intf__Stats__IntfStats, rxpackets),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "role",
    6,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Intf__Stats__IntfStats, role),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned intf__stats__intf_stats__field_indices_by_name[] = {
  0,   /* field[0] = ifName */
  5,   /* field[5] = role */
  2,   /* field[2] = rxBytes */
  4,   /* field[4] = rxPackets */
  1,   /* field[1] = txBytes */
  3,   /* field[3] = txPackets */
};
static const ProtobufCIntRange intf__stats__intf_stats__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 6 }
};
const ProtobufCMessageDescriptor intf__stats__intf_stats__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "intf.stats.IntfStats",
  "IntfStats",
  "Intf__Stats__IntfStats",
  "intf.stats",
  sizeof(Intf__Stats__IntfStats),
  6,
  intf__stats__intf_stats__field_descriptors,
  intf__stats__intf_stats__field_indices_by_name,
  1,  intf__stats__intf_stats__number_ranges,
  (ProtobufCMessageInit) intf__stats__intf_stats__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor intf__stats__observation_window__field_descriptors[3] =
{
  {
    "startedAt",
    1,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(Intf__Stats__ObservationWindow, has_startedat),
    offsetof(Intf__Stats__ObservationWindow, startedat),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "endedAt",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(Intf__Stats__ObservationWindow, has_endedat),
    offsetof(Intf__Stats__ObservationWindow, endedat),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "intfStats",
    3,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(Intf__Stats__ObservationWindow, n_intfstats),
    offsetof(Intf__Stats__ObservationWindow, intfstats),
    &intf__stats__intf_stats__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned intf__stats__observation_window__field_indices_by_name[] = {
  1,   /* field[1] = endedAt */
  2,   /* field[2] = intfStats */
  0,   /* field[0] = startedAt */
};
static const ProtobufCIntRange intf__stats__observation_window__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 3 }
};
const ProtobufCMessageDescriptor intf__stats__observation_window__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "intf.stats.ObservationWindow",
  "ObservationWindow",
  "Intf__Stats__ObservationWindow",
  "intf.stats",
  sizeof(Intf__Stats__ObservationWindow),
  3,
  intf__stats__observation_window__field_descriptors,
  intf__stats__observation_window__field_indices_by_name,
  1,  intf__stats__observation_window__number_ranges,
  (ProtobufCMessageInit) intf__stats__observation_window__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor intf__stats__intf_report__field_descriptors[3] =
{
  {
    "reportedAt",
    1,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(Intf__Stats__IntfReport, has_reportedat),
    offsetof(Intf__Stats__IntfReport, reportedat),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "observationPoint",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(Intf__Stats__IntfReport, observationpoint),
    &intf__stats__observation_point__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "observationWindow",
    3,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(Intf__Stats__IntfReport, n_observationwindow),
    offsetof(Intf__Stats__IntfReport, observationwindow),
    &intf__stats__observation_window__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned intf__stats__intf_report__field_indices_by_name[] = {
  1,   /* field[1] = observationPoint */
  2,   /* field[2] = observationWindow */
  0,   /* field[0] = reportedAt */
};
static const ProtobufCIntRange intf__stats__intf_report__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 3 }
};
const ProtobufCMessageDescriptor intf__stats__intf_report__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "intf.stats.IntfReport",
  "IntfReport",
  "Intf__Stats__IntfReport",
  "intf.stats",
  sizeof(Intf__Stats__IntfReport),
  3,
  intf__stats__intf_report__field_descriptors,
  intf__stats__intf_report__field_indices_by_name,
  1,  intf__stats__intf_report__number_ranges,
  (ProtobufCMessageInit) intf__stats__intf_report__init,
  NULL,NULL,NULL    /* reserved[123] */
};
