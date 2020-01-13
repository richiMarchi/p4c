#include <polycube_model.p4>

#define REASON_FLOODING 0x01
#define FDB_TIMEOUT 300

struct fwd_entry {
  bit<32> timestamp;
  bit<32> port;
}

header eth_hdr {
    bit<48> destination;
    bit<48> source;
    bit<16> protocol;
}

struct Headers {
    eth_hdr ethernet;
}

parser Parser(packet_in packet, out Headers hdr) {
    state start {
        packet.extract(hdr.ethernet);
        transition accept;
    }
}

control Simplebridge(inout Headers hdr, out CTXTYPE ctx, out pkt_metadata md) {

    bool tableMiss;
    bit<32> sysTimestamp = 0;
    bit<32> timestamp_key = 0;
    bit<64> fwdtable_key;
    fwd_entry fwdtable_value;
    fwd_entry toUpdate;
    bit<32> timeInterval;

    action timestamp_hit(bit<32> ts) {
        sysTimestamp = ts;
    }

    action table_miss() {
        // never happens
        tableMiss = true;
    }

    action fwdtable_hit(fwd_entry fwd_hit)
    {
        fwdtable_value = fwd_hit;
    }

    table fwdtable {
        key = { fwdtable_key : exact; }
        actions = {
            fwdtable_hit;
            table_miss;
        }
        default_action = table_miss;
        implementation = hash_table(1024);
    }

    table timestamp {
        key = { timestamp_key : exact; }
        actions = {
            timestamp_hit;
            table_miss;
        }
        default_action = table_miss;
        implementation = array_table(1);
    }

    apply {
        tableMiss = false;
        timestamp.apply();
        if (tableMiss) {
            pcn_pkt_drop(ctx, md);
        }
        fwdtable_key = ((bit<64>)hdr.ethernet.source);
        toUpdate.timestamp = sysTimestamp;
        toUpdate.port = (bit<32>)md.in_port;
        TABLE_UPDATE<bit<64>, fwd_entry>("fwdtable", fwdtable_key, toUpdate);

        fwdtable_key = ((bit<64>)hdr.ethernet.destination);
        tableMiss = false;
        fwdtable.apply();

        if (tableMiss) {
            pcn_pkt_controller(ctx, md, REASON_FLOODING);
        }

        SUBTRACTION<bit<32>>(timeInterval, sysTimestamp, fwdtable_value.timestamp);
        if (timeInterval > FDB_TIMEOUT) {
            TABLE_DELETE<bit<64>>("fwdtable", fwdtable_key);
            pcn_pkt_controller(ctx, md, REASON_FLOODING);
        }

        if (((bit<32>)md.in_port) == fwdtable_value.port){
            pcn_pkt_drop(ctx, md);
        }
        pcn_pkt_redirect(ctx, md, (bit<16>)fwdtable_value.port);
    }
}

polycubeFilter(
Parser(),
Simplebridge()
) main;