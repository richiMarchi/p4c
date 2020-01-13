#include <polycube_model.p4>

#define ETH_P_IP 0x0800
#define ETH_P_ARP 0x0806
#define IP_P_ICMP 1
#define CHECK_MAC_DST
#define MAC_MULTICAST_MASK 0x1
#define TYPE_LOCALINTERFACE 1
#define SLOWPATH_ARP_REPLY 1
#define SLOWPATH_ARP_LOOKUP_MISS 2
#define SLOWPATH_TTL_EXCEEDED 3
#define SLOWPATH_PKT_FOR_ROUTER 4
#define ARPOP_REQUEST 1
#define ARPOP_REPLY 2
#define HTONS_ARPOP_REQUEST 256
#define HTONS_ARPOP_REPLY 512
#define HTONS_ETH_P_IP 8
#define HTONS_ETH_P_ARP 1544
#define ARP_TABLE_UPDATE 5
#define IP_CSUM_OFFSET 24

extern bit<16> htons(bit<16> hostshort);
extern bit<16> bpf_htons(bit<16> hostshort);

extern bit<32> bpf_csum_diff(inout bit<32> old, bit<32> four1, inout bit<32> new, bit<32> four2, in bit<32> l3sum);
extern bit<32> bpf_l3_csum_replace(in CTXTYPE skb, bit<32> offset, bit<32> zero1, in bit<32> l3sum, bit<32> zero2);

header eth_hdr {
    bit<48> dst;
    bit<48> src;
    bit<16> proto;
}

header iphdr {
    bit<8>    version;
    bit<8>    tos;
    bit<16>  tot_len;
    bit<16>  id;
    bit<16>  frag_off;
    bit<8>    ttl;
    bit<8>    protocol;
    bit<16> check;
    bit<32>  saddr;
    bit<32>  daddr;
}

extern bit<16> checksum(in iphdr buf, bit<32> bufsz);

header arp_hdr {
  bit<16> ar_hrd;        /* format of hardware address   */
  bit<16> ar_pro;        /* format of protocol address   */
  bit<8> ar_hln; /* length of hardware address   */
  bit<8> ar_pln; /* length of protocol address   */
  bit<16> ar_op;         /* ARP opcode (command)     */
  bit<48> ar_sha;   /* sender hardware address  */
  bit<32> ar_sip;        /* sender IP address        */
  bit<48> ar_tha;   /* target hardware address  */
  bit<32> ar_tip;        /* target IP address        */
}

struct arp_entry {
    bit<48> mac;
    bit<32> port;
}

struct rt_k {
  bit<32> netmask_len;
  bit<32> network;
};

struct rt_v {
  bit<32> port;
  bit<32> nexthop;
  bit<8> type;
};

struct fragmentation {
  bit<16> __unused;
  bit<16> mtu;
};

struct echo_pkt {
  bit<16> id;
  bit<16> sequence;
}

struct union_pkt {
    fragmentation frag;
    echo_pkt echo;
    bit<32> gateway;
}

header icmphdr {
  bit<8> type;                /* message type */
  bit<8> code;                /* type sub-code */
  bit<16> checksum;
  union_pkt un;
};

struct r_port {
  bit<32> ip;
  bit<32> netmask;
  bit<32> secondary_ip;
  bit<32> secondary_netmask;
  bit<48> mac;
};

struct Headers {
    eth_hdr ethernet;
    iphdr ip;
    arp_hdr arp;
    icmphdr icmp;
}

parser Parser(packet_in packet, out Headers hdr) {
    state start {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.proto) {
            ETH_P_IP: parse_ipv4;
            ETH_P_ARP: parse_arp;
            default: reject;
        }
    }

    state parse_ipv4 {
        packet.extract(hdr.ip);
        transition select(hdr.ip.protocol) {
            IP_P_ICMP: parse_icmp;
            default: accept;
        }
    }

    state parse_arp {
        packet.extract(hdr.arp);
        transition accept;
    }

    state parse_icmp {
        packet.extract(hdr.icmp);
        transition accept;
    }
}

control Router(inout Headers hdr, out CTXTYPE ctx, out pkt_metadata md) {

    bit<16> pcn_port;
    bit<32> pcn_meta0;
    bit<32> pcn_meta1;
    bit<32> pcn_meta2;

    bool tableMiss;
    bit<16> router_port_key;
    r_port router_port_value;
    r_port first_router_port;
    rt_k routing_table_key;
    rt_v routing_table_value;
    bit<32> arp_table_key;
    arp_entry arp_table_value;
    bit<32> dst_ip;
    arp_entry toUpdate;
    bit<32> remoteIp;
    bit<48> remoteMac;

    action table_miss () {
        tableMiss = true;
    }

    action router_port_hit(r_port routerporthit)
    {
        router_port_value = routerporthit;
    }

    action routing_table_hit (rt_v value) {
        routing_table_value = value;
    }

    action arp_table_hit(arp_entry arphit) {
        arp_table_value = arphit;
    }

    table router_port {
        key = { router_port_key : exact; }
        actions = {
            router_port_hit;
            table_miss;
        }
        default_action = table_miss;
        implementation = hash_table(32);
    }

    table routing_table {
        key = { routing_table_key : lpm; }
        actions = {
            routing_table_hit;
            table_miss;
        }
        default_action = table_miss;
        implementation = hash_table(256);
    }

    table arp_table {
        key = { arp_table_key : exact; }
        actions = {
            arp_table_hit;
            table_miss;
        }
        default_action = table_miss;
        implementation = hash_table(32);
    }

    apply {
        router_port_key = md.in_port;
        tableMiss = false;
        router_port.apply();
        if (tableMiss)
                pcn_pkt_drop(ctx, md);
        first_router_port = router_port_value;
        EMIT_LOC("#ifdef CHECK_MAC_DST");
            if ((hdr.ethernet.dst != first_router_port.mac) && ((hdr.ethernet.dst & MAC_MULTICAST_MASK) == 0)) {
                pcn_pkt_drop(ctx, md);
            }
        EMIT_LOC("#endif");
        if (hdr.ethernet.proto == htons(ETH_P_IP)) {
            routing_table_key.network = hdr.ip.daddr;
            routing_table_key.netmask_len = 32;
            tableMiss = false;
            routing_table.apply();
            if (tableMiss)
                pcn_pkt_drop(ctx, md);
            if (routing_table_value.type == TYPE_LOCALINTERFACE) {
                EMIT_LOC("#ifdef SHADOW");
                    pcn_port = md.in_port;
                    pcn_pkt_redirect_ns(ctx, md, pcn_port);
                EMIT_LOC("#endif");
                pcn_meta0 = hdr.ip.saddr;
                pcn_meta1 = hdr.ip.daddr;
                pcn_meta2 = (bit<32>)hdr.ip.protocol;
                pcn_pkt_controller_with_metadata(ctx, md, SLOWPATH_PKT_FOR_ROUTER, pcn_meta0, pcn_meta1, pcn_meta2);
            }
            if (hdr.ip.ttl == 1) {
                pcn_meta0 = router_port_value.ip;
                pcn_pkt_controller_with_metadata(ctx, md, SLOWPATH_TTL_EXCEEDED, pcn_meta0, 0, 0);
            }
            router_port_key = (bit<16>)routing_table_value.port;
            tableMiss = false;
            router_port.apply();
            if (tableMiss)
                pcn_pkt_drop(ctx, md);
            if (routing_table_value.nexthop == 0)
                dst_ip = hdr.ip.daddr;
            else
                dst_ip = routing_table_value.nexthop;
            arp_table_key = dst_ip;
            tableMiss = false;
            arp_table.apply();
            if (!tableMiss) {
                hdr.ethernet.dst = arp_table_value.mac;
                hdr.ethernet.src = router_port_value.mac;
                bit<32> new_ttl;
                bit<32> l3sum = 0;
                SUBTRACTION<bit<32>>(new_ttl, (bit<32>)hdr.ip.ttl, 1);
                bit<32> old_ttl = (bit<32>)hdr.ip.ttl;
                EMIT_LOC("#ifdef POLYCUBE_XDP");
                    hdr.ip.check = 0;
                    hdr.ip.ttl = (bit<8>)new_ttl;
                    hdr.ip.check = checksum(hdr.ip, 20);
                EMIT_LOC("#else");
                    l3sum = bpf_csum_diff(old_ttl, 4, new_ttl, 4, l3sum);
                    hdr.ip.ttl = (bit<8>)new_ttl;
                    bpf_l3_csum_replace(ctx, IP_CSUM_OFFSET, 0, l3sum, 0);
                EMIT_LOC("#endif");

                pcn_port = (bit<16>)routing_table_value.port;
                pcn_pkt_redirect(ctx, md, pcn_port);
            } else {
                pcn_meta0 = dst_ip;
                pcn_meta1 = routing_table_value.port;
                pcn_meta2 = router_port_value.ip;
                pcn_pkt_controller_with_metadata(ctx, md, SLOWPATH_ARP_LOOKUP_MISS, pcn_meta0, pcn_meta1, pcn_meta2);
            }
        } else {
            if (hdr.arp.ar_op == bpf_htons(ARPOP_REQUEST)) {
                EMIT_LOC("#ifdef SHADOW");
                    pcn_port = md.in_port;
                    pcn_pkt_redirect_ns(ctx, md, pcn_port);
                EMIT_LOC("#endif");
                // send arp reply
                remoteIp = hdr.arp.ar_sip;
                remoteMac = hdr.arp.ar_sha;
                hdr.arp.ar_op = bpf_htons(ARPOP_REPLY);
                hdr.arp.ar_tha = remoteMac;
                hdr.arp.ar_sha = first_router_port.mac;
                hdr.arp.ar_sip = first_router_port.ip;
                hdr.arp.ar_tip = remoteIp;
                hdr.ethernet.dst = remoteMac;
                hdr.ethernet.src = first_router_port.mac;
                // update arp table
                toUpdate.mac = remoteMac;
                toUpdate.port = (bit<32>)md.in_port;
                TABLE_UPDATE<bit<32>, arp_entry>("arp_table", remoteIp, toUpdate);

                pcn_port = md.in_port;
                pcn_pkt_redirect(ctx, md, pcn_port);
            } else if (hdr.arp.ar_op == bpf_htons(ARPOP_REPLY)) {
                // norify arp reply to slowpath
                remoteIp = hdr.arp.ar_sip;
                remoteMac = hdr.arp.ar_sha;
                toUpdate.mac = remoteMac;
                toUpdate.port = (bit<32>)md.in_port;
                TABLE_UPDATE<bit<32>, arp_entry>("arp_table", remoteIp, toUpdate);

                pcn_meta0 = hdr.arp.ar_sip;
                pcn_pkt_controller_with_metadata(ctx, md, SLOWPATH_ARP_REPLY, pcn_meta0, 0, 0);
            } else {
                pcn_pkt_drop(ctx, md);
            }
        }
    }
}

polycubeFilter(
Parser(),
Router()
) main;
