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

#include "polycubeType.h"
#include "polycubeControl.h"

namespace POLYCUBE {

PolycubeTypeFactory* PolycubeTypeFactory::instance;

PolycubeType* PolycubeTypeFactory::create(const IR::Type* type) {
    CHECK_NULL(type);
    CHECK_NULL(typeMap);
    PolycubeType* result = nullptr;
    if (type->is<IR::Type_Boolean>()) {
        result = new PolycubeBoolType();
    } else if (auto bt = type->to<IR::Type_Bits>()) {
        result = new PolycubeScalarType(bt);
    } else if (auto st = type->to<IR::Type_StructLike>()) {
        result = new PolycubeStructType(st);
    } else if (auto tt = type->to<IR::Type_Typedef>()) {
        auto canon = typeMap->getTypeType(type, true);
        result = create(canon);
        auto path = new IR::Path(tt->name);
        result = new PolycubeTypeName(new IR::Type_Name(path), result);
    } else if (auto tn = type->to<IR::Type_Name>()) {
        auto canon = typeMap->getTypeType(type, true);
        result = create(canon);
        result = new PolycubeTypeName(tn, result);
    } else if (auto te = type->to<IR::Type_Enum>()) {
        result = new PolycubeEnumType(te);
    } else if (auto ts = type->to<IR::Type_Stack>()) {
        auto et = create(ts->elementType);
        if (et == nullptr)
            return nullptr;
        result = new PolycubeStackType(ts, et);
    } else {
        ::error("Type %1% not supported", type);
    }

    return result;
}

void
PolycubeBoolType::declare(CodeBuilder* builder, cstring id, bool asPointer) {
    emit(builder);
    if (asPointer)
        builder->append("*");
    builder->appendFormat(" %s", id.c_str());
}

/////////////////////////////////////////////////////////////

void PolycubeStackType::declare(CodeBuilder* builder, cstring id, bool) {
    elementType->declareArray(builder, id, size);
}

void PolycubeStackType::emitInitializer(CodeBuilder* builder) {
    builder->append("{");
    for (unsigned i = 0; i < size; i++) {
        if (i > 0)
            builder->append(", ");
        elementType->emitInitializer(builder);
    }
    builder->append(" }");
}

unsigned PolycubeStackType::widthInBits() {
    return size * elementType->to<IHasWidth>()->widthInBits();
}

unsigned PolycubeStackType::implementationWidthInBits() {
    return size * elementType->to<IHasWidth>()->implementationWidthInBits();
}

/////////////////////////////////////////////////////////////

unsigned PolycubeScalarType::alignment() const {
    if (width <= 8)
        return 1;
    else if (width <= 16)
        return 2;
    else if (width <= 32)
        return 4;
    else if (width <= 64)
        return 8;
    else
        // compiled as u8*
        return 1;
}

void PolycubeScalarType::emit(CodeBuilder* builder) {
    auto prefix = isSigned ? "i" : "u";

    if (width <= 8)
        builder->appendFormat("%s8", prefix);
    else if (width <= 16)
        builder->appendFormat("%s16", prefix);
    else if (width <= 32)
        builder->appendFormat("%s32", prefix);
    else if (width <= 64)
        builder->appendFormat("%s64", prefix);
    else
        builder->appendFormat("u8*");
}

void
PolycubeScalarType::declare(CodeBuilder* builder, cstring id, bool asPointer) {
    if (PolycubeScalarType::generatesScalar(width)) {
        emit(builder);
        if (asPointer)
            builder->append("*");
        builder->spc();
        builder->append(id);
    } else {
        if (asPointer)
            builder->append("u8*");
        else
            builder->appendFormat("u8 %s[%d]", id.c_str(), bytesRequired());
    }
}

//////////////////////////////////////////////////////////

PolycubeStructType::PolycubeStructType(const IR::Type_StructLike* strct) :
        PolycubeType(strct) {
    if (strct->is<IR::Type_Struct>())
        kind = "struct";
    else if (strct->is<IR::Type_Header>())
        kind = "struct";
    else if (strct->is<IR::Type_HeaderUnion>())
        kind = "union";
    else
        BUG("Unexpected struct type %1%", strct);
    name = strct->name.name;
    width = 0;
    implWidth = 0;

    for (auto f : strct->fields) {
        auto type = PolycubeTypeFactory::instance->create(f->type);
        auto wt = dynamic_cast<IHasWidth*>(type);
        if (wt == nullptr) {
            ::error("Polycube: Unsupported type in struct: %s", f->type);
        } else {
            width += wt->widthInBits();
            implWidth += wt->implementationWidthInBits();
        }
        fields.push_back(new PolycubeField(type, f));
    }
}

void
PolycubeStructType::declare(CodeBuilder* builder, cstring id, bool asPointer) {
    builder->append(kind);
    if (asPointer)
        builder->append("*");
    builder->appendFormat(" %s %s", name.c_str(), id.c_str());
}

void PolycubeStructType::emitInitializer(CodeBuilder* builder) {
    builder->blockStart();
    builder->emitIndent();
    if (type->is<IR::Type_Struct>() || type->is<IR::Type_HeaderUnion>()) {
        for (auto f : fields) {
            builder->emitIndent();
            builder->appendFormat(".%s = ", f->field->name.name);
            f->type->emitInitializer(builder);
            builder->append(",");
            builder->newline();
        }
    } else if (type->is<IR::Type_Header>()) {
        builder->emitIndent();
        builder->appendLine(".polycube_valid = 0");
    } else {
        BUG("Unexpected type %1%", type);
    }
    builder->blockEnd(false);
}

void PolycubeStructType::emit(CodeBuilder* builder) {
    builder->emitIndent();
    builder->append(kind);
    builder->spc();
    builder->append(name);
    builder->spc();
    builder->blockStart();
    builder->emitIndent();

    bool first = true;
    for (auto f : fields) {
        auto type = f->type;
        if (first) {
            first = false;
        } else {
            builder->emitIndent();
        }
        if (name == PolycubeControl::headersTypeName) {
            type->declare(builder, "*" + f->field->name, false);
        } else {
            type->declare(builder, f->field->name, false);
        }
        if (f->type->is<PolycubeScalarType>()
            && ((f->type->type->width_bits() < 8)
                || ((f->type->type->width_bits() & (f->type->type->width_bits() - 1)) != 0))) {
            builder->appendFormat(" : %d", f->type->type->width_bits());
        }
        builder->append("; ");
        builder->append("/* ");
        builder->append(type->type->toString());
        if (f->comment != nullptr) {
            builder->append(" ");
            builder->append(f->comment);
        }
        builder->append(" */");
        builder->newline();
    }
    builder->blockEnd(false);
    builder->appendFormat(" __attribute__((packed))");
    builder->endOfStatement(true);
}

void
PolycubeStructType::declareArray(CodeBuilder* builder, cstring id, unsigned size) {
    builder->appendFormat("%s %s[%d]", name.c_str(), id.c_str(), size);
}

///////////////////////////////////////////////////////////////

void PolycubeTypeName::declare(CodeBuilder* builder, cstring id, bool asPointer) {
    if (canonical != nullptr)
        canonical->declare(builder, id, asPointer);
}

void PolycubeTypeName::emitInitializer(CodeBuilder* builder) {
    if (canonical != nullptr)
        canonical->emitInitializer(builder);
}

unsigned PolycubeTypeName::widthInBits() {
    auto wt = dynamic_cast<IHasWidth*>(canonical);
    if (wt == nullptr) {
        ::error("Type %1% does not have a fixed witdh", type);
        return 0;
    }
    return wt->widthInBits();
}

unsigned PolycubeTypeName::implementationWidthInBits() {
    auto wt = dynamic_cast<IHasWidth*>(canonical);
    if (wt == nullptr) {
        ::error("Type %1% does not have a fixed witdh", type);
        return 0;
    }
    return wt->implementationWidthInBits();
}

void
PolycubeTypeName::declareArray(CodeBuilder* builder, cstring id, unsigned size) {
    declare(builder, id, false);
    builder->appendFormat("[%d]", size);
}

////////////////////////////////////////////////////////////////

void PolycubeEnumType::declare(POLYCUBE::CodeBuilder* builder, cstring id, bool asPointer) {
    builder->append("enum ");
    builder->append(getType()->name);
    if (asPointer)
        builder->append("*");
    builder->append(" ");
    builder->append(id);
}

void PolycubeEnumType::emit(POLYCUBE::CodeBuilder* builder) {
    builder->append("enum ");
    auto et = getType();
    builder->append(et->name + " ");
    builder->blockStart();
    builder->emitIndent();
    bool first = true;
    for (auto m : et->members) {
        if (first) {
            first = false;
        } else {
            builder->emitIndent();
        }
        builder->append(m->name);
        builder->appendLine(",");
    }
    builder->blockEnd(false);
    builder->append(";");
    builder->newline();
}

}  // namespace POLYCUBE
