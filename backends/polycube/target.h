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

#ifndef _BACKENDS_POLYCUBE_TARGET_H_
#define _BACKENDS_POLYCUBE_TARGET_H_

#include "lib/cstring.h"
#include "lib/sourceCodeBuilder.h"
#include "lib/exceptions.h"

// We are prepared to support code generation using multiple styles
// (e.g., using BCC or using CLANG).

namespace POLYCUBE {

enum TableKind {
    TableHash,
    TableArray,
    TableLPMTrie  // longest prefix match trie
};

class Target {
 public:
    explicit Target(cstring name = "Linux kernel") : name(name) {}
    virtual ~Target() = default;
    const cstring name;
    void emitCodeSection(Util::SourceCodeBuilder* builder, cstring sectionName) const;
    void emitIncludes(Util::SourceCodeBuilder* builder) const;
    void emitTableLookup(Util::SourceCodeBuilder* builder, cstring tblName,
                         cstring key, cstring value) const;
    void emitTableUpdate(Util::SourceCodeBuilder* builder, cstring tblName,
                         cstring key, cstring value) const;
    void emitUserTableUpdate(Util::SourceCodeBuilder* builder, cstring tblName,
                             cstring key, cstring value) const;
    void emitTableDecl(Util::SourceCodeBuilder* builder,
                       cstring tblName, TableKind tableKind,
                       cstring keyType, cstring valueType, unsigned size) const;
    void emitMain(Util::SourceCodeBuilder* builder,
                  cstring functionName,
                  cstring argName) const;
    cstring dataOffset(cstring base) const
    { return cstring("((void*)(long)")+ base + "->data)"; }
    cstring dataEnd(cstring base) const
    { return cstring("((void*)(long)")+ base + "->data_end)"; }
    cstring abortReturnCode() const { return "RX_ERROR"; }
    cstring sysMapPath() const { return "/sys/fs/bpf/tc/globals"; }
};

}  // namespace POLYCUBE

#endif /* _BACKENDS_POLYCUBE_TARGET_H_ */
