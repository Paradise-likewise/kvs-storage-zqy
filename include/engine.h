#pragma once
#ifndef INCLUDE_ENGINE_H_
#define INCLUDE_ENGINE_H_
#include <functional>
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <future>

#include "conf.h"
#include "interfaces.h"
#include "options.h"
#include "glog/logging.h"

#include "btree.h"

namespace kvs
{
/**
 * The actual Engine you need to implement
 * TODO: your code
 */
class Engine : public IEngine
{
public:
    Engine(const std::string &path, EngineOptions options);

    static Pointer new_instance(const std::string &path, EngineOptions options)
    {
        // std::cout << "Start new_instance...\n";
        return std::make_shared<Engine>(path, options);
    }

    virtual ~Engine();

    // Write a key-value pair into engine
    RetCode put(const Key &key, const Value &value) override;
    RetCode remove(const Key &key) override;
    RetCode get(const Key &key, Value &value) override;

    /**
     * @brief syncing all the modifications to the disk/SSD
     * All the modification operations before calling sync() should be
     * persistent
     */
    RetCode sync() override;

    /**
     * @brief visit applies the visitor to all the KV pairs within
     * the range of [lower, upper).
     * Note that lower == "" is treated as the one before all the keys
     * upper == "" is treated as the one after all the keys
     * So, visit("", "", visitor) will traverse the entire database.
     * @param lower the lower key range
     * @param upper the upper key range
     * @param visitor any function which accepts Key, Value and return void
     *
     * @example
     * void print_visitor(const Key& k, const Value& v)
     * {
     *     std::cout << "visiting key " << k << ", value " << v << std::endl;
     * }
     * engine.visit("a", "Z", print_visitor);
     */
    RetCode visit(const Key &lower,
                  const Key &upper,
                  const Visitor &visitor) override;
    /**
     * @brief generate snapshot, if you want to implement
     * @see class Snapshot
     */
    std::shared_ptr<IROEngine> snapshot() override;

    /**
     * @brief trigger garbage collection, if you want to implement
     */
    RetCode garbage_collect() override;

private:
    // std::ofstream log_ofstream;
    // int log_app_fd;
    // int log_r_fd;
    std::string log_filename;
    
    BTree btree;
    int log_offset;
    FILE* log_fp;

    mutable std::shared_mutex _mutex;
    mutable std::shared_mutex _mutex2;
    mutable std::mutex _mutex_gc;

    void rebuild_btree(const std::string& log_filename);

    void log_line(FILE*& fp, const std::string& line);
    void read_line(FILE*& fp, std::string& line);
    int read_key_value(FILE*& fp, Key& key, Value& value);  // return 1 if insert, 0 if remove, -1 if failed
    bool read_num(FILE*&fp, size_t num, std::string& line);

    void _gc();

    struct NodeIdxPair {
        BTree::BNode* v;
        size_t idx;
        NodeIdxPair(BTree::BNode* _v, size_t _idx) :
            v(_v), idx(_idx)
        { }
    };
    std::vector<NodeIdxPair> _pair_vector;
    std::vector<size_t> _pair_addr;
    bool pair_comp(size_t i1, size_t i2) {
        return (_pair_vector[i1].v->data[_pair_vector[i1].idx] < 
            _pair_vector[i2].v->data[_pair_vector[i2].idx]);
    }
};

}  // namespace kvs

#endif  // INCLUDE_ENGINE_H_
