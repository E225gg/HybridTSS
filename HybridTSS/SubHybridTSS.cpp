#include "SubHybridTSS.h"

using namespace std;

// -----------------------------------------------------------------------------
// func name: SubHybridTSS
// description: 构造函数
// To-do: 部分构造方式不再使用，冗余待删除
// -----------------------------------------------------------------------------
SubHybridTSS::SubHybridTSS(int inflation_param) : TMO(nullptr), pstss(nullptr), bigClassifier(nullptr), par(nullptr), maxBigPriority(-1), state(0), action(0), reward(-1), fun(0), rules(), nHashTable(0), nHashBit(0), dim(0), bit(0), offset(0), offsetBit(), inflation_param(inflation_param) {}

SubHybridTSS::~SubHybridTSS() {
    recurDelete();
}

SubHybridTSS::SubHybridTSS(const vector<Rule> &r, int inflation_param) : TMO(nullptr), pstss(nullptr), bigClassifier(nullptr), par(nullptr), maxBigPriority(-1), state(0), action(0), reward(0), fun(0), rules(), nHashTable(0), nHashBit(0), dim(0), bit(0), offset(0), offsetBit(), inflation_param(inflation_param) {
    this->rules = r;
    bigOffset.resize(4, 0);
}

SubHybridTSS::SubHybridTSS(const vector<Rule> &r, vector<int> offsetBit, int inflation_param) : TMO(nullptr), pstss(nullptr), bigClassifier(nullptr), par(nullptr), maxBigPriority(-1), state(0), action(0), reward(-1), fun(0), rules(), nHashTable(0), nHashBit(0), dim(0), bit(0), offset(0), offsetBit(std::move(offsetBit)), inflation_param(inflation_param) {
    this->rules = r;
}
SubHybridTSS::SubHybridTSS(const vector<Rule> &r, int s, SubHybridTSS* p, int inflation_param) : TMO(nullptr), pstss(nullptr), bigClassifier(nullptr), par(p), maxBigPriority(-1), state(s), action(0), reward(0), fun(0), rules(), nHashTable(0), nHashBit(0), dim(0), bit(0), offset(0), offsetBit(), inflation_param(inflation_param) {
    this->rules = r;
}



// -----------------------------------------------------------------------------
// func name: ConstructClassifier
// description: 按照op指定的方式构造，并返回孩子节点，其中Hash是通过先解析action,
//              再哈希，散列表的大小与当前节点的规则数与膨胀系数相关
// To-do: 部分构造方式不再使用，冗余待删除
// -----------------------------------------------------------------------------
vector<SubHybridTSS *> SubHybridTSS::ConstructClassifier(const vector<int> &op, const string& mode) {
    this->fun = op[0];
    action = 0;
    action |= (fun << 6);

    switch(fun) {
        case linear: {
            break;
        }
        case PSTSS: {
            SubHybridTSS *p = this;
            int rew = -static_cast<int>(rules.size());
            while (p) {
                p->addReward(rew);
                p = p->par;
            }
            if (mode == "build") {
                this->pstss = new PriorityTupleSpaceSearch();
                pstss->ConstructClassifier(this->rules);
            }
            break;
        }
        case TM: {
            SubHybridTSS *p = this;
            int rew = -static_cast<int>(rules.size());
            while (p) {
                p->addReward(rew);
                p = p->par;
            }
            if (mode == "build") {
                this->TMO = new TupleMergeOnline();
                TMO->ConstructClassifier(this->rules);
            }
            break;
        }
        case Hash: {
            // modify action
            dim = op[1];
            if (dim == 0 || dim == 1) {
                bit = 12 + op[2];
            } else {
                bit = 9 + op[2];
            }
            action |= (dim << 4);
            action |= op[2];
            vector<int> mask = {32, 32, 16, 16};
            offset = mask[dim] - bit;

            int hashChildenState = state;
            hashChildenState |= (1 << (5 * dim));
            hashChildenState |= (op[2] << (5 * dim + 1));

            int hashBigState = state;
            hashBigState |= (1 << (5 * dim));

            vector<Rule> bigRules;
            maxBigPriority = -1;

            uint32_t nrules = rules.size();
            nHashBit = 0;
            while (nHashBit <= bit && (1 << nHashBit) - 1 < nrules * inflation_param) {
                nHashBit ++;
            }
            this->nHashTable = (1 << nHashBit) - 1;
            this->children.resize(nHashTable + 1, nullptr);
            vector<vector<Rule> > subRules(nHashTable + 1);

            for (const Rule &r : rules) {
                if (r.prefix_length[dim] < bit) {
                    bigRules.push_back(r);
                    maxBigPriority = max(maxBigPriority, r.priority);
                    continue;
                }
                uint32_t Key = getKey(r);
                if (Key >= subRules.size()) {
                    cout << "here" << endl;
                }
                subRules[Key].push_back(r);
            }
            for (int i = 0; i < nHashTable + 1; i++) {
                if (!subRules[i].empty()) {
                    children[i] = new SubHybridTSS(subRules[i], hashChildenState, this);
                    children[i]->bigOffset = this->bigOffset;
                }
            }
            if (!bigRules.empty()) {
                this->bigClassifier = new SubHybridTSS(bigRules, hashBigState, this);
                this->bigClassifier->bigOffset = this->bigOffset;
                this->bigClassifier->bigOffset[dim] = bit;
            }

        }
    }
    vector<SubHybridTSS*> next = children;
    if (bigClassifier) {
        next.push_back(bigClassifier);
    }
    return next;

}


// -----------------------------------------------------------------------------
// func name: ClassifyAPacket
// description: 根据packet内容进行查找，对于hash节点，先根据bit为进行哈希，
//              再查找bigClassifier
// To-do:
// -----------------------------------------------------------------------------
int SubHybridTSS::ClassifyAPacket(const Packet &packet) {
    int matchPri = -1;
    switch (fun) {
        case linear: {
            for (const auto& rule : this->rules) {
                if (rule.MatchesPacket(packet)) {
                    matchPri = rule.priority;
                    break;
                }
            }
            break;
        }
        case TM: {
            matchPri = this->TMO->ClassifyAPacket(packet);
            break;
        }
        case PSTSS: {
            matchPri = this->pstss->ClassifyAPacket(packet);
            break;
        }
        case Hash: {
            uint32_t Key = getKey(packet);
            if (!children[Key]) {
                matchPri = -1;
            } else {
                matchPri = children[Key]->ClassifyAPacket(packet);
            }
            break;
        }

    }

    if (bigClassifier && matchPri < maxBigPriority) {
        matchPri = max(matchPri, bigClassifier->ClassifyAPacket(packet));
    }
    return matchPri;
}

// -----------------------------------------------------------------------------
// func name: DeleteRule
// description: 找到rule所在的节点并删除，对于线性节点使用二分查找
// To-do:
// -----------------------------------------------------------------------------
void SubHybridTSS::DeleteRule(const Rule &rule) {
    switch (fun) {
        case linear: {
            auto iter = std::lower_bound(rules.begin(), rules.end(), rule);
            if (iter == rules.end()) {
                std::cout << "Not found" << std::endl;
                return ;
            }
            rules.erase(iter);
            return ;
        }
        case TM: {
            this->TMO->DeleteRule(rule);
            return ;
        }
        case PSTSS: {
            this->pstss->DeleteRule(rule);
            return ;
        }
        case Hash: {
            // To-do: record dim and offset in construct
            if (rule.prefix_length[dim] < bit) {
                if (bigClassifier) {
                    bigClassifier->DeleteRule(rule);
                }
            } else {
                uint32_t Key = getKey(rule);
                if (children[Key]) {
                    children[Key]->DeleteRule(rule);
                }
            }
        }
    }

}

// -----------------------------------------------------------------------------
// func name: InsertRule
// description: 根据rule找到相应的节点并插入，对于线性节点使用二分查找插入
// To-do:
// -----------------------------------------------------------------------------
void SubHybridTSS::InsertRule(const Rule &rule) {
    switch (fun) {
        case linear: {
            auto iter = lower_bound(rules.begin(), rules.end(), rule);
            rules.insert(iter, rule);
            break;
        }
        case TM: {
            this->TMO->InsertRule(rule);
            break;
        }
        case PSTSS: {
            this->pstss->InsertRule(rule);
            break;
        }
        case Hash: {
            // To-do: record dim and offset in construct
            if (rule.prefix_length[dim] < bit) {
                if (bigClassifier) {
                    bigClassifier->InsertRule(rule);
                }
            } else {
                uint32_t Key = getKey(rule);
                if (children[Key]) {
                    children[Key]->InsertRule(rule);
                } else {
                    children[Key] = new SubHybridTSS({rule});
                    children[Key]->ConstructClassifier({linear, -1, -1}, "build");
                }
            }
        }
    }

}

// -----------------------------------------------------------------------------
// func name: MemSizeBytes
// description: 根据节点类型计算Memory
// To-do:
// -----------------------------------------------------------------------------
Memory SubHybridTSS::MemSizeBytes() const {
    Memory totMemory = 0;
    totMemory += NODE_SIZE;
    switch(fun) {
        case linear: {
            totMemory += rules.size() * PTR_SIZE;
            break;
        }
        case PSTSS: {
            totMemory += pstss->MemSizeBytes();
            break;
        }
        case TM: {
            totMemory += TMO->MemSizeBytes();
            break;
        }
        case Hash: {
            for (auto child : children) {
                totMemory += PTR_SIZE;
                if (child) {
                    totMemory += child->MemSizeBytes();
                }
            }
            break;
        }
    }
    return totMemory;
}


// -----------------------------------------------------------------------------
// func name: MemoryAccess
// description: 因父类设计不合理，待重新实现
// To-do:
// -----------------------------------------------------------------------------
int SubHybridTSS::MemoryAccess() const {
    return 0;
}

// -----------------------------------------------------------------------------
// func name: getKey
// description: 根据rule与当前节点的状态获得哈希key
// To-do:
// -----------------------------------------------------------------------------
uint32_t SubHybridTSS::getKey(const Rule &r) const {
    uint32_t Key = 0;
    uint32_t t = static_cast<int>(r.range[dim][LowDim] >> offset);
    while (t) {
        Key ^= t & nHashTable;
        t >>= (nHashBit - 1);
    }
    return Key;
}

// -----------------------------------------------------------------------------
// func name: getKey
// description: 根据Packet与当前节点的状态获得哈希key
// To-do:
// -----------------------------------------------------------------------------
uint32_t SubHybridTSS::getKey(const Packet &p) const {
    uint32_t Key = 0;
    uint32_t t = (p[dim] >> offset);
    while(t) {
        Key ^= t & nHashTable;
        t >>= (nHashBit - 1);
    }
    return Key;
}

// -----------------------------------------------------------------------------
// func name: getReward
// description: 计算当前节点即所有子节点的reward，先依次递归所有孩子节点计算reward，
//              再根据所有孩子节点reward值总和计算当前节点的reward，保存当前节点的
//              state, action, reward返回，供QTable更新使用
// To-do:
// -----------------------------------------------------------------------------
vector<vector<int> > SubHybridTSS::getReward() {
    vector<vector<int> > res;
    vector<int> currReward = {state, action, reward};
    res.push_back(currReward);
    for (auto iter : children) {
        if (iter) {
            vector<vector<int> > tmp = iter->getReward();
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
    }
    if (bigClassifier) {
        vector<vector<int> > tmp = bigClassifier->getReward();
        res.insert(res.end(), tmp.begin(), tmp.end());
    }
    return res;
}

// -----------------------------------------------------------------------------
// func name: getRuleSize
// description: 计算当前节点含有的规则数量
// To-do:
// -----------------------------------------------------------------------------
uint32_t SubHybridTSS::getRuleSize() {
    return rules.size();
}

// -----------------------------------------------------------------------------
// func name: getRulePrefixKey
// description: 已废弃不再使用
// To-do:
// -----------------------------------------------------------------------------
uint32_t SubHybridTSS::getRulePrefixKey(const Rule &r) {
    return (r.prefix_length[0] << 6) + r.prefix_length[1];
}

// -----------------------------------------------------------------------------
// func name: getRules
// description: 返回当前节点所有规则，后门接口，不使用
// To-do:
// -----------------------------------------------------------------------------
const vector<Rule>& SubHybridTSS::getRules() const {
    return rules;
}

// -----------------------------------------------------------------------------
// func name: FindRule
// description: 查询rule是否在当前节点及子节点中，DEBUG代码，不使用
// To-do:
// -----------------------------------------------------------------------------
void SubHybridTSS::FindRule(const Rule &rule) {
    if (fun == linear || fun == TM || fun == PSTSS) {
        cout << nodeId << " " << fun << endl;
    } else {
        cout << nodeId << " ";
        if (rule.prefix_length[dim] >= bit) {
            uint32_t Key = getKey(rule);
            cout << "hash Key:" << Key << endl;

            if (children[Key]) {
                children[Key]->FindRule(rule);
            } else {
                cout << "None" << endl;
            }
        } else {
            cout << "big" << endl;
            if (bigClassifier) {
                bigClassifier->FindRule(rule);
            } else {
                cout << "bigClassifier is null" << endl;
            }
        }

    }
}

// -----------------------------------------------------------------------------
// func name: recurDelete
// description: 递归删除，C++ RAII
// To-do:
// -----------------------------------------------------------------------------
#define DEBUGREDELETE
#ifdef DEBUGREDELETE
void SubHybridTSS::recurDelete() {
    for (auto &iter : children) {
        if (iter) {
            iter->recurDelete();
            delete iter;
            iter = nullptr;
        }
    }
    if(bigClassifier) {
        bigClassifier->recurDelete();
        delete bigClassifier;
        bigClassifier = nullptr;
    }
    // Free leaf-node classifiers owned by this node
    if (TMO) {
        delete TMO;
        TMO = nullptr;
    }
    if (pstss) {
        delete pstss;
        pstss = nullptr;
    }
    children.clear();
}
#endif
//#undef DEBUGREDELETE

// -----------------------------------------------------------------------------
// func name: reset
// description: 重置節點狀態以便在訓練中重用，避免重複 new/delete
// -----------------------------------------------------------------------------
void SubHybridTSS::reset() {
    // 步驟 1: 清理所有動態配置的子節點
    recurDelete();

    // 步驟 2: 重置成員變數到初始狀態 (參考建構函式)
    // 注意：rules, par, bigOffset 等在重用時不需要改變，所以不重置
    state = 0;
    reward = 0;
    maxBigPriority = -1;
    fun = -1; // 或其他代表未設定的初始值
    action = 0;

    // 清理分類器指標
    TMO = nullptr;
    pstss = nullptr;
    bigClassifier = nullptr; // recurDelete 已經處理，這裡確保為空

    // 清理與 Hash 相關的狀態
    nHashTable = 0;
    nHashBit = 0;
    dim = 0;
    bit = 0;
    offset = 0;

    // 確保 children vector 是空的
    children.clear();
}

// -----------------------------------------------------------------------------
// func name: getState
// description: 获取当前节点state
// To-do:
// -----------------------------------------------------------------------------
int SubHybridTSS::getState() const {
    return state;
}

// -----------------------------------------------------------------------------
// func name: getAction
// description: 获取当前节点action
// To-do:
// -----------------------------------------------------------------------------
int SubHybridTSS::getAction() const {
    return action;
}

// -----------------------------------------------------------------------------
// func name: addReward
// description: 在构造时对当前节点的reward值进行更新，当节点类型为TM或PSTSS时被调用
// To-do:
// -----------------------------------------------------------------------------
void SubHybridTSS::addReward(int r) {
    this->reward += r;
}

// -----------------------------------------------------------------------------
// func name: FindPacket
// description: DEBUG代码，不使用
// To-do:
// -----------------------------------------------------------------------------
void SubHybridTSS::FindPacket(const Packet &packet) {
    int matchPri = -1;
    cout << nodeId << " ";
    switch (fun) {
        case linear: {
            for (const auto& rule : this->rules) {
                if (rule.MatchesPacket(packet)) {
                    matchPri = rule.priority;
                    break;
                }
            }
            break;
        }
        case TM: {
            matchPri = this->TMO->ClassifyAPacket(packet);
            break;
        }
        case PSTSS: {
            matchPri = this->pstss->ClassifyAPacket(packet);
            break;
        }
        case Hash: {
            uint32_t Key = getKey(packet);
            cout << "hash Key:" << Key << endl;
            if (!children[Key]) {
                cout << "None" << endl;
                matchPri = -1;
            } else {
                children[Key]->FindPacket(packet);
            }
            break;
        }

    }

    if (bigClassifier && matchPri < maxBigPriority) {
        matchPri = max(matchPri, bigClassifier->ClassifyAPacket(packet));
    }
}

// -----------------------------------------------------------------------------
// func name: state2str
// description: state转string
// To-do:
// -----------------------------------------------------------------------------
string state2str(int state) {
    string res;
    vector<int> vec(4, -1);
    for (int i = 0; i < 4; i++) {
        if (state & 1) {
            state >>= 1;
            vec[i] = state & 15;
            state >>= 4;
        }
        res += to_string(vec[i]);
        res += "   ";
    }

    return res;

}

// -----------------------------------------------------------------------------
// func name: printInfo
// description: 打印当前节点信息
// To-do:
// -----------------------------------------------------------------------------
void SubHybridTSS::printInfo() {
    size_t tuple_size = 0;
    switch(fun) {
        case TM: if(TMO) tuple_size = TMO->NumTables(); break;
        case PSTSS: if(pstss) tuple_size = pstss->NumTables(); break;
        case Hash: tuple_size = children.size(); break;
        case linear: tuple_size = children.size(); break;
        default: tuple_size = 0; break;
    }

    std::string fun_str;
    switch(fun) {
        case TM: fun_str = "TM"; break;
        case PSTSS: fun_str = "PSTSS"; break;
        case Hash: fun_str = "Hash"; break;
        case linear: fun_str = "Linear"; break;
        default: fun_str = "Unknown"; break;
    }

    if ((fun == TM || fun == PSTSS || fun == Hash || fun == linear) && (tuple_size > 0)) {
        cout << "nodeID:# " << nodeId  << endl;
        cout << "fun: " << fun_str << endl;
        cout << "rule size: " << rules.size() << endl;
        cout << "tuple size: " << tuple_size << endl;
        cout << "state: " << state2str(state) << endl;
        cout << "bigOffset: " << bigOffset[0] << "  "
                  << bigOffset[1] << "  "
                  << bigOffset[2] << "  "
                  << bigOffset[3] << endl;

        for (const auto& r : rules) {
            r.Print();
        }
        cout << endl;
    }

    for (auto iter : children) {
        if (iter) {
            iter->printInfo();
        }
    }
    if (bigClassifier) {
        bigClassifier->printInfo();
    }
}
