#include "engine.h"

namespace kvs
{
Engine::Engine(const std::string &path, EngineOptions options) :
    log_offset(0)
{
    // TODO: your code here
    log_filename = std::string(path + "/log");
    if (access(log_filename.c_str(), F_OK) == 0) {  // log file exist, after crash
        // rebuild the B+tree indexing
        rebuild_btree(log_filename);
    }

    std::ignore = options;
}

Engine::~Engine()
{
    // TODO: your code here
    // LOG(INFO) << "~Engine";
}

void Engine::rebuild_btree(const std::string& log_filename)
{
    // LOG(INFO) << "rebuild_btree(" << log_filename << ")";
    {
        std::lock_guard<std::shared_mutex> w_lock(_mutex);
        FILE* crash_fp = fopen(log_filename.c_str(), "r");
        if (!crash_fp) LOG(FATAL) << "cannot open log file to rebuild indexing" << log_filename;
        fseek(crash_fp, 0, SEEK_SET);

        std::string key, value;
        int ret = read_key_value(crash_fp, key, value);
        while (ret != -1) {
            if (ret == 0) {
                btree.remove(key);
                log_offset += (sizeof(size_t) * 2 + key.size());
            }
            else {
                btree.insert(key, log_offset);
                log_offset += (sizeof(size_t) * 2 + key.size() + value.size());
            }
            key.clear();
            value.clear();
            ret = read_key_value(crash_fp, key, value);
        }
        fclose(crash_fp);
    }
}

void Engine::log_line(FILE*& fp, const std::string& line)
{
    size_t s = 0;
    while (s < line.size()) {
        size_t len = 8092;
        if (line.size() - s < 8092) len = line.size() - s;
        size_t n = fwrite(line.c_str() + s, 1, len, fp);
        s += n;
    }
    if (s > line.size()) {
        LOG(FATAL) << "s != line.size(), s = " << s << ", line.size() = " << line.size();
    }
}

void Engine::read_line(FILE*& fp, std::string& line)
{
    char buf[16];
    size_t k_size, v_size, n;

    n = fread(buf, 1, sizeof(size_t), fp);
    if (n != sizeof(size_t)) return;
    k_size = *(size_t *)(buf);
    line.append(buf, sizeof(size_t));

    n = fread(buf, 1, sizeof(size_t), fp);
    if (n != sizeof(size_t)) return;
    v_size = *(size_t *)(buf);
    line.append(buf, sizeof(size_t));

    size_t total = k_size;
    if (v_size != (size_t)-1) total += v_size;
    read_num(fp, total, line);

    // while (fgets(buf, sizeof(buf), fp)) {
    //     line.append(buf);
    //     if (line.back() == '\n') {
    //         line.pop_back();
    //         break;
    //     }
    // }
}

int Engine::read_key_value(FILE*& fp, Key& key, Value& value)
{
    char buf[16];
    size_t k_size, v_size, n;

    n = fread(buf, 1, sizeof(size_t), fp);
    if (n != sizeof(size_t)) return -1;
    k_size = *(size_t *)(buf);

    n = fread(buf, 1, sizeof(size_t), fp);
    if (n != sizeof(size_t)) return -1;
    v_size = *(size_t *)(buf);
        
    if (v_size != (size_t)-1) {  // insert
        if (!read_num(fp, k_size, key)) return -1;
        if (!read_num(fp, v_size, value)) return -1;
        return 1;
    }
    else {  // remove
        if (!read_num(fp, k_size, key)) return -1;
        return 0;
    }
}

bool Engine::read_num(FILE*&fp, size_t num, std::string& line)
{
    char buf[8092];
    size_t s = 0;
    while (s < num) {
        size_t len = 8092;
        if (num - s < 8092) len = num - s;
        size_t n = fread(buf, 1, len, fp);
        if (n < len) return false;
        line.append(buf, n);
        s += n;
    }
    return true;
}


RetCode Engine::put(const Key &key, const Value &value)
{
    // TODO: your code here
    // LOG(INFO) << "put(" << key << ", " << value << ")";
    std::string line;
    size_t k_size = key.size();
    size_t v_size = value.size();
    line.append((char*)&k_size, sizeof(size_t));
    line.append((char*)&v_size, sizeof(size_t));
    line.append(key);
    line.append(value);

    std::lock_guard<std::shared_mutex> w_lock(_mutex);
    log_fp = fopen(log_filename.c_str(), "a+");
    if (!log_fp) LOG(FATAL) << "cannot open and read the file: " << log_filename;
    log_line(log_fp, line);
    // fsync(fileno(log_fp));
    fclose(log_fp);

    btree.insert(key, log_offset);
    log_offset += line.size();
    return kSucc;
}
RetCode Engine::remove(const Key &key)
{
    // TODO: your code here
    // LOG(INFO) << "remove(" << key << ")";
    std::string line;
    size_t k_size = key.size();
    size_t v_size = (size_t)(-1);
    line.append((char*)&k_size, sizeof(size_t));
    line.append((char*)&v_size, sizeof(size_t));
    line.append(key);
    
    std::lock_guard<std::shared_mutex> w_lock(_mutex);
    if (btree.remove(key) == 0) {
        return kNotFound;
    }
    else {
        log_fp = fopen(log_filename.c_str(), "a+");
        if (!log_fp) LOG(FATAL) << "cannot open and read the file: " << log_filename;
        log_line(log_fp, line);
        // fsync(fileno(log_fp));
        fclose(log_fp);

        log_offset += line.size();
        return kSucc;
    }
}
RetCode Engine::get(const Key &key, Value &value)
{
    // TODO: your code here
    // LOG(INFO) << "get(" << key << ")";

    std::shared_lock<std::shared_mutex> r_lock(_mutex);
    std::shared_lock<std::shared_mutex> r_lock2(_mutex2);
    size_t index = btree.search(key);
    if (index == (size_t)-1) return kNotFound;
    else {
        FILE* r_fp = fopen(log_filename.c_str(), "r");
        fseek(r_fp, index, SEEK_SET);

        Key tmp;
        value.clear();
        int ret = read_key_value(r_fp, tmp, value);
        if (ret != 1) LOG(FATAL) << "cannot get value from log: " << key;
        fclose(r_fp);
        return kSucc;
    }
    
}

RetCode Engine::sync()
{
    // TODO: your code here
    std::lock_guard<std::shared_mutex> w_lock(_mutex);
    log_fp = fopen(log_filename.c_str(), "a+");
    if (fsync(fileno(log_fp)) == 0) {
        fclose(log_fp);
        return kSucc;
    }
    else {
        fclose(log_fp);
        return kIOError;
    }
}

RetCode Engine::visit(const Key &lower,
                      const Key &upper,
                      const Visitor &visitor)
{
    // TODO: your code here
    // LOG(INFO) << "visit(" << lower << ", " << upper << ")";

    std::shared_lock<std::shared_mutex> r_lock(_mutex);
    std::shared_lock<std::shared_mutex> r_lock2(_mutex2);
    BTree::BNode* _hot = nullptr;
    std::vector<size_t> _hot_idx;
    btree.search(lower, _hot, _hot_idx);

    BTree::BNode* v = _hot;
    size_t idx = _hot_idx.back();
    bool end = false;

    FILE* r_fp = fopen(log_filename.c_str(), "r");
    while (v != nullptr) {
        while (idx < v->key.size()) {
            if (v->key[idx] > upper) {
                end = true;
                break;
            }
            fseek(r_fp, v->data[idx], SEEK_SET);
            std::string key, value;
            int ret = read_key_value(r_fp, key, value);
            if (ret != 1) LOG(FATAL) << "cannot get value from log: " << key;

            visitor(key, value);
            idx += 1;
        }
        if (end) break;
        v = v->next;
        idx = 0;
    }
    fclose(r_fp);
    return kSucc;
}

RetCode Engine::garbage_collect()
{
    // TODO: your code here
    // LOG(INFO) << "garbage_collect";
    auto f = std::async(&Engine::_gc, this);
    return kSucc;
}

void Engine::_gc()
{
    // create another new log file
    std::lock_guard<std::mutex> gc_lock(_mutex_gc);
    std::shared_lock<std::shared_mutex> r_lock(_mutex);
    std::vector<size_t> _offset;
    _offset.reserve(btree.size() + 1);

    BTree::BNode* first_c = btree.get_root();
    while (!first_c->is_leaf) first_c = first_c->child[0];
    BTree::BNode* v = first_c;

    std::string log_filename1(log_filename + "(1)");
    FILE* r_fp = fopen(log_filename.c_str(), "r");
    FILE* w_fp = fopen(log_filename1.c_str(), "a");
    size_t off = 0;
    while (v != nullptr) {
        for (size_t i = 0; i < v->key.size(); i++) {
            fseek(r_fp, v->data[i], SEEK_SET);
            std::string line;
            read_line(r_fp, line);
            log_line(w_fp, line);

            // v->data[i] = off;
            _offset.emplace_back(off);
            off += line.size();
        }
        v = v->next;
    }
    _offset.emplace_back(off);
    fclose(w_fp);
    fclose(r_fp);

    // update btree indexing
    std::lock_guard<std::shared_mutex> w_lock2(_mutex2);
    v = first_c;
    size_t _off_idx = 0;
    while (v != nullptr) {
        for (size_t i = 0; i < v->key.size(); i++) {
            v->data[i] = _offset[_off_idx];
            _off_idx += 1;
        }
        v = v->next;
    }

    // delete old log and rename new log
    std::remove(log_filename.c_str());
    std::rename(log_filename1.c_str(), log_filename.c_str());
    log_offset = _offset.back();
}

std::shared_ptr<IROEngine> Engine::snapshot()
{
    // TODO: your code here
    return nullptr;
}

}  // namespace kvs
