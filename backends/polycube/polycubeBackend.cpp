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

#include "lib/error.h"
#include "lib/nullstream.h"
#include "frontends/p4/evaluator/evaluator.h"

#include "polycubeBackend.h"
#include "target.h"
#include "polycubeType.h"
#include "polycubeProgram.h"

namespace POLYCUBE {

void run_polycube_backend(const PolycubeOptions& options, const IR::ToplevelBlock* toplevel,
                      P4::ReferenceMap* refMap, P4::TypeMap* typeMap) {
    if (toplevel == nullptr)
        return;

    auto main = toplevel->getMain();
    if (main == nullptr) {
        ::warning(ErrorType::WARN_MISSING,
                  "Could not locate top-level block; is there a %1% module?",
                  IR::P4Program::main);
        return;
    }

    Target* target  = new Target();

    CodeBuilder c(target);
    CodeBuilder h(target);

    PolycubeTypeFactory::createFactory(typeMap);
    auto polycubeprog = new PolycubeProgram(options, toplevel->getProgram(), refMap, typeMap, toplevel);
    if (!polycubeprog->build())
        return;

    if (options.outputFile.isNullOrEmpty())
        return;

    cstring cfile = options.outputFile;
    auto cstream = openFile(cfile, false);
    if (cstream == nullptr)
        return;

    cstring hfile;
    const char* dot = cfile.findlast('.');
    if (dot == nullptr)
        hfile = cfile + ".h";
    else
        hfile = cfile.before(dot) + ".h";

    polycubeprog->emitH(&h, hfile);
    polycubeprog->emitC(&c);
    *cstream << c.toString();
    cstream->flush();
}

}  // namespace POLYCUBE
