#include "btree.h"

namespace kvs
{
BTree::BTree() :
    _size(0), _root(new BNode(nullptr, true))
{
    _root->child.emplace_back(nullptr);
    _tmp_idx.resize(M);
}

BTree::~BTree()
{
    _root->delete_child();
    delete _root;
}

size_t BTree::search(const Key& key, BNode*& _hot, std::vector<size_t>& _hot_idx) 
{
    // LOG(INFO) << "===== search key: " << key;
    BNode* v = _root;
    _hot_idx.reserve(1024);
    while (v) {
        auto _v_beg = v->key.begin();
        auto _v_end = v->key.end();
        auto _hot_iter = upper_bound(_v_beg, _v_end, key);  // return the first el > key
        size_t tmp_idx = _hot_iter - _v_beg;

        _hot = v;
        v = v->child[tmp_idx];
        
        _hot_idx.emplace_back(tmp_idx);
    }

    if (_hot_idx.back() > 0 && _hot->key[_hot_idx.back() - 1] == key) {
        _hot_idx.back() -= 1;  // for leaf idx, return the first el == key.
        size_t index = _hot->data[_hot_idx.back()];
        // LOG(INFO) << "find (key, data): " << key << ", " << index;
        return index;  // find the key
    }
    else {
        // LOG(INFO) << "not found";
        return (size_t)-1;
    }
}

size_t BTree::search(const Key& key) {
    BNode* _hot = nullptr;
    std::vector<size_t> _hot_idx;
    return search(key, _hot, _hot_idx);
}

/*
 * return 1: key not exist, insert success!
 * return 0: key exist, modify success!
 * return -1: error 
 */
int BTree::insert(const Key &key, const size_t data)
{
    BNode* _hot = nullptr;
    std::vector<size_t> _hot_idx;
    size_t _find = search(key, _hot, _hot_idx);
    if (_hot == nullptr) {  // 
        LOG(FATAL) << "b+tree's root is nullptr";
    }
    
    // std::string log_out("===== [");
    // for (auto iter = _hot->key.begin(); iter != _hot->key.end(); iter++) {
    //     log_out.append(iter->data());
    //     log_out.append(", ");
    // }
    // log_out.append("] begin to insert: ");
    // LOG(INFO) << log_out << key << ", " << data;
    if (_find == (size_t)-1) {
        if (!_hot->is_full()) {
            size_t lf_idx = _hot_idx.back();
            _hot->key.emplace(_hot->key.begin() + lf_idx, key);
            _hot->data.emplace(_hot->data.begin() + lf_idx, data);
            _hot->child.emplace_back(nullptr);
        }
        else {
            solve_overflow_leaf(key, data, _hot, _hot_idx);
        }
        _size += 1;
        // show_tree();
        return 1;
    }
    else {
        // modify
        _hot->data[_hot_idx.back()] = data;
        // show_tree();
        return 0;
    }
}

/*
 * return 1: key exist, remove success!
 * return 0: key not found, remove failed!
 * return -1: error 
 */
int BTree::remove(const Key &key)
{
    BNode* _hot = nullptr;
    std::vector<size_t> _hot_idx;
    size_t _find = search(key, _hot, _hot_idx);
    if (_find == (size_t)-1) {
        return 0;
    }
    else {
        // remove the key
        // std::string log_out("===== [");
        // for (auto iter = _hot->key.begin(); iter != _hot->key.end(); iter++) {
        //     log_out.append(iter->data());
        //     log_out.append(", ");
        // }
        // log_out.append("] begin to remove: ");
        // LOG(INFO) << log_out << key;
        size_t lf_idx = _hot_idx.back();
        if (_hot != _root && _hot->key.size() <= ((M + 1) / 2) - 1) {
            solve_underflow_leaf(_hot, _hot_idx);
        }
        else {
            _hot->key.erase(_hot->key.begin() + lf_idx);
            _hot->data.erase(_hot->data.begin() + lf_idx);
            _hot->child.pop_back();
            // update parent (not neccesary)
            if (_hot != _root && lf_idx == 0) {
                size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
                if (pa_idx > 0)
                    _hot->parent->key[pa_idx - 1] = _hot->key.front();
            }
        }
        _size -= 1;
        // show_tree();
        return 1;
    }
}


void BTree::solve_overflow_leaf(const Key& key, const size_t data, BNode*& _hot, std::vector<size_t>& _hot_idx)
{
    // LOG(INFO) << "=============== begin solve_overflow_leaf";
    size_t lf_idx = _hot_idx[_hot_idx.size() - 1];
    if (_hot->prev && _hot->parent == _hot->prev->parent && !_hot->prev->is_full()) {  // not root, not leftmost node, must be leaf
        // LOG(INFO) << "rotate 1";
        // give out a child from _hot to _hot->prev
        _hot->prev->key.emplace_back(_hot->key.front().c_str());
        _hot->prev->data.emplace_back(_hot->data.front());
        _hot->prev->child.emplace_back(nullptr);

        // insert the key and data into _hot
        for (size_t i = 0; i < lf_idx - 1; i++) {
            _hot->key[i] = _hot->key[i + 1];
            _hot->data[i] = _hot->data[i + 1];
        }
        _hot->key[lf_idx - 1] = key;
        _hot->data[lf_idx - 1] = data;

        // update parent's key
        size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
        _hot->parent->key[pa_idx - 1] = _hot->key.front();

        // show_tree();
    }
    else if (_hot->next && _hot->parent == _hot->next->parent && !_hot->next->is_full()) {  // not root, not rightmost node, must be leaf
        // LOG(INFO) << "rotate 2";
        size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
        if (lf_idx == _hot->key.size()) {
            // directly insert to _hot->next
            _hot->next->key.emplace(_hot->next->key.begin(), key.c_str());
            _hot->next->data.emplace(_hot->next->data.begin(), data);
            _hot->next->child.emplace_back(nullptr);

            // update parent's key
            _hot->parent->key[pa_idx] = key;
        }
        else {
            // give out a child from _hot to _hot->next
            _hot->next->key.emplace(_hot->next->key.begin(), _hot->key.back().c_str());
            _hot->next->data.emplace(_hot->next->data.begin(), _hot->data.back());
            _hot->next->child.emplace_back(nullptr);

            // update parent's key
            _hot->parent->key[pa_idx] = _hot->key.back();

            // insert the key and data into _hot
            for (size_t i = _hot->key.size() - 1; i > lf_idx; i--) {
                _hot->key[i] = _hot->key[i - 1];
                _hot->data[i] = _hot->data[i - 1];
            }
            _hot->key[lf_idx] = key;
            _hot->data[lf_idx] = data;
        }
        // show_tree();
    }
    else {
        // LOG(INFO) << "split";
        // prepare for the idx
        for (size_t i = 0; i < lf_idx; i++) _tmp_idx[i] = i;
        _tmp_idx[lf_idx] = (size_t)-1;
        for (size_t i = lf_idx + 1; i < M; i++) _tmp_idx[i] = i - 1;
        
        // create new node
        BNode* u = new BNode(_hot->parent, true);
        u->prev = _hot; u->next = _hot->next;
        if (_hot->next) _hot->next->prev = u;
        _hot->next = u;

        u->child.resize(M - (M / 2) + 1, nullptr);
        for (size_t i = M / 2; i < M; i++) {
            size_t ti = _tmp_idx[i];
            if (ti == (size_t)-1) {
                u->key.emplace_back(key.c_str());
                u->data.emplace_back(data);
            }
            else {
                u->key.emplace_back(_hot->key[ti].c_str());
                u->data.emplace_back(_hot->data[ti]);
            }
        }

        // update _hot
        for (size_t i = (M / 2) - 1; i != (size_t)-1; i--) {
            size_t ti = _tmp_idx[i];
            if (ti == i) break;
            else if (ti == (size_t)-1) {
                _hot->key[i] = key;
                _hot->data[i] = data;
                break;
            }
            else{
                _hot->key[i] = _hot->key[ti];
                _hot->data[i] = _hot->data[ti];
            }
        }
        _hot->key.resize(M / 2);  // delete the rest elements
        _hot->data.resize(M / 2);
        _hot->child.resize((M / 2) + 1);

        // update parents
        if (_hot->parent == nullptr) { // must be root
            BNode* p = new BNode(nullptr, false);
            p->key.emplace_back(u->key.front().c_str());
            p->child.emplace_back(_hot);
            p->child.emplace_back(u);
            _hot->parent = p;
            u->parent = p;
            this->_root = p;
        }
        else {
            BNode* p = _hot->parent;
            if (!p->is_full()) {
                size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
                p->key.emplace(p->key.begin() + pa_idx, u->key.front().c_str());
                p->child.emplace(p->child.begin() + pa_idx + 1, u);
            }
            else {
                _hot_idx.pop_back();
                solve_overflow_innode(u->key.front(), u, p, _hot_idx);
            }
        }
        // show_tree();
    }
}


void BTree::solve_overflow_innode(const Key& key, BNode* _c, BNode*& _hot, std::vector<size_t>& _hot_idx)
{
    // LOG(INFO) << "=============== begin solve_overflow_innode";
    size_t lf_idx = _hot_idx[_hot_idx.size() - 1];
    // prepare for the idx
    for (size_t i = 0; i < lf_idx; i++) _tmp_idx[i] = i;
    _tmp_idx[lf_idx] = (size_t)-1;
    for (size_t i = lf_idx + 1; i < M; i++) _tmp_idx[i] = i - 1;
    
    // create new node
    BNode* u = new BNode(_hot->parent, false);
    size_t mi = _tmp_idx[(M / 2)];
    Key mi_key;
    if (mi == (size_t)-1) {
        u->child.emplace_back(_c);
        _c->parent = u;
        mi_key = key;
    }
    else {
        u->child.emplace_back(_hot->child[mi + 1]);
        u->child.back()->parent = u;
        mi_key = _hot->key[mi];
    }
    for (size_t i = (M / 2) + 1; i < M; i++) {
        size_t ki = _tmp_idx[i];
        if (ki == (size_t)-1) {
            u->key.emplace_back(key.c_str());
            u->child.emplace_back(_c);
            _c->parent = u;
        }
        else {
            u->key.emplace_back(_hot->key[ki].c_str());
            u->child.emplace_back(_hot->child[ki + 1]);
            u->child.back()->parent = u;
        }
    }

    // update _hot
    for (size_t i = (M / 2) - 1; i != (size_t)-1; i--) {
        size_t ti = _tmp_idx[i];
        if (ti == i) break;
        else if (ti == (size_t)-1) {
            _hot->key[i] = key;
            _hot->child[i + 1] = _c;
            break;
        }
        else {
            _hot->key[i] = _hot->key[ti];
            _hot->child[i + 1] = _hot->child[ti + 1];
        }
    }
    _hot->key.resize(M / 2);  // delete the rest elements
    _hot->child.resize((M / 2) + 1);

    // update parents
    if (_hot->parent == nullptr) { // must be root
        BNode* p = new BNode(nullptr, false);
        p->key.emplace_back(mi_key.c_str());
        p->child.emplace_back(_hot);
        p->child.emplace_back(u);
        _hot->parent = p;
        u->parent = p;
        this->_root = p;
    }
    else {
        size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
        BNode* p = _hot->parent;
        if (!p->is_full()) {
            p->key.emplace(p->key.begin() + pa_idx, mi_key.c_str());
            p->child.emplace(p->child.begin() + pa_idx + 1, u);
        }
        else {
            _hot_idx.pop_back();
            solve_overflow_innode(mi_key, u, p, _hot_idx);
        }
    }
}


void BTree::solve_underflow_leaf(BNode*& _hot, std::vector<size_t>& _hot_idx)  // _hot must not root
{
    // LOG(INFO) << "=============== begin solve_underflow_leaf";
    size_t rm_idx = _hot_idx.back();
    size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
    BNode* p = _hot->parent;
    BNode* prev = _hot->prev;
    BNode* next = _hot->next;
    if (pa_idx > 0 && prev->key.size() >= ((M + 1) / 2)) {
        // _hot remove and borrow a smallest key from _hot->prev
        // LOG(INFO) << "rotate 1";
        for (size_t i = rm_idx; i >= 1; i--) {
            _hot->key[i] = _hot->key[i - 1];
            _hot->data[i] = _hot->data[i - 1];
        }
        _hot->key[0] = prev->key.back();
        _hot->data[0] = prev->data.back();

        // update _hot->prev
        prev->key.pop_back();
        prev->data.pop_back();
        prev->child.pop_back();

        // update parent
        p->key[pa_idx - 1] = _hot->key.front();
    }
    else if (pa_idx < p->child.size() - 1 && next->key.size() >= ((M + 1) / 2)) {
        // _hot remove and borrow a biggest key from _hot->next
        // LOG(INFO) << "rotate 2";
        for (size_t i = rm_idx; i < _hot->key.size() - 1; i++) {
            _hot->key[i] = _hot->key[i + 1];
            _hot->data[i] = _hot->data[i + 1];
        }
        _hot->key[_hot->key.size() - 1] = next->key.front();
        _hot->data[_hot->data.size() - 1] = next->data.front();

        // update _hot->next
        next->key.erase(_hot->next->key.begin());
        next->data.erase(_hot->next->data.begin());
        next->child.pop_back();

        // update parent (not neccesary)
        size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
        p->key[pa_idx] = next->key.front();
        if (pa_idx > 0 && rm_idx == 0) {
            p->key[pa_idx - 1] = _hot->key.front();
        }
    }
    else if (pa_idx > 0) {
        // _hot remove and merge with _hot->prev
        // LOG(INFO) << "merge 1";
        for (size_t i = 0; i < _hot->key.size(); i++) {
            if (i == rm_idx) continue;
            prev->key.emplace_back(_hot->key[i].c_str());
            prev->data.emplace_back(_hot->data[i]);
            prev->child.emplace_back(nullptr);
        }
        prev->next = _hot->next;
        if (_hot->next) _hot->next->prev = prev;

        // delete _hot
        delete _hot;
        
        // update parent
        if (p != _root && p->key.size() <= ((M + 1) / 2) - 1)
        {
            _hot_idx.pop_back();
            _hot_idx.back() -= 1;
            solve_underflow_innode(p, _hot_idx);
        }
        else if (p == _root && p->key.size() <= 1) {
            prev->parent = nullptr;
            _root = prev;
            delete p;
        }
        else {
            p->key.erase(p->key.begin() + pa_idx - 1);
            p->child.erase(p->child.begin() + pa_idx);
        }
    }
    else if (pa_idx < p->child.size() - 1) {
        // _hot merge with _hot->next
        // LOG(INFO) << "merge 2";
        for (size_t i = rm_idx; i < _hot->key.size() - 1; i++) {
            _hot->key[i] = _hot->key[i + 1];
            _hot->data[i] = _hot->data[i + 1];
        }
        _hot->key[_hot->key.size() - 1] = next->key.front();
        _hot->data[_hot->data.size() - 1] = next->data.front();
        for (size_t i = 1; i < next->key.size(); i++) {
            _hot->key.emplace_back(next->key[i].c_str());
            _hot->data.emplace_back(next->data[i]);
            _hot->child.emplace_back(nullptr);
        }
        _hot->next = next->next;
        if (next->next) next->next->prev = _hot;

        // delete next
        delete next;

        // update parent
        if (p != _root && p->key.size() <= ((M + 1) / 2) - 1)
        {
            _hot_idx.pop_back();
            solve_underflow_innode(p, _hot_idx);
        }
        else if (p == _root && p->key.size() <= 1) {
            _hot->parent = nullptr;
            _root = _hot;
            delete p;
        }
        else {
            p->key.erase(p->key.begin() + pa_idx);
            p->child.erase(p->child.begin() + pa_idx + 1);
        }
    }
    else {
        LOG(FATAL) << "undefined case in solve_underflow_leaf";
    }
}


void BTree::solve_underflow_innode(BNode*& _hot, std::vector<size_t>& _hot_idx)  // _hot must not root
{
    // LOG(INFO) << "=============== begin solve_underflow_innode";
    size_t rm_idx = _hot_idx.back();
    size_t pa_idx = _hot_idx[_hot_idx.size() - 2];
    BNode* p = _hot->parent;
    if (pa_idx > 0 && p->child[pa_idx - 1]->key.size() >= ((M + 1) / 2)) {
        // LOG(INFO) << "rotate 11";
        BNode* prev = p->child[pa_idx - 1];
        // _hot remove and borrow a smallest key from prev
        for (size_t i = rm_idx; i >= 1; i--) {
            _hot->key[i] = _hot->key[i - 1];
            _hot->child[i + 1] = _hot->child[i];
        }
        _hot->child[1] = _hot->child[0];
        _hot->child[0] = prev->child.back();
        _hot->child.front()->parent = _hot;
        if (_hot->child.front()->is_leaf)
            _hot->key[0] = _hot->child[1]->key.front();
        else {
            _hot->key[0] = p->key[pa_idx - 1];
        }

        // update p
        p->key[pa_idx - 1] = prev->key.back();

        // update prev
        prev->key.pop_back();
        prev->child.pop_back();
    }
    else if (pa_idx < p->child.size() - 1 && p->child[pa_idx + 1]->key.size() >= ((M + 1) / 2)) {
        // LOG(INFO) << "rotate 12";
        BNode* next = p->child[pa_idx + 1];
        // _hot remove and borrow a biggest key from _hot->next
        for (size_t i = rm_idx; i < _hot->key.size() - 1; i++) {
            _hot->key[i] = _hot->key[i + 1];
            _hot->child[i + 1] = _hot->child[i + 2];
        }
        _hot->child[_hot->child.size() - 1] = next->child.front();
        _hot->child.back()->parent = _hot;
        if (_hot->child.back()->is_leaf) {
            _hot->key[_hot->key.size() - 1] = _hot->child.back()->key.front();
        }
        else {
            _hot->key[_hot->key.size() - 1] = p->key[pa_idx];
        }

        // update p
        p->key[pa_idx] = next->key.front();

        // update next
        next->key.erase(next->key.begin());
        next->child.erase(next->child.begin());
    }
    else if (pa_idx > 0) {
        // LOG(INFO) << "merge 11";
        BNode* prev = p->child[pa_idx - 1];
        // _hot remove and merge with _hot->prev
        prev->child.emplace_back(_hot->child.front());
        prev->child.back()->parent = prev;
        if (prev->child.back()->is_leaf) {
            prev->key.emplace_back(prev->child.back()->key.front().c_str());
        }
        else {
            prev->key.emplace_back(p->key[pa_idx - 1]);
        }
        
        for (size_t i = 0; i < _hot->key.size(); i++) {
            if (i == rm_idx) continue;
            prev->key.emplace_back(_hot->key[i].c_str());
            prev->child.emplace_back(_hot->child[i + 1]);
            prev->child.back()->parent = prev;
        }

        // delete _hot
        delete _hot;

        // update parent
        if (p != _root && p->key.size() <= ((M + 1) / 2) - 1)
        {
            _hot_idx.pop_back();
            _hot_idx.back() -= 1;
            solve_underflow_innode(p, _hot_idx);
        }
        else if (p == _root && p->key.size() <= 1) {
            prev->parent = nullptr;
            _root = prev;
            delete p;
        }
        else {
            p->key.erase(p->key.begin() + pa_idx - 1);
            p->child.erase(p->child.begin() + pa_idx);
        }

    }
    else if (pa_idx < p->child.size() - 1) {
        // LOG(INFO) << "merge 12";
        BNode* next = p->child[pa_idx + 1];
        // _hot merge with _hot->next
        for (size_t i = rm_idx; i < _hot->key.size() - 1; i++) {
            _hot->key[i] = _hot->key[i + 1];
            _hot->child[i + 1] = _hot->child[i + 2];
        }
        _hot->child[_hot->child.size() - 1] = next->child.front();
        _hot->child.back()->parent = _hot;
        if (_hot->child.back()->is_leaf) {
            _hot->key[_hot->key.size() - 1] = _hot->child.back()->key.front();
        }
        else {
            _hot->key[_hot->key.size() - 1] = p->key[pa_idx];
        }
        
        for (size_t i = 0; i < next->key.size(); i++) {
            _hot->key.emplace_back(next->key[i].c_str());
            _hot->child.emplace_back(next->child[i + 1]);
            _hot->child.back()->parent = _hot;
        }

        // delete next
        delete next; 

        // update parent
        if (p != _root && p->key.size() <= ((M + 1) / 2) - 1)
        {
            _hot_idx.pop_back();
            solve_underflow_innode(p, _hot_idx);
        }
        else if (p == _root && p->key.size() <= 1) {
            _hot->parent = nullptr;
            _root = _hot;
            delete p;
        }
        else {
            p->key.erase(p->key.begin() + pa_idx);
            p->child.erase(p->child.begin() + pa_idx + 1);
        }
    }
    else {
        LOG(FATAL) << "undefined case in solve_underflow_leaf";
    }
}

}