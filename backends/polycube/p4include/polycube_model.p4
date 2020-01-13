/*
Copyright 2013-present Barefoot Networks, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef _POLYCUBE_MODEL_P4_
#define _POLYCUBE_MODEL_P4_

#include <core.p4>

/*
 Each table must have an implementation property which is either an array_table
 or a hash_table.
*/

/**
 Implementation property for tables indicating that tables must be implemented
 using EBPF array map.  However, if a table uses an LPM match type, the implementation
 is only used for the size, and the table used is an LPM trie.
*/
extern array_table {
    /// @param size: maximum number of entries in table
    array_table(bit<32> size);
}

/**
 Implementation property for tables indicating that tables must be implemented
 using EBPF hash map.
*/
extern hash_table {
    /// @param size: maximum number of entries in table
    hash_table(bit<32> size);
}

/**
 * Polycube packet structure definition.
 */
struct CTXTYPE {};

/**
 * Polycube packet metadata structure definition
 */
struct pkt_metadata {
    bit<16> cube_id;      //__attribute__((deprecated)) // use CUBE_ID instead
    bit<16> in_port;      // The interface on which a packet was received.
    bit<32> packet_len;   //__attribute__((deprecated)) // Use ctx->len

    // used to send data to controller
    bit<16> reason;
}


/**
 * Table management utility functions
 */
extern void TABLE_UPDATE<T, U>(string t, inout T a, inout U b); /* t.update(&a, &b)*/
extern void TABLE_DELETE<T>(string t, inout T a); /* t.delete(&a)*/

/**
 * Utility functions to handle unmanageable translation.
 */
extern void SUBTRACTION<T>(in T result, in T varA, in T varB); /* result = varA - varB*/
extern void EMIT_LOC(string t);

/**
 * Polycube functions to handle packets.
 */
extern void pcn_pkt_redirect(in CTXTYPE skb, in pkt_metadata md, in bit<16> port);
extern void pcn_pkt_drop(in CTXTYPE skb, in pkt_metadata md);
extern void pcn_pkt_controller(in CTXTYPE skb, in pkt_metadata md, bit<16> reason);
extern void pcn_pkt_controller_with_metadata(in CTXTYPE skb, in pkt_metadata md, bit<16> reason,
                                             in bit<32> md0, in bit<32> md1, in bit<32> md2);
extern void pcn_pkt_redirect_ns(in CTXTYPE skb, in pkt_metadata md, in bit<16> port);

/* architectural model for Polycube packet filter target architecture */

parser parse<H>(packet_in packet, out H headers);
control filter<H>(inout H headers, out CTXTYPE ctx, out pkt_metadata md);

package polycubeFilter<H>(parse<H> prs, filter<H> filt);

#endif
