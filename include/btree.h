#include <string>
#include <vector>
#include <algorithm>
#include <functional>

#include "glog/logging.h"

namespace kvs
{
/* 
 * (3.5)-Tree:
 * max key: M-1 = 4;
 * min key: ceil(M/2)-1 = 2;
 * max child: M = 5;
 * min child (except root): ceil(M/2) = 3
 */

#define M 5


class BTree
{
public:
    using Key = std::string;
    using Value = std::string;
    using Visitor = std::function<void(const Key &, const Value &)>;

    class BNode
    {
    public:
        BNode(BNode* parent, bool is_leaf) : 
            parent(parent), is_leaf(is_leaf), prev(nullptr), next(nullptr)
        {
            key.reserve(M - 1);
            child.reserve(M);
            if (is_leaf) data.reserve(M - 1);
        }
        BNode* parent;
        bool is_leaf;

        std::vector<Key> key;
        std::vector<BNode*> child;
        /* Leaf Node Only */
        std::vector<int> data;
        BNode* prev;
        BNode* next;

        bool is_full() { return key.size() == (M - 1); }

        void delete_child() {
            if (is_leaf) return;
            for (size_t i = 0; i < child.size(); i++) {
                child[i]->delete_child();
                delete child[i];
            }
        }
        void show_key(std::string &log_out) {
            log_out.append("[");
            for (size_t i = 0; i < key.size(); i++) {
                log_out.append(key[i]);
                if (is_leaf) {
                    log_out.append("(");
                    log_out.append(std::to_string(data[i]));
                    log_out.append(")");
                }
                log_out.append(", ");
            }
            log_out.append("]");
        }
    };

    BTree();

    ~BTree();

    BNode* get_root() { return _root; }

    size_t size() { return _size; }

    /*
     * return data: find the key!
     * return unsigned_int_max: key not found! 
     */
    size_t search(const Key& key, BNode*& _hot, std::vector<size_t>& _hot_idx);

    size_t search(const Key& key);

    /*
     * return 1: key not exist, insert success!
     * return 0: key exist, modify success!
     * return -1: error 
     */
    int insert(const Key &key, const size_t data);

    /*
     * return 1: key exist, remove success!
     * return 0: key not exist, remove failed!
     * return -1: error 
     */
    int remove(const Key &key);

    void show_tree() {
        std::vector<BNode*> node_queue;
        size_t i = 0;
        int level = 0;
        int child_n = 0;
        BNode* first_node = _root;
        BNode* cur_parent = nullptr;
        node_queue.emplace_back(_root);

        std::string log_out("_size = ");
        log_out.append(std::to_string(_size));
        while (i < node_queue.size()) {
            BNode* v = node_queue[i];
            i += 1;
            if (v == first_node) {
                level += 1;
                cur_parent = first_node->parent;
                child_n = 0;
                first_node = first_node->child[0];
                LOG(INFO) << log_out;
                log_out.clear();
                log_out.append("ch0: ");
            }
            else if (v->parent != cur_parent) {
                child_n += 1;
                // log_out.append("ch");
                // log_out.append(std::to_string(child_n));
                // log_out.append(": ");
                log_out.append(" || ");
                cur_parent = v->parent;
            }
            
            v->show_key(log_out);
            log_out.append(", ");

            for (size_t j = 0; j < v->child.size(); j++) {
                if (v->child[j] != nullptr) {
                    node_queue.emplace_back(v->child[j]);
                }
            }            
        }
        LOG(INFO) << log_out;
    }


protected:
    size_t _size; // total key num
    BNode* _root;
    std::vector<size_t> _tmp_idx;

    void solve_overflow_leaf(const Key& key, const size_t data, BNode*& _hot, std::vector<size_t>& _hot_idx);

    void solve_overflow_innode(const Key& key, BNode* _c, BNode*& _hot, std::vector<size_t>& _hot_idx);

    void solve_underflow_leaf(BNode*& _hot, std::vector<size_t>& _hot_idx);

    void solve_underflow_innode(BNode*& _hot, std::vector<size_t>& _hot_idx);
    

};

}
