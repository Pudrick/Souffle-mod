#pragma once

#include "interpreter/Relation.h"

namespace souffle::interpreter {

class NullRelationWrapper : public RelationWrapper {
public:
    using RamDomain = souffle::RamDomain;
    using arity_type = souffle::Relation::arity_type;

    NullRelationWrapper(arity_type arity, arity_type auxiliaryArity, std::string relName)
            : RelationWrapper(arity, auxiliaryArity, std::move(relName)) {}

    class NullIterator : public iterator_base {
    public:
        iterator_base& operator++() override {
            return *this;
        }
        const RamDomain* operator*() override {
            return nullptr;
        }
        iterator_base* clone() const override {
            return new NullIterator();
        }
        bool equal(const iterator_base& other) const override {
            return true;
        }
    };

    Iterator begin() const override {
        return Iterator(new NullIterator());
    }
    Iterator end() const override {
        return Iterator(new NullIterator());
    }
    void insert(const RamDomain*) override {}
    bool contains(const RamDomain*) const override {
        return false;
    }
    std::size_t size() const override {
        return 0;
    }
    void purge() override {}
    void printStats(std::ostream& o) const override {
        o << "NullRelation: " << getName() << std::endl;
    }
    souffle::interpreter::Order getIndexOrder(std::size_t) const override {
        return souffle::interpreter::Order::create(getArity());
    }
    IndexViewPtr createView(const std::size_t&) const override {
        struct NullView : public souffle::interpreter::ViewWrapper {};
        return mk<NullView>();
    }
};
}  // namespace souffle::interpreter