#include <iostream> 
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <fstream>
#include <sstream>

#define STORE_FILE "store/dumpFile"

std::mutex mtx;     // mutex for critical section
char delimiter = ':';

//Class template to implement node
template<typename K, typename V> 
class Node {

public:
    
    Node() {} 

    Node(K k, V v, int); 

    ~Node();

    K get_key() const;

    V get_value() const;

    void set_value(V);
    
    // Linear array to hold pointers to next node of different level
    Node<K, V> **forward; //指针数组：保存各个level下一个结点的指针

    int node_level; //结点层级

private:
    K key;
    V value;
};

template<typename K, typename V> 
Node<K, V>::Node(const K k, const V v, int level) {
    this->key = k;
    this->value = v;
    this->node_level = level; 

    // level + 1, because array index is from 0 - level
    this->forward = new Node<K, V>*[level+1];
    
	// Fill forward array with 0(NULL) 初始化结点的指针数组，forward[i]表示当前结点第i层的下一个结点
    memset(this->forward, 0, sizeof(Node<K, V>*)*(level+1));
};

template<typename K, typename V> 
Node<K, V>::~Node() {
    delete []forward;
};

template<typename K, typename V> 
K Node<K, V>::get_key() const {
    return key;
};

template<typename K, typename V> 
V Node<K, V>::get_value() const {
    return value;
};
template<typename K, typename V> 
void Node<K, V>::set_value(V value) {
    this->value=value;
};

// Class template for Skip list
template <typename K, typename V> 
class SkipList {

public: 
    SkipList(int);
    ~SkipList();
    int get_random_level();
    Node<K, V>* create_node(K, V, int);
    int insert_element(K, V);
    void display_list();
    bool search_element(K);
    void delete_element(K);
    void dump_file();
    void load_file();
    int size();

private:
    void get_key_value_from_string(const std::string &str, K &key, V &value);
    bool is_valid_string(const std::string& str);

private:    
    // Maximum level of the skip list 
    int _max_level; //允许的最大层数，如果随机到的层数大于这个数，则返回的还是设置的最大层数

    // current level of skip list 
    int _skip_list_level; //当前跳表的最高层数

    // pointer to header node 
    Node<K, V> *_header; //头结点，不保存数据，保存的是各层级的头指针

    // file operator 写入读取 文件操作符
    std::ofstream _file_writer;
    std::ifstream _file_reader;

    // skiplist current element count
    int _element_count;
};

// create new node 
template<typename K, typename V>
Node<K, V>* SkipList<K, V>::create_node(const K k, const V v, int level) {
    Node<K, V> *n = new Node<K, V>(k, v, level);
    return n;
}

// Insert given key and value in skip list 
// return 1 means element exists  
// return 0 means insert successfully
/* 
                            +------------+
                            |  insert 50 |
                            +------------+
level 4     +-->1+ --->xx                                                100
                 |
                 |                       insert +----+
level 3         1+-------->10--->xx             | 50 |          70       100
                            |                   |    |
                            |                   |    |
level 2         1          10+------->30 --->xx | 50 |          70       100
                                       |        |    |
                                       |        |    |
level 1         1    4     10         30 --->xx | 50 |          70       100
                                       |        |    |
                                       |        |    |
level 0         1    4   9 10         30+->40+->| 50 |  60      70       100
                                                +----+

*/
template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value) {
    
    mtx.lock();
    Node<K, V> *current = this->_header;

    // create update array and initialize it 
    // update is array which put node that the node->forward[i] should be operated later
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));  

    // start form highest level of skip list 
    for(int i = _skip_list_level; i >= 0; i--) {
        while(current->forward[i] != NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i]; //查询当前层的后续结点
        }
        update[i] = current; //保存插入结点第i层的前一个结点，后续如果随机的层级小于则设为无效？
    }

    // reached level 0 and forward pointer to right node, which is desired to insert key.
    current = current->forward[0];

    // if current node have key equal to searched key, we get it
    if (current != NULL && current->get_key() == key) {
        std::cout << "key: " << key << ", exists" << std::endl;
        mtx.unlock();
        return 1;
    }

    // if current is NULL that means we have reached to end of the level 
    // if current's key is not equal to key that means we have to insert node between update[0] and current node 
    if (current == NULL || current->get_key() != key ) {
        
        // Generate a random level for node
        int random_level = get_random_level(); //随机层级返回值不会超过设置的最高，可能会超过当前最高

        // If random level is greater thar skip list's current level, initialize update value with pointer to header
        if (random_level > _skip_list_level) {
            //如果随机的层级大于现有的最高层级，那么更新update长度并初始化
            //？怎么动态扩充的
            for (int i = _skip_list_level+1; i < random_level+1; i++) {
                update[i] = _header;
            }
            _skip_list_level = random_level;
        }

        // create new node with random level generated 
        Node<K, V>* inserted_node = create_node(key, value, random_level);
        
        // insert node 
        for (int i = 0; i <= random_level; i++) {
            inserted_node->forward[i] = update[i]->forward[i]; //<==>  node->next = pre->next;
            update[i]->forward[i] = inserted_node; // <==>  pre->next = node;
        }
        std::cout << "Successfully inserted key:" << key << ", value:" << value << std::endl;
        _element_count ++;
    }
    mtx.unlock();
    return 0;
}

// Display skip list 
template<typename K, typename V> 
void SkipList<K, V>::display_list() {

    std::cout << "\n*****Skip List*****"<<"\n"; 
    for (int i = 0; i <= _skip_list_level; i++) {//从0级开始打印
        Node<K, V> *node = this->_header->forward[i]; 
        std::cout << "Level " << i << ": ";
        while (node != NULL) {
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

// Dump data in memory to file 
template<typename K, typename V> 
void SkipList<K, V>::dump_file() {

    std::cout << "dump_file-----------------" << std::endl;
    _file_writer.open(STORE_FILE);
    Node<K, V> *node = this->_header->forward[0]; 

    while (node != NULL) {
        _file_writer << node->get_key() << delimiter << node->get_value() << "\n";
        std::cout << node->get_key() << delimiter << node->get_value() << ";\n";
        node = node->forward[0];
    }

    _file_writer.flush(); //刷新缓冲区
    _file_writer.close();
    return ;
}

// Load data from disk
template<typename K, typename V> 
void SkipList<K, V>::load_file() {

    _file_reader.open(STORE_FILE);
    std::cout << "load_file-----------------" << std::endl;
    std::string line;
    K key;
    V value;
    std::string token;
    while (getline(_file_reader, line)) {
        get_key_value_from_string(line, key, value);
        insert_element(key, value); //bug:此时传入的为string类型 已修复
        std::cout << "key:" << key << "value:" << value << std::endl;
    }
    _file_reader.close();
}
// template<typename K, typename V> 
// void SkipList<K, V>::load_file() {

//     _file_reader.open(STORE_FILE);
//     std::cout << "load_file-----------------" << std::endl;
//     std::string line;
//     K key;
//     V value;
//     while (getline(_file_reader, line)) {
//         std::stringstream ss(line);
//         if(!(ss >> key >> value)){
//             continue;
//         }
//         insert_element(key, value); //bug:此时传入的为string类型 已修复
//         std::cout << "key:" << key << "value:" << value << std::endl;
//     }
//     _file_reader.close();
// }
// Get current SkipList size 
template<typename K, typename V> 
int SkipList<K, V>::size() { 
    return _element_count;
}

template<typename K, typename V> //已弃用
void SkipList<K, V>::get_key_value_from_string(const std::string& str, K& key, V& value) {
    //将从磁盘中读取到的字符串行 分割转换成键值对
    if(!is_valid_string(str)) {
        return;
    }
    std::string token;
    token = str.substr(0, str.find(delimiter));
    if(!token.empty()){
        std::stringstream ss(token);
        ss >> key;
    }
    token = str.substr(str.find(delimiter)+1, str.length());
    if(!token.empty()){
        std::stringstream ss(token);
        ss >> value;
    }
}

template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {

    if (str.empty()) {
        return false;
    }
    if (str.find(delimiter) == std::string::npos) {
        return false;
    }
    return true;
}

// Delete element from skip list 
template<typename K, typename V> 
void SkipList<K, V>::delete_element(K key) {

    mtx.lock();
    Node<K, V> *current = this->_header; 
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));

    // start from highest level of skip list
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] !=NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];
    if (current != NULL && current->get_key() == key) {
       
        // start for lowest level and delete the current node of each level
        for (int i = 0; i <= _skip_list_level; i++) {

            // if at level i, next node is not target node, break the loop.
            if (update[i]->forward[i] != current) 
                break;

            update[i]->forward[i] = current->forward[i]; // pre->next = cur->next;
        }

        // Remove levels which have no elements 删除空的层级
        while (_skip_list_level > 0 && _header->forward[_skip_list_level] == 0) {
            _skip_list_level --; 
        }

        std::cout << "Successfully deleted key "<< key << std::endl;
        delete current; //释放内存
        _element_count --;
    }
    mtx.unlock();
    return;
}

// Search for element in skip list 
/*
                           +------------+
                           |  select 60 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |
level 3         1+-------->10+------------------>50+           70       100
                                                   |
                                                   |
level 2         1          10         30         50|           70       100
                                                   |
                                                   |
level 1         1    4     10         30         50|           70       100
                                                   |
                                                   |
level 0         1    4   9 10         30   40    50+-->60      70       100
*/
template<typename K, typename V> 
bool SkipList<K, V>::search_element(K key) {

    std::cout << "search_element-----------------" << std::endl;
    Node<K, V> *current = _header;

    // start from highest level of skip list
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }

    //reached level 0 and advance pointer to right node, which we search
    current = current->forward[0];

    // if current node have key equal to searched key, we get it
    if (current and current->get_key() == key) {
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }

    std::cout << "Not Found Key:" << key << std::endl;
    return false;
}

// construct skip list
template<typename K, typename V> 
SkipList<K, V>::SkipList(int max_level) {

    this->_max_level = max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;

    // create header node and initialize key and value to null
    K k;
    V v;
    this->_header = new Node<K, V>(k, v, _max_level);
};

template<typename K, typename V> 
SkipList<K, V>::~SkipList() {

    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
        _file_reader.close();
    }
    Node<K, V>* pnext = nullptr;
    while(_header != nullptr) //level 0 保存了所有的元素，通过level 0 遍历释放
    {
        pnext = _header->forward[0];
        delete _header;
        _header = pnext;
    }
    _header = nullptr;
}

template<typename K, typename V>
int SkipList<K, V>::get_random_level(){

    int k = 1;
    while (rand() % 2) {//每次随机出一个数判断奇偶，即增加层级的概率为1/2
        k++;
    }
    k = (k < _max_level) ? k : _max_level;
    return k;
};
// vim: et tw=100 ts=4 sw=4 cc=120
