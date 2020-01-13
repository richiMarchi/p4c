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

#include "polycubeControl.h"
#include "polycubeType.h"
#include "polycubeTable.h"
#include "lib/error.h"
#include "frontends/p4/tableApply.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/methodInstance.h"
#include "frontends/p4/parameterSubstitution.h"

namespace POLYCUBE {

ControlBodyTranslator::ControlBodyTranslator(const PolycubeControl* control) :
        CodeGenInspector(control->program->refMap, control->program->typeMap), control(control),
        p4lib(P4::P4CoreLibrary::instance)
{ setName("ControlBodyTranslator"); }

bool ControlBodyTranslator::preorder(const IR::PathExpression* expression) {
    auto decl = control->program->refMap->getDeclaration(expression->path, true);
    auto param = decl->getNode()->to<IR::Parameter>();
    if (param != nullptr) {
        if (toDereference.count(param) > 0)
            builder->append("*");
        auto subst = ::get(substitution, param);
        if (subst != nullptr) {
            builder->append(subst->name);
            return false;
        }
    }
    builder->append(expression->path->name);  // each identifier should be unique
    return false;
}

void ControlBodyTranslator::processCustomExternFunction(const P4::ExternFunction* function) {
    if (!control->emitExterns)
        ::error("%1%: Not supported", function->method);

    builder->emitIndent();
    if (function->expr->method->toString() == "TABLE_UPDATE"
            || function->expr->method->toString() == "TABLE_DELETE") {
        auto parameters = function->substitution.getParametersInArgumentOrder();
        auto tableName = function->substitution.lookup(parameters->next())->toString();
        tableName = tableName.substr(1, tableName.size() - 2);
        auto keyName = function->substitution.lookup(parameters->next());
        if (function->expr->method->toString() == "TABLE_UPDATE") {
            auto valueName = function->substitution.lookup(parameters->next());
            builder->appendFormat("%s.update(&", tableName);
            visit(keyName);
            builder->append(", &");
            visit(valueName);
            builder->append(")");
        } else {
            builder->appendFormat("%s.delete(&", tableName);
            visit(keyName);
            builder->append(")");
        }
    } else if (function->expr->method->toString() == "SUBTRACTION") {
        auto parameters = function->substitution.getParametersInArgumentOrder();
        auto result = function->substitution.lookup(parameters->next());
        auto arg1 = function->substitution.lookup(parameters->next());
        auto arg2 = function->substitution.lookup(parameters->next());
        visit(result);
        builder->append(" = ");
        visit(arg1);
        builder->append(" - ");
        visit(arg2);
    } else if (function->expr->method->toString() == "EMIT_LOC") {
        auto parameters = function->substitution.getParametersInArgumentOrder();
        auto loc = function->substitution.lookup(parameters->next())->toString();
        std::string output;
        for(size_t i = 1; i < loc.size() - 1; ++i)
            if(loc[i] != '\\') output += loc[i];
        builder->appendLine(output);
    } else if (function->expr->method->toString() == "pcn_pkt_controller_with_metadata") {
        auto parameters = function->substitution.getParametersInArgumentOrder();
        auto arg1 = function->substitution.lookup(parameters->next());
        auto arg2 = function->substitution.lookup(parameters->next());
        auto arg3 = function->substitution.lookup(parameters->next());
        auto arg4 = function->substitution.lookup(parameters->next());
        auto arg5 = function->substitution.lookup(parameters->next());
        auto arg6 = function->substitution.lookup(parameters->next());
        builder->blockStart();
        builder->emitIndent();
        builder->append("u32 tmp[3] = {");
        visit(arg4);
        builder->append(", ");
        visit(arg5);
        builder->append(", ");
        visit(arg6);
        builder->appendFormat("};\n");
        builder->emitIndent();
        builder->appendFormat("return %s(", function->expr->method->toString());
        visit(arg1);
        builder->append(", ");
        visit(arg2);
        builder->append(", ");
        visit(arg3);
        builder->appendFormat(", tmp);\n");
        builder->decreaseIndent();
        builder->emitIndent();
        builder->appendFormat("}\n");
    } else if (function->expr->method->toString() == "pcn_pkt_drop") {
        builder->append("return RX_DROP");
    } else {
        if (function->expr->method->toString() == "pcn_pkt_redirect"
            || function->expr->method->toString() == "pcn_pkt_controller"
            || function->expr->method->toString() == "pcn_pkt_redirect_ns") {
            builder->append("return ");
        }
        visit(function->expr->method);
        builder->append("(");
        bool first = true;

        for (auto p : *function->substitution.getParametersInArgumentOrder()) {
            if (!first)
                builder->append(", ");
            first = false;
            auto arg = function->substitution.lookup(p);
            if (p->direction == IR::Direction::Out || p->direction == IR::Direction::InOut) {
                builder->append("&");
            }
            visit(arg);
        }

        builder->append(")");
    }
}

void ControlBodyTranslator::processFunction(const P4::ExternFunction* function) {
    if (!control->emitExterns)
        ::error("%1%: Not supported", function->method);
    processCustomExternFunction(function);
}

bool ControlBodyTranslator::preorder(const IR::MethodCallExpression* expression) {
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
                                          control->program->refMap,
                                          control->program->typeMap);
    auto apply = mi->to<P4::ApplyMethod>();
    if (apply != nullptr) {
        processApply(apply);
        return false;
    }
    auto ef = mi->to<P4::ExternFunction>();
    if (ef != nullptr) {
        processFunction(ef);
        return false;
    }
    auto bim = mi->to<P4::BuiltInMethod>();
    if (bim != nullptr) {
        builder->emitIndent();
        if (bim->name == IR::Type_Header::isValid) {
            visit(bim->appliedTo);
            builder->append(".polycube_valid");
            return false;
        } else if (bim->name == IR::Type_Header::setValid) {
            visit(bim->appliedTo);
            builder->append(".polycube_valid = true");
            return false;
        } else if (bim->name == IR::Type_Header::setInvalid) {
            visit(bim->appliedTo);
            builder->append(".polycube_valid = false");
            return false;
        }
    }
    auto ac = mi->to<P4::ActionCall>();
    if (ac != nullptr) {
        // Action arguments have been eliminated by the mid-end.
        BUG_CHECK(expression->arguments->size() == 0,
                  "%1%: unexpected arguments for action call", expression);
        visit(ac->action->body);
        return false;
    }

    ::error("Unsupported method invocation %1%", expression);
    return false;
}

void ControlBodyTranslator::processApply(const P4::ApplyMethod* method) {
    builder->emitIndent();
    auto table = control->getTable(method->object->getName().name);
    BUG_CHECK(table != nullptr, "No table for %1%", method->expr);

    P4::ParameterSubstitution binding;
    cstring actionVariableName;
    if (!saveAction.empty()) {
        actionVariableName = saveAction.at(saveAction.size() - 1);
        if (!actionVariableName.isNullOrEmpty()) {
            builder->appendFormat("enum %s %s;\n",
                                  table->actionEnumName.c_str(), actionVariableName.c_str());
            builder->emitIndent();
        }
    }
    builder->blockStart();
    builder->emitIndent();

    BUG_CHECK(method->expr->arguments->size() == 0, "%1%: table apply with arguments", method);
    cstring keyname = "key";
    if (table->keyGenerator != nullptr) {
        builder->emitIndent();
        builder->appendLine("/* construct key */");
        builder->emitIndent();
        builder->appendFormat("%s %s", table->keyTypeName.c_str(), keyname.c_str());
        builder->endOfStatement(true);
        table->emitKey(builder, keyname);
    }
    builder->emitIndent();
    builder->appendLine("/* value */");
    builder->emitIndent();
    cstring valueName = "value";
    builder->appendFormat("%s *%s = NULL", table->valueTypeName.c_str(), valueName.c_str());
    builder->endOfStatement(true);

    if (table->keyGenerator != nullptr) {
        builder->emitIndent();
        builder->appendLine("/* perform lookup */");
        builder->emitIndent();
        builder->target->emitTableLookup(builder, table->dataMapName, keyname, valueName);
        builder->endOfStatement(true);
    }

    builder->emitIndent();
    builder->appendLine("/* run action */");
    builder->emitIndent();
    builder->appendFormat("if (%s != NULL) ", valueName.c_str());
    builder->newline();
    table->emitAction(builder, valueName);
    toDereference.clear();
    builder->blockEnd(false);
}

bool ControlBodyTranslator::preorder(const IR::ExitStatement*) {
    builder->appendFormat("goto %s;", control->program->endLabel.c_str());
    return false;
}

bool ControlBodyTranslator::preorder(const IR::ReturnStatement*) {
    builder->appendFormat("goto %s;", control->program->endLabel.c_str());
    return false;
}

bool ControlBodyTranslator::preorder(const IR::IfStatement* statement) {
    bool isHit = P4::TableApplySolver::isHit(statement->condition, control->program->refMap,
                                             control->program->typeMap);
    if (isHit) {
        // visit first the table, and then the conditional
        auto member = statement->condition->to<IR::Member>();
        CHECK_NULL(member);
        visit(member->expr);  // table application.  Sets 'hitVariable'
        builder->emitIndent();
    }

    // This is almost the same as the base class method
    builder->append("if (");
    if (isHit)
        builder->append(control->hitVariable);
    else
        visit(statement->condition);
    builder->append(") ");
    if (!statement->ifTrue->is<IR::BlockStatement>()) {
        builder->increaseIndent();
        builder->newline();
        builder->emitIndent();
    }
    visit(statement->ifTrue);
    if (!statement->ifTrue->is<IR::BlockStatement>())
        builder->decreaseIndent();
    if (statement->ifFalse != nullptr) {
        builder->newline();
        builder->emitIndent();
        builder->append("else ");
        if (!statement->ifFalse->is<IR::BlockStatement>()) {
            builder->increaseIndent();
            builder->newline();
            builder->emitIndent();
        }
        visit(statement->ifFalse);
        if (!statement->ifFalse->is<IR::BlockStatement>())
            builder->decreaseIndent();
    }
    return false;
}

bool ControlBodyTranslator::preorder(const IR::SwitchStatement* statement) {
    cstring newName = control->program->refMap->newName("action_run");
    saveAction.push_back(newName);
    // This must be a table.apply().action_run
    auto mem = statement->expression->to<IR::Member>();
    BUG_CHECK(mem != nullptr,
              "%1%: Unexpected expression in switch statement", statement->expression);
    visit(mem->expr);
    saveAction.pop_back();
    saveAction.push_back(nullptr);
    builder->emitIndent();
    builder->append("switch (");
    builder->append(newName);
    builder->append(") ");
    builder->blockStart();
    builder->emitIndent();
    for (auto c : statement->cases) {
        builder->emitIndent();
        if (c->label->is<IR::DefaultExpression>()) {
            builder->append("default");
        } else {
            builder->append("case ");
            auto pe = c->label->to<IR::PathExpression>();
            auto decl = control->program->refMap->getDeclaration(pe->path, true);
            BUG_CHECK(decl->is<IR::P4Action>(), "%1%: expected an action", pe);
            auto act = decl->to<IR::P4Action>();
            cstring name = PolycubeObject::externalName(act);
            builder->append(name);
        }
        builder->append(":");
        builder->newline();
        builder->emitIndent();
        visit(c->statement);
        builder->newline();
        builder->emitIndent();
        builder->appendLine("break;");
    }
    builder->blockEnd(false);
    saveAction.pop_back();
    return false;
}

/////////////////////////////////////////////////

cstring PolycubeControl::headersTypeName;
cstring PolycubeControl::packetTypeName;
cstring PolycubeControl::metadataTypeName;

PolycubeControl::PolycubeControl(const PolycubeProgram* program, const IR::ControlBlock* block,
                         const IR::Parameter* parserHeaders) :
        program(program), controlBlock(block), headers(nullptr), packet(nullptr), metadata(nullptr),
        parserHeaders(parserHeaders), codeGen(nullptr), emitExterns(program->options.emitExterns) {}

void PolycubeControl::scanConstants() {
    for (auto c : controlBlock->constantValue) {
        auto b = c.second;
        if (!b->is<IR::Block>()) continue;
        if (b->is<IR::TableBlock>()) {
            auto tblblk = b->to<IR::TableBlock>();
            auto tbl = new PolycubeTable(program, tblblk, codeGen);
            tables.emplace(tblblk->container->name, tbl);
        } else {
            ::error("Unexpected block %s nested within control", b->toString());
        }
    }
}

bool PolycubeControl::build() {
    hitVariable = program->refMap->newName("hit");
    auto pl = controlBlock->container->type->applyParams;
    if (pl->size() != 3) {
        ::error("Expected control block to have exactly 3 parameters");
        return false;
    }

    auto it = pl->parameters.begin();
    headers = *it;
    PolycubeControl::headersTypeName = headers->type->getP4Type()->toString();
    ++it;
    packet = *it;
    PolycubeControl::packetTypeName = packet->type->getP4Type()->toString();
    ++it;
    metadata = *it;
    PolycubeControl::metadataTypeName = metadata->type->getP4Type()->toString();

    codeGen = new ControlBodyTranslator(this);
    codeGen->substitute(headers, parserHeaders);

    scanConstants();
    return ::errorCount() == 0;
}

void PolycubeControl::emitDeclaration(CodeBuilder* builder, const IR::Declaration* decl) {
    if (decl->is<IR::Declaration_Variable>()) {
        auto vd = decl->to<IR::Declaration_Variable>();
        auto etype = PolycubeTypeFactory::instance->create(vd->type);
        builder->emitIndent();
        etype->declare(builder, vd->name, false);
        builder->endOfStatement(true);
        BUG_CHECK(vd->initializer == nullptr,
                  "%1%: declarations with initializers not supported", decl);
        return;
    } else if (decl->is<IR::P4Table>() ||
               decl->is<IR::P4Action>() ||
               decl->is<IR::Declaration_Instance>()) {
        return;
    }
    BUG("%1%: not yet handled", decl);
}

void PolycubeControl::emit(CodeBuilder* builder) {
    auto hitType = PolycubeTypeFactory::instance->create(IR::Type_Boolean::get());
    hitType->declare(builder, hitVariable, false);
    builder->endOfStatement(true);
    for (auto a : controlBlock->container->controlLocals)
        emitDeclaration(builder, a);
    builder->emitIndent();
    codeGen->setBuilder(builder);
    controlBlock->container->body->apply(*codeGen);
    builder->newline();
}

void PolycubeControl::emitTableTypes(CodeBuilder* builder) {
    for (auto it : tables)
        it.second->emitTypes(builder);
}

void PolycubeControl::emitTableInstances(CodeBuilder* builder) {
    for (auto it : tables)
        it.second->emitInstance(builder);
}

void PolycubeControl::emitTableInitializers(CodeBuilder* builder) {
    for (auto it : tables)
        it.second->emitInitializer(builder);
}

}  // namespace POLYCUBE
