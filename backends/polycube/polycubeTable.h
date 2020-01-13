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

#ifndef _BACKENDS_POLYCUBE_POLYCUBETABLE_H_
#define _BACKENDS_POLYCUBE_POLYCUBETABLE_H_

#include "polycubeObject.h"
#include "polycubeProgram.h"
#include "frontends/p4/methodInstance.h"

namespace POLYCUBE {
// Also used to represent counters
class PolycubeTableBase : public PolycubeObject {
 public:
    const PolycubeProgram* program;

    cstring instanceName;
    cstring keyTypeName;
    cstring valueTypeName;
    cstring dataMapName;
    CodeGenInspector* codeGen;

 protected:
    PolycubeTableBase(const PolycubeProgram* program, cstring instanceName,
                  CodeGenInspector* codeGen) :
            program(program), instanceName(instanceName), codeGen(codeGen) {
        CHECK_NULL(codeGen); CHECK_NULL(program);
        keyTypeName = program->refMap->newName(instanceName + "_key");
        valueTypeName = program->refMap->newName(instanceName + "_value");
        dataMapName = instanceName;
    }
};

class PolycubeTable final : public PolycubeTableBase {
    unsigned upperPowerOfTwo(unsigned width);
 public:
    const IR::Key*            keyGenerator;
    const IR::ActionList*     actionList;
    const IR::TableBlock*    table;
    cstring               defaultActionMapName;
    cstring               actionEnumName;
    std::map<const IR::KeyElement*, cstring> keyFieldNames;
    std::map<const IR::KeyElement*, PolycubeType*> keyTypes;
    cstring keyTypeName;
    cstring valueTypeName;

    PolycubeTable(const PolycubeProgram* program, const IR::TableBlock* table, CodeGenInspector* codeGen);
    void emitTypes(CodeBuilder* builder);
    void emitInstance(CodeBuilder* builder);
    void emitActionArguments(CodeBuilder* builder, const IR::P4Action* action, cstring name);
    void emitKeyType(CodeBuilder* builder);
    void emitValueType(CodeBuilder* builder);
    void emitKey(CodeBuilder* builder, cstring keyName);
    void emitAction(CodeBuilder* builder, cstring valueName);
    void emitInitializer(CodeBuilder* builder);
};
}  // namespace POLYCUBE

#endif /* _BACKENDS_POLYCUBE_POLYCUBETABLE_H_ */
