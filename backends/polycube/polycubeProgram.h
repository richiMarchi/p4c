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

#ifndef _BACKENDS_POLYCUBE_POLYCUBEPROGRAM_H_
#define _BACKENDS_POLYCUBE_POLYCUBEPROGRAM_H_

#include "target.h"
#include "polycubeModel.h"
#include "polycubeObject.h"
#include "polycubeOptions.h"
#include "ir/ir.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/evaluator/evaluator.h"
#include "frontends/common/options.h"
#include "codeGen.h"

namespace POLYCUBE {

class PolycubeProgram;
class PolycubeParser;
class PolycubeControl;
class PolycubeTable;
class PolycubeType;

class PolycubeProgram : public PolycubeObject {
 public:
    const PolycubeOptions& options;
    const IR::P4Program* program;
    const IR::ToplevelBlock*  toplevel;
    P4::ReferenceMap*    refMap;
    P4::TypeMap*         typeMap;
    PolycubeParser*      parser;
    PolycubeControl*     control;
    PolycubeModel        &model;

    cstring endLabel, offsetVar;
    cstring zeroKey, functionName, errorVar;
    cstring packetStartVar, packetEndVar, byteVar;
    cstring license = "GPL";  // TODO: this should be a compiler option probably
    cstring arrayIndexType = "u32";

    virtual bool build();  // return 'true' on success

    PolycubeProgram(const PolycubeOptions &options, const IR::P4Program* program,
                P4::ReferenceMap* refMap, P4::TypeMap* typeMap, const IR::ToplevelBlock* toplevel) :
            options(options), program(program), toplevel(toplevel),
            refMap(refMap), typeMap(typeMap),
            parser(nullptr), control(nullptr), model(PolycubeModel::instance) {
        offsetVar = PolycubeModel::reserved("packetOffsetInBits");
        zeroKey = PolycubeModel::reserved("zero");
        functionName = PolycubeModel::reserved("filter");
        errorVar = PolycubeModel::reserved("errorCode");
        packetStartVar = PolycubeModel::reserved("packetStart");
        packetEndVar = PolycubeModel::reserved("packetEnd");
        byteVar = PolycubeModel::reserved("byte");
        endLabel = PolycubeModel::reserved("end");
    }

 protected:
    virtual void emitGeneratedComment(CodeBuilder* builder);
    virtual void emitPreamble(CodeBuilder* builder);
    virtual void emitTypes(CodeBuilder* builder);
    virtual void emitHeaderInstances(CodeBuilder* builder);
    virtual void emitLocalVariables(CodeBuilder* builder);
    virtual void emitPipeline(CodeBuilder* builder);

 public:
    virtual void emitH(CodeBuilder* builder, cstring headerFile);  // emits C headers
    virtual void emitC(CodeBuilder* builder);  // emits C program
};

}  // namespace POLYCUBE

#endif /* _BACKENDS_POLYCUBE_POLYCUBEPROGRAM_H_ */
