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

#include "target.h"

namespace POLYCUBE {


void Target::emitIncludes(Util::SourceCodeBuilder* builder) const {
    builder->append("#include <uapi/linux/bpf.h>\n"
                    "#include <uapi/linux/filter.h>\n"
                    "#include <uapi/linux/if_ether.h>\n"
                    "#include <uapi/linux/if_packet.h>\n"
                    "#include <uapi/linux/in.h>\n"
                    "#include <uapi/linux/ip.h>\n"
                    "#include <uapi/linux/pkt_cls.h>");
    builder->newline();
}

void Target::emitTableLookup(Util::SourceCodeBuilder* builder, cstring tblName,
                                          cstring key, cstring value) const {
    builder->appendFormat("%s = %s.lookup(&%s)",
                          value.c_str(), tblName.c_str(), key.c_str());
}

void Target::emitTableUpdate(Util::SourceCodeBuilder* builder, cstring tblName,
                                          cstring key, cstring value) const {
    builder->appendFormat("%s.update(&%s, &%s);",
                          tblName.c_str(), key.c_str(), value.c_str());
}

void Target::emitUserTableUpdate(Util::SourceCodeBuilder* builder, cstring tblName,
                                          cstring key, cstring value) const {
    builder->appendFormat("BPF_USER_MAP_UPDATE_ELEM(%s, &%s, &%s, BPF_ANY);",
                          tblName.c_str(), key.c_str(), value.c_str());
}

void Target::emitTableDecl(Util::SourceCodeBuilder* builder,
                                        cstring tblName, TableKind tableKind,
                                        cstring keyType, cstring valueType,
                                        unsigned size) const {
    cstring kind;
    if (tableKind == TableHash) {
        kind = "hash";
        builder->appendFormat("BPF_TABLE(\"%s\", %s, ", kind.c_str(), keyType.c_str());
        builder->appendFormat("%s, %s, %d);",
                              valueType.c_str(), tblName.c_str(), size);
    } else if (tableKind == TableArray) {
        kind = "array";
        builder->appendFormat("BPF_TABLE(\"%s\", %s, ", kind.c_str(), keyType.c_str());
        builder->appendFormat("%s, %s, %d);",
                              valueType.c_str(), tblName.c_str(), size);
    } else if (tableKind == TableLPMTrie) {
        kind = "lpm_trie";
        builder->appendFormat("BPF_F_TABLE(\"%s\", %s, ", kind.c_str(), keyType.c_str());
        builder->appendFormat("%s, %s, %d, BPF_F_NO_PREALLOC);",
                              valueType.c_str(), tblName.c_str(), size);
    } else
        BUG("%1%: unsupported table kind", tableKind);
    builder->newline();
}

void Target::emitCodeSection(
    Util::SourceCodeBuilder* builder, cstring sectionName) const {
    builder->lastIsSpace();
    sectionName.isNull();
}

void Target::emitMain(Util::SourceCodeBuilder* builder,
                                   cstring functionName,
                                   cstring argName) const {
    functionName.isNull();
    argName.isNull();
    builder->appendFormat("static int handle_rx(struct CTXTYPE *ctx, struct pkt_metadata *md)");
}

}  // namespace POLYCUBE
