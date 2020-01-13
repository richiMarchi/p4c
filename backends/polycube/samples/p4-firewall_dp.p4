#include <polycube_model.p4>

#define TYPE_IPV4 0x800
#define TYPE_TCP 6
#define IP_CSUM_OFFSET 24
#define TH_SYN 0x02
#define REASON_FLOODING 1

header ethernet {
    bit<48> dstAddr;
    bit<48> srcAddr;
    bit<16> etherType;
}

header ipv4 {
    bit<4>    version;
    bit<4>    ihl;
    bit<8>    diffserv;
    bit<16>   totalLen;
    bit<16>   identification;
    bit<3>    flags;
    bit<13>   fragOffset;
    bit<8>    ttl;
    bit<8>    protocol;
    bit<16>   hdrChecksum;
    bit<32>   srcAddr;
    bit<32>   dstAddr;
}

header tcp {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4>  dataOffset;
    bit<4>  res;
    bit<8>  flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

struct cp_k {
    bit<32> ingress_port;
    bit<32> egress_spec;
}

struct ipv4_fw_v {
    bit<48> dstAddr;
    bit<32> port;
}

struct ipv4_fw_k {
    bit<32> netmask;
    bit<32> address;
}

struct headers {
    ethernet   ethernet;
    ipv4       ipv4;
    tcp        tcp;
}

struct st_k {
  bit<32> srcIp;
  bit<32> dstIp;
  bit<32> srcPort;
  bit<32> dstPort;
}

extern bit<16> htons(bit<16> hostshort);
extern bit<32> bpf_csum_diff(inout bit<32> old, bit<32> four1, inout bit<32> new, bit<32> four2, in bit<32> l3sum);
extern bit<32> bpf_l3_csum_replace(in CTXTYPE skb, bit<32> offset, bit<32> zero1, in bit<32> l3sum, bit<32> zero2);

parser Parser(packet_in packet, out headers hdr) {
    state start {
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            TYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol){
            TYPE_TCP: parse_tcp;
            default: accept;
        }
    }

    state parse_tcp {
       packet.extract(hdr.tcp);
       transition accept;
    }
}

control Firewall(inout headers hdr, out CTXTYPE ctx, out pkt_metadata md) {

    bit<1> internalNetwork;
    bool miss;
    cp_k check_ports_key;
    ipv4_fw_v forward_value;
    bit<16> out_port;
    st_k sessions_key;
    bit<1> isSessionSet;
    ipv4_fw_k forward_key;

    action table_miss() {
        miss = true;
    }

    action ipv4_forward(ipv4_fw_v fw_value) {
        forward_value = fw_value;
    }
    
    table ipv4_lpm {
        key = {
            forward_key: lpm;
        }
        actions = {
            ipv4_forward;
            table_miss;
        }
        default_action = table_miss();
        implementation = hash_table(1024);
    }

    action set_direction(bit<1> direction) {
        internalNetwork = direction;
    }

    table check_ports {
        key = {
            check_ports_key: exact;
        }
        actions = {
            set_direction;
            table_miss;
        }
        default_action = table_miss();
        implementation = hash_table(1024);
    }

    action sessions_hit(bit<1> isSet) {
        isSessionSet = isSet;
    }

    table sessions {
        key = {
            sessions_key: exact;
        }
        actions = {
            sessions_hit;
            table_miss;
        }
        default_action = table_miss();
        implementation = hash_table(65536);
    }
    
    apply {
        if (hdr.ethernet.etherType != htons(TYPE_IPV4))
            pcn_pkt_controller(ctx, md, REASON_FLOODING);
        miss = false;
        forward_key.address = hdr.ipv4.dstAddr;
        forward_key.netmask = 32;
        ipv4_lpm.apply();
        if (miss)
            pcn_pkt_controller(ctx, md, REASON_FLOODING);
        out_port = (bit<16>)forward_value.port;
        hdr.ethernet.srcAddr = hdr.ethernet.dstAddr;
        hdr.ethernet.dstAddr = forward_value.dstAddr;
        internalNetwork = 0;
        miss = false;
        check_ports_key.ingress_port = (bit<32>)md.in_port;
        check_ports_key.egress_spec = (bit<32>)out_port;
        check_ports.apply();
        if (!miss && (hdr.ipv4.protocol == TYPE_TCP)) {
            if (internalNetwork == 0){
                sessions_key.srcIp = hdr.ipv4.srcAddr; 
                sessions_key.dstIp = hdr.ipv4.dstAddr;
                sessions_key.srcPort = (bit<32>)hdr.tcp.srcPort;
                sessions_key.dstPort = (bit<32>)hdr.tcp.dstPort;
                isSessionSet = 1;
                if ((hdr.tcp.flags & TH_SYN) != 0){
                    TABLE_UPDATE<st_k, bit<1>>("sessions", sessions_key, isSessionSet);
                }
            } else {
                sessions_key.srcIp = hdr.ipv4.dstAddr; 
                sessions_key.dstIp = hdr.ipv4.srcAddr;
                sessions_key.srcPort = (bit<32>)hdr.tcp.dstPort;
                sessions_key.dstPort = (bit<32>)hdr.tcp.srcPort;
                // Read bloom filter cells to check if there are 1's
                isSessionSet = 0;
                sessions.apply();
                // only allow flow to pass if both entries are set
                if (isSessionSet != 1){
                    pcn_pkt_drop(ctx, md);
                }
            }   
        }
        bit<32> new_ttl;
        bit<32> l3sum = 0;
        SUBTRACTION<bit<32>>(new_ttl, (bit<32>)hdr.ipv4.ttl, 1);
        bit<32> old_ttl = (bit<32>)hdr.ipv4.ttl;
        l3sum = bpf_csum_diff(old_ttl, 4, new_ttl, 4, l3sum);
        hdr.ipv4.ttl = (bit<8>)new_ttl;
        bpf_l3_csum_replace(ctx, IP_CSUM_OFFSET, 0, l3sum, 0);
        pcn_pkt_redirect(ctx, md, out_port);
    }
}

polycubeFilter(
Parser(),
Firewall()
) main;
