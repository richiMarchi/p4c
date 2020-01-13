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

#include "polycubeParser.h"
#include "polycubeType.h"
#include "frontends/p4/coreLibrary.h"
#include "frontends/p4/methodInstance.h"

namespace POLYCUBE {

namespace {
class StateTranslationVisitor : public CodeGenInspector {
    bool hasDefault;
    P4::P4CoreLibrary& p4lib;
    PolycubeParserState* state;

    void compileExtract(const IR::Expression* destination);
    void compileLookahead(const IR::Expression* destination);

 public:
    explicit StateTranslationVisitor(PolycubeParserState* state) :
            CodeGenInspector(state->parser->program->refMap, state->parser->program->typeMap),
            hasDefault(false), p4lib(P4::P4CoreLibrary::instance), state(state) {}
    bool preorder(const IR::ParserState* state) override;
    bool preorder(const IR::SelectCase* selectCase) override;
    bool preorder(const IR::SelectExpression* expression) override;
    bool preorder(const IR::Member* expression) override;
    bool preorder(const IR::MethodCallExpression* expression) override;
    bool preorder(const IR::MethodCallStatement* stat) override
    { visit(stat->methodCall); return false; }
    bool preorder(const IR::AssignmentStatement* stat) override;
};
}  // namespace

void
StateTranslationVisitor::compileLookahead(const IR::Expression* destination) {
    builder->emitIndent();
    builder->blockStart();
    builder->emitIndent();
    builder->appendFormat("%s_save = %s",
                          state->parser->program->offsetVar.c_str(),
                          state->parser->program->offsetVar.c_str());
    builder->endOfStatement(true);
    compileExtract(destination);
    builder->emitIndent();
    builder->appendFormat("%s = %s_save",
                          state->parser->program->offsetVar.c_str(),
                          state->parser->program->offsetVar.c_str());
    builder->endOfStatement(true);
    builder->blockEnd(true);
}

bool StateTranslationVisitor::preorder(const IR::AssignmentStatement* statement) {
    if (auto mce = statement->right->to<IR::MethodCallExpression>()) {
        auto mi = P4::MethodInstance::resolve(mce,
                                              state->parser->program->refMap,
                                              state->parser->program->typeMap);
        auto extMethod = mi->to<P4::ExternMethod>();
        if (extMethod == nullptr)
            BUG("Unhandled method %1%", mce);

        auto decl = extMethod->object;
        if (decl == state->parser->packet) {
            if (extMethod->method->name.name == p4lib.packetIn.lookahead.name) {
                compileLookahead(statement->left);
                return false;
            }
        }
        ::error("Unexpected method call in parser %1%", statement->right);
    }

    CodeGenInspector::visit(statement);
    return false;
}

bool StateTranslationVisitor::preorder(const IR::ParserState* parserState) {
    if (parserState->isBuiltin()) return false;

    builder->emitIndent();
    builder->append(parserState->name.name);
    builder->append(":");
    builder->spc();
    builder->blockStart();
    builder->emitIndent();

    visit(parserState->components, "components");
    if (parserState->selectExpression == nullptr) {
        builder->emitIndent();
        builder->append("goto ");
        builder->append(IR::ParserState::reject);
        builder->endOfStatement(true);
    } else if (parserState->selectExpression->is<IR::SelectExpression>()) {
        visit(parserState->selectExpression);
    } else {
        // must be a PathExpression which is a state name
        if (!parserState->selectExpression->is<IR::PathExpression>())
            BUG("Expected a PathExpression, got a %1%", parserState->selectExpression);
        builder->emitIndent();
        builder->append("goto ");
        visit(parserState->selectExpression);
        builder->endOfStatement(true);
    }

    builder->blockEnd(true);
    return false;
}

bool StateTranslationVisitor::preorder(const IR::SelectExpression* expression) {
    hasDefault = false;
    if (expression->select->components.size() != 1) {
        // TODO: this does not handle correctly tuples
        ::error("%1%: only supporting a single argument for select", expression->select);
        return false;
    }
    builder->emitIndent();
    builder->append("switch (");
    std::string elementType(expression->select->components[0]->type->toString());
    std::string oldString(expression->select->components[0]->toString());
    unsigned lastPoint = oldString.find(".", state->parser->headers->name.toString().size() + 1);
    std::string newSelectComponent = oldString.substr(0, lastPoint);
    newSelectComponent.append("->");
    newSelectComponent.append(oldString.substr(lastPoint + 1, oldString.size() - lastPoint - 1));
    builder->append(newSelectComponent);
    builder->append(") ");
    builder->blockStart();
    builder->emitIndent();

    bool first = true;
    for (auto e : expression->selectCases) {
        std::list<cstring> newList(state->parser->previousStates.at(state->state->name.toString()));
        newList.push_back(state->state->name.toString());
        state->parser->previousStates.emplace(e->state->toString(), newList);
        if (first) {
            first = false;
        } else {
            builder->emitIndent();
        }
        visit(e);
    }

    if (!hasDefault) {
        builder->emitIndent();
        builder->appendFormat("default: goto %s;", IR::ParserState::reject.c_str());
        builder->newline();
    }

    builder->blockEnd(true);
    return false;
}

bool StateTranslationVisitor::preorder(const IR::SelectCase* selectCase) {
    if (selectCase->keyset->is<IR::DefaultExpression>()) {
        hasDefault = true;
        builder->append("default: ");
    } else {
        builder->append("case ");
        visit(selectCase->keyset);
        builder->append(": ");
    }
    builder->append("goto ");
    visit(selectCase->state);
    builder->endOfStatement(true);
    return false;
}

void
StateTranslationVisitor::compileExtract(const IR::Expression* destination) {
    auto type = state->parser->typeMap->getType(destination);
    auto ht = type->to<IR::Type_StructLike>();
    if (ht == nullptr) {
        ::error("Cannot extract to a non-struct type %1%", destination);
        return;
    }
    //unsigned width = ht->width_bits();
    auto program = state->parser->program;
    builder->emitIndent();
    std::string headerType(destination->type->toString());
    builder->appendFormat("%s = %s", destination->toString(),
                          program->packetStartVar);

    std::string toAppend;
    if (!state->parser->stateToHeader.empty()) {
        std::list<cstring>::iterator it;
        for (it = state->parser->previousStates.at(state->state->name.toString()).begin();
                it != state->parser->previousStates.at(state->state->name.toString()).end();
                it++) {
            toAppend.append(" + sizeof(*(" + state->parser->stateToHeader.at(*it) + "))");
        }
    }

    if (state->parser->stateToHeader.empty()) {
        state->parser->previousStates.emplace(state->state->name.toString(), std::list<cstring>());
    }
    state->parser->stateToHeader.emplace(state->state->name.toString(), destination->toString());

    builder->append(toAppend);
    builder->endOfStatement(true);

    builder->emitIndent();
    builder->appendFormat("if (%s", program->packetStartVar.c_str());
    builder->append(toAppend);
    builder->appendFormat(" + sizeof(*(%s))", destination->toString());
    builder->appendFormat(" > %s) ", program->packetEndVar.c_str());

    builder->blockStart();
    builder->emitIndent();
    builder->appendFormat("goto %s;", IR::ParserState::reject.c_str());
    builder->newline();
    builder->blockEnd(true);

    unsigned alignment = 0;
    for (auto f : ht->fields) {
        auto ftype = state->parser->typeMap->getType(f);
        auto etype = PolycubeTypeFactory::instance->create(ftype);
        auto et = dynamic_cast<IHasWidth*>(etype);
        if (et == nullptr) {
            ::error("Only headers with fixed widths supported %1%", f);
            return;
        }
        alignment += et->widthInBits();
        alignment %= 8;
    }
}

bool StateTranslationVisitor::preorder(const IR::MethodCallExpression* expression) {
    builder->append("/* ");
    visit(expression->method);
    builder->append("(");
    bool first = true;
    for (auto a  : *expression->arguments) {
        if (!first)
            builder->append(", ");
        first = false;
        visit(a);
    }
    builder->append(")");
    builder->append("*/");
    builder->newline();

    auto mi = P4::MethodInstance::resolve(expression,
                                          state->parser->program->refMap,
                                          state->parser->program->typeMap);
    auto extMethod = mi->to<P4::ExternMethod>();
    if (extMethod != nullptr) {
        auto decl = extMethod->object;
        if (decl == state->parser->packet) {
            if (extMethod->method->name.name == p4lib.packetIn.extract.name) {
                if (expression->arguments->size() != 1) {
                    ::error("Variable-sized header fields not yet supported %1%", expression);
                    return false;
                }
                compileExtract(expression->arguments->at(0)->expression);
                return false;
            }
            BUG("Unhandled packet method %1%", expression->method);
            return false;
        }
    }

    ::error("Unexpected method call in parser %1%", expression);
    return false;
}

bool StateTranslationVisitor::preorder(const IR::Member* expression) {
    if (expression->expr->is<IR::PathExpression>()) {
        auto pe = expression->expr->to<IR::PathExpression>();
        auto decl = state->parser->program->refMap->getDeclaration(pe->path, true);
        if (decl == state->parser->packet) {
            builder->append(expression->member);
            return false;
        }
    }

    visit(expression->expr);
    builder->append(".");
    builder->append(expression->member);
    return false;
}

//////////////////////////////////////////////////////////////////

void PolycubeParserState::emit(CodeBuilder* builder) {
    StateTranslationVisitor visitor(this);
    visitor.setBuilder(builder);
    state->apply(visitor);
}

PolycubeParser::PolycubeParser(PolycubeProgram* program, const IR::ParserBlock* block,
                       const P4::TypeMap* typeMap) :
        program(program), typeMap(typeMap), parserBlock(block),
        packet(nullptr), headers(nullptr), headerType(nullptr) {}

void PolycubeParser::emitDeclaration(CodeBuilder* builder, const IR::Declaration* decl) {
    if (decl->is<IR::Declaration_Variable>()) {
        auto vd = decl->to<IR::Declaration_Variable>();
        auto etype = PolycubeTypeFactory::instance->create(vd->type);
        builder->emitIndent();
        etype->declare(builder, vd->name, false);
        builder->endOfStatement(true);
        BUG_CHECK(vd->initializer == nullptr,
                  "%1%: declarations with initializers not supported", decl);
        return;
    }
    BUG("%1%: not yet handled", decl);
}


void PolycubeParser::emit(CodeBuilder* builder) {
    for (auto l : parserBlock->container->parserLocals)
        emitDeclaration(builder, l);
    for (auto s : states)
        s->emit(builder);
    builder->newline();

    // Create a synthetic reject state
    builder->emitIndent();
    builder->appendFormat("%s: { return %s; }",
                          IR::ParserState::reject.c_str(),
                          builder->target->abortReturnCode().c_str());
    builder->newline();
    builder->newline();
}

bool PolycubeParser::build() {
    auto pl = parserBlock->container->type->applyParams;
    if (pl->size() != 2) {
        ::error("Expected parser to have exactly 2 parameters");
        return false;
    }

    auto it = pl->parameters.begin();
    packet = *it; ++it;
    headers = *it;
    for (auto state : parserBlock->container->states) {
        auto ps = new PolycubeParserState(state, this);
        states.push_back(ps);
    }

    auto ht = typeMap->getType(headers);
    if (ht == nullptr)
        return false;
    headerType = PolycubeTypeFactory::instance->create(ht);
    return true;
}

}  // namespace POLYCUBE
