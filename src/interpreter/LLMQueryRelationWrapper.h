/************************************************************************
 *
 * @file LLMQueryRelationWrapper.h
 *
 * Defines classes that extends the RelationWrapper, used to be LLM Query.
 *
 ***********************************************************************/
#pragma once

// #include "interpreter/Engine.h"
#include "interpreter/Relation.h"

#include <SQLiteCpp/SQLiteCpp.h>

#pragma once

#include "interpreter/Relation.h"  // 引用标准的 Relation 实现
#include <string>
#include <utility>
#include <vector>

namespace souffle::interpreter {

class Engine;
/**
 * @brief Load data from API(moduled.)
 *
 * @tparam Arity relation 的元组长度，在编译时确定
 */
template <std::size_t Arity>
class LLMQueryRelationWrapper : public RelationWrapper {
private:
    // 定义内部使用的标准 B-Tree Relation 类型
    using InternalRelation = Relation<Arity, 0, Btree>;
    using Tuple = typename InternalRelation::Tuple;

    Engine& engineRef;
    Own<InternalRelation> internalRelation;  // 内部持有一个标准的 B-Tree Relation
    mutable bool dataLoaded = false;

    /**
     * @brief 按需加载数据。
     * 仅在第一次访问时执行。从 API 获取数据并填充到内部的 B-Tree 中。
     */
    void loadDataIfNeeded() const {
        if (dataLoaded) {
            return;
        }

        auto& relMap = engineRef.getRelIDMap();
        const std::string targetRelName = "R_5coref4java8Callable7getName";
        auto map_iterator = relMap.find(targetRelName);
        if (map_iterator == relMap.end()) {
            // dataLoaded = true;
            // 即使没数据也标记为已加载，避免重复尝试
            return;
        }
        auto nameID = map_iterator->second;
        auto& rel = *engineRef.getRelationHandle(nameID);

        std::set<std::string> unemptyName;
        for (const auto& [fst, snd] : relMap) {
            const auto& temprel = snd;
            auto& trrel = *engineRef.getRelationHandle(temprel);
            if (trrel.getName() == "R_5coref4java8Callable16getBelongedClass") continue;
            if (trrel.size() != 0) unemptyName.insert(trrel.getName());
        }

#ifndef NDEBUG
        std::cout << "name start===" << std::endl;
        for (const auto& name : unemptyName) {
            std::cout << name << std::endl;
        }
        std::cout << "name end===" << std::endl;
#endif

        // 这里这段暂时没有用上，注释掉也可以
        // 获取依赖关系的数据
        std::vector<int64_t> methodHashVec;
        std::vector<std::string> methodNameVec;
        for (const RamDomain* tupleData : rel) {
            methodHashVec.push_back(tupleData[1]);  // 0:hashid, 1:name, 2: signature
            auto methodNameId = tupleData[0];
            const std::string& methodName = engineRef.getSymbolTable().decode(methodNameId);
            methodNameVec.push_back(methodName);

            dataLoaded = true;
        }

        // query for class name

        // 为了测试方便，暂时把数据直接硬编码进来，后续可以替换成LLM的API
        std::vector<std::string> classNames = {"Book", "Book", "Book", "Book", "Book", "Book", "Book",
                "Member", "Member", "Member", "Member", "Member", "Member", "Library", "Library", "Library",
                "Library", "Library", "Library", "Library", "Library", "LibraryManagementSystem"};

        std::vector<std::string> methodSignatures = {
                "Book.Book:null(java.lang.String, java.lang.String, java.lang.String)",
                "Book.getIsbn:java.lang.String()", "Book.getTitle:java.lang.String()",
                "Book.getAuthor:java.lang.String()", "Book.isAvailable:boolean()",
                "Book.setAvailable:void(boolean)", "Book.toString:java.lang.String()",
                "Member.Member:null(java.lang.String, java.lang.String)",
                "Member.getMemberId:java.lang.String()", "Member.getName:java.lang.String()",
                "Member.getBorrowedBooks:java.util.List<Book>()", "Member.borrowBook:void(Book)",
                "Member.returnBook:void(Book)", "Library.Library:null()",
                "Library.addBook:void(java.lang.String, java.lang.String, java.lang.String)",
                "Library.addMember:void(java.lang.String, java.lang.String)",
                "Library.borrowBook:boolean(java.lang.String, java.lang.String)",
                "Library.returnBook:boolean(java.lang.String, java.lang.String)",
                "Library.getAvailableBooks:java.util.List<Book>()",
                "Library.getMemberBorrowedBooks:java.util.List<Book>(java.lang.String)",
                "Library.searchBookByTitle:java.util.List<Book>(java.lang.String)",
                "LibraryManagementSystem.main:void(java.lang.String[])"};

        std::vector<int64_t> classElementIds;
        SQLite::Database db("../mulme-test/coref_java_src.db", SQLite::OPEN_READONLY);
        SQLite::Statement queryClass(db, "SELECT element_hash_id FROM class WHERE qualified_name = ?");

        for (const auto& className : classNames) {
            queryClass.bind(1, className);
            if (queryClass.executeStep()) {
                classElementIds.push_back(queryClass.getColumn(0).getInt64());
            }
            queryClass.reset();
        }

        std::vector<int64_t> methodElementIds;
        SQLite::Statement queryMethod(db, "SELECT element_hash_id FROM method WHERE signature = ?");
        SQLite::Statement queryConstructor(db, "SELECT element_hash_id FROM constructor WHERE signature = ?");

        for (const auto& methodSignature : methodSignatures) {
            queryMethod.bind(1, methodSignature);
            if (queryMethod.executeStep()) {
                methodElementIds.push_back(queryMethod.getColumn(0).getInt64());
            } else {
                queryConstructor.bind(1, methodSignature);
                if (queryConstructor.executeStep()) {
                    methodElementIds.push_back(queryConstructor.getColumn(0).getInt64());
                }
                queryConstructor.reset();
            }
            queryMethod.reset();
        }

        // query for class hashid

        for (size_t i = 0; i < classElementIds.size(); i++) {
            Tuple tupleToInsert;
            tupleToInsert[0] = souffle::ramBitCast<RamDomain>(classElementIds[i]);
            tupleToInsert[1] = souffle::ramBitCast<RamDomain>(methodElementIds[i]);
            internalRelation->insert(tupleToInsert);
        }
        dataLoaded = true;
    }

public:
    LLMQueryRelationWrapper(
            Engine& eng, const ram::Relation& id, const ram::analysis::IndexCluster& indexSelection)
            : RelationWrapper(Arity, 0, id.getName()), engineRef(eng) {
        // 创建内部持有的 BTree relation
        internalRelation = mk<InternalRelation>(id.getName(), indexSelection);
    }

    // --- 接口都委托给 internalRelation ---

    Iterator begin() const override {
        loadDataIfNeeded();
        return internalRelation->begin();
    }

    Iterator end() const override {
        loadDataIfNeeded();
        return internalRelation->end();
    }

    // 忽略来自 Souffle 规则的插入请求
    void insert(const RamDomain* /* tuple */) override { /* ignore */
        loadDataIfNeeded();
    }

    bool contains(const RamDomain* tuple) const override {
        loadDataIfNeeded();
        return internalRelation->contains(tuple);
    }

    bool isEmpty() const {
        return !dataLoaded || internalRelation->empty();
    }

    std::size_t size() const override {
        loadDataIfNeeded();
        return internalRelation->size();
    }

    void purge() override {
        internalRelation->purge();
        dataLoaded = false;  // 清空后允许重新加载
    }

    void printStats(std::ostream& o) const override {
        loadDataIfNeeded();
        internalRelation->printStats(o);
    }

    Order getIndexOrder(std::size_t idx) const override {
        loadDataIfNeeded();
        return internalRelation->getIndexOrder(idx);
    }

    IndexViewPtr createView(const std::size_t& indexPos) const override {
        loadDataIfNeeded();
        return internalRelation->createView(indexPos);
    }
};

}  // namespace souffle::interpreter
