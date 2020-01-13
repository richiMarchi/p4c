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

#ifndef _BACKENDS_POLYCUBE_POLYCUBETYPE_H_
#define _BACKENDS_POLYCUBE_POLYCUBETYPE_H_

#include "lib/algorithm.h"
#include "lib/sourceCodeBuilder.h"
#include "polycubeObject.h"
#include "ir/ir.h"

namespace POLYCUBE {

// Base class for POLYCUBE types
class PolycubeType : public PolycubeObject {
 protected:
    explicit PolycubeType(const IR::Type* type) : type(type) {}
 public:
    const IR::Type* type;
    virtual void emit(CodeBuilder* builder) = 0;
    virtual void declare(CodeBuilder* builder, cstring id, bool asPointer) = 0;
    virtual void emitInitializer(CodeBuilder* builder) = 0;
    virtual void declareArray(CodeBuilder* /*builder*/, cstring /*id*/, unsigned /*size*/)
    { BUG("%1%: unsupported array", type); }
    template<typename T> bool is() const { return dynamic_cast<const T*>(this) != nullptr; }
    template<typename T> T *to() { return dynamic_cast<T*>(this); }
};

class IHasWidth {
 public:
    virtual ~IHasWidth() {}
    // P4 width
    virtual unsigned widthInBits() = 0;
    // Width in the target implementation.
    // Currently a multiple of 8.
    virtual unsigned implementationWidthInBits() = 0;
};

class PolycubeTypeFactory {
 protected:
    const P4::TypeMap* typeMap;
    explicit PolycubeTypeFactory(const P4::TypeMap* typeMap) :
            typeMap(typeMap) { CHECK_NULL(typeMap); }
 public:
    static PolycubeTypeFactory* instance;
    static void createFactory(const P4::TypeMap* typeMap)
    { PolycubeTypeFactory::instance = new PolycubeTypeFactory(typeMap); }
    virtual PolycubeType* create(const IR::Type* type);
};

class PolycubeBoolType : public PolycubeType, public IHasWidth {
 public:
    PolycubeBoolType() : PolycubeType(IR::Type_Boolean::get()) {}
    void emit(CodeBuilder* builder) override
    { builder->append("u8"); }
    void declare(CodeBuilder* builder, cstring id, bool asPointer) override;
    void emitInitializer(CodeBuilder* builder) override
    { builder->append("0"); }
    unsigned widthInBits() override { return 1; }
    unsigned implementationWidthInBits() override { return 8; }
};

class PolycubeStackType : public PolycubeType, public IHasWidth {
    PolycubeType* elementType;
    unsigned  size;
 public:
    PolycubeStackType(const IR::Type_Stack* type, PolycubeType* elementType) :
            PolycubeType(type), elementType(elementType), size(type->getSize()) {
        CHECK_NULL(type); CHECK_NULL(elementType);
        BUG_CHECK(elementType->is<IHasWidth>(), "Unexpected element type %1%", elementType);
    }
    void emit(CodeBuilder*) override {}
    void declare(CodeBuilder* builder, cstring id, bool asPointer) override;
    void emitInitializer(CodeBuilder* builder) override;
    unsigned widthInBits() override;
    unsigned implementationWidthInBits() override;
};

class PolycubeScalarType : public PolycubeType, public IHasWidth {
 public:
    const unsigned width;
    const bool     isSigned;
    explicit PolycubeScalarType(const IR::Type_Bits* bits) :
            PolycubeType(bits), width(bits->size), isSigned(bits->isSigned) {}
    unsigned bytesRequired() const { return ROUNDUP(width, 8); }
    unsigned alignment() const;
    void emit(CodeBuilder* builder) override;
    void declare(CodeBuilder* builder, cstring id, bool asPointer) override;
    void emitInitializer(CodeBuilder* builder) override
    { builder->append("0"); }
    unsigned widthInBits() override { return width; }
    unsigned implementationWidthInBits() override { return bytesRequired() * 8; }
    // True if this width is small enough to store in a machine scalar
    static bool generatesScalar(unsigned width)
    { return width <= 64; }
};

// This should not always implement IHasWidth, but it may...
class PolycubeTypeName : public PolycubeType, public IHasWidth {
    const IR::Type_Name* type;
    PolycubeType* canonical;
 public:
    PolycubeTypeName(const IR::Type_Name* type, PolycubeType* canonical) :
            PolycubeType(type), type(type), canonical(canonical) {}
    void emit(CodeBuilder* builder) override { canonical->emit(builder); }
    void declare(CodeBuilder* builder, cstring id, bool asPointer) override;
    void emitInitializer(CodeBuilder* builder) override;
    unsigned widthInBits() override;
    unsigned implementationWidthInBits() override;
    void declareArray(CodeBuilder* builder, cstring id, unsigned size) override;
};

// Also represents headers and unions
class PolycubeStructType : public PolycubeType, public IHasWidth {
    class PolycubeField {
     public:
        cstring comment;
        PolycubeType* type;
        const IR::StructField* field;

        PolycubeField(PolycubeType* type, const IR::StructField* field, cstring comment = nullptr) :
            comment(comment), type(type), field(field) {}
    };

 public:
    cstring  kind;
    cstring  name;
    std::vector<PolycubeField*>  fields;
    unsigned width;
    unsigned implWidth;

    explicit PolycubeStructType(const IR::Type_StructLike* strct);
    void declare(CodeBuilder* builder, cstring id, bool asPointer) override;
    void emitInitializer(CodeBuilder* builder) override;
    unsigned widthInBits() override { return width; }
    unsigned implementationWidthInBits() override { return implWidth; }
    void emit(CodeBuilder* builder) override;
    void declareArray(CodeBuilder* builder, cstring id, unsigned size) override;
};

class PolycubeEnumType : public PolycubeType, public POLYCUBE::IHasWidth {
 public:
    explicit PolycubeEnumType(const IR::Type_Enum* type) : PolycubeType(type) {}
    void emit(CodeBuilder* builder) override;
    void declare(CodeBuilder* builder, cstring id, bool asPointer) override;
    void emitInitializer(CodeBuilder* builder) override
    { builder->append("0"); }
    unsigned widthInBits() override { return 32; }
    unsigned implementationWidthInBits() override { return 32; }
    const IR::Type_Enum* getType() const { return type->to<IR::Type_Enum>(); }
};

}  // namespace POLYCUBE

#endif /* _BACKENDS_POLYCUBE_POLYCUBETYPE_H_ */
