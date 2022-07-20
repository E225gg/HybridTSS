//
// Created by lyx on 2022/3/7.
//

#include "SubHybridTSS.h"

//#include <utility>
//#define DEBUG
//#define CHECKONERULE
SubHybridTSS::SubHybridTSS() {
    reward = -1;
//    offsetBit = {0, 32, 0, 32, 0, 16, 0, 16};
    bigClassifier = nullptr;
    state = 0;
    maxBigPriority = -1;

}

SubHybridTSS::SubHybridTSS(const vector<Rule> &r) {
    this->rules = r;
//    offsetBit = {0, 32, 0, 32, 0, 16, 0, 16};
    state = 0;
    reward = 0;
    bigClassifier = nullptr;
    maxBigPriority = -1;
    par = nullptr;
    bigOffset.resize(4, 0);

}


SubHybridTSS::SubHybridTSS(const vector<Rule> &r, vector<int> offsetBit) {
    this->rules = r;
    reward = -1;
    this->offsetBit = std::move(offsetBit);
    bigClassifier = nullptr;
    maxBigPriority = -1;
}
SubHybridTSS::SubHybridTSS(const vector<Rule> &r, int s, SubHybridTSS* p) {
    this->rules = r;
    this->state = s;
    bigClassifier = nullptr;
    maxBigPriority = -1;
    reward = 0;
    par = p;
}



/* op: 3-tuple  0: fun, 1: dim, 2: bit
 * the prefixlength shorter than op[2] are bigRules
 *
*/
vector<SubHybridTSS *> SubHybridTSS::ConstructClassifier(const vector<int> &op, const string& mode) {
    this->fun = op[0];
    action = 0;
    action |= (fun << 6);
#ifdef DEBUG
    cout << "SubHybridTSS" << endl;
    cout << op[0] << " " << op[1] << " " << op[2] << endl;
#endif
#ifdef CHECKONERULE

#endif
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
#ifdef DEBUG
            cout << "Hash" << endl;
#endif
            // modify action
            dim = op[1];
//            bit = 9 + op[2];
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
            while (nHashBit <= bit && (1 << nHashBit) - 1 < nrules * inflation) {
                nHashBit ++;
            }
            this->nHashTable = (1 << nHashBit) - 1;
#ifdef DEBUG
            cout << "nHashBit:" << nHashBit << "\t" << "nHashTable:" << nHashTable << endl;
#endif
            this->children.resize(nHashTable + 1, nullptr);
            vector<vector<Rule> > subRules(nHashTable + 1);

            for (const Rule &r : rules) {
//                if (r.priority == 4069 && dim == 1) {
//                    cout << "rule is here" << endl;
//                }
                if (r.prefix_length[dim] < bit) {
#ifdef CHECKONERULE
                    if (r.priority == 722) {
                        r.Print();
                        cout << dim << "\t" << r.prefix_length[dim] << "\t" << bit << endl;
                        cout << nodeId << "\tbig" << endl;
                    }
#endif
                    bigRules.push_back(r);
                    maxBigPriority = max(maxBigPriority, r.priority);
                    continue;
                }
                uint32_t Key = getKey(r);
#ifdef CHECKONERULE
                if (r.priority == 722) {
                    cout << nodeId << "\tHash Key:" << Key << endl;
                    cout << children.size() << endl;
                }
#endif
                if (Key >= subRules.size()) {
                    cout << "here" << endl;
                }
                subRules[Key].push_back(r);
            }
#ifdef DEBUG
            cout << "construct children" << endl;
#endif
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

    if (matchPri < maxBigPriority) {
        matchPri = max(matchPri, bigClassifier->ClassifyAPacket(packet));
    }
    return matchPri;
}

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
                bigClassifier->DeleteRule(rule);
            } else {
                uint32_t Key = getKey(rule);
                if (children[Key]) {
                    children[Key]->DeleteRule(rule);
                }
            }
        }
    }

}

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
                bigClassifier->InsertRule(rule);
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

int SubHybridTSS::MemoryAccess() const {
    return 0;
}



uint32_t SubHybridTSS::getKey(const Rule &r) const {
    uint32_t Key = 0;
    uint32_t t = static_cast<int>(r.range[dim][LowDim] >> offset);
    while (t) {
        Key ^= t & nHashTable;
        t >>= (nHashBit - 1);
    }
    return Key;
}

uint32_t SubHybridTSS::getKey(const Packet &p) const {
    uint32_t Key = 0;
    uint32_t t = (p[dim] >> offset);
    while(t) {
        Key ^= t & nHashTable;
        t >>= (nHashBit - 1);
    }
    return Key;
}

vector<vector<int> > SubHybridTSS::getReward() {
    vector<vector<int> > res;
    vector<int> currReward = {state, action, reward};
//    cout << "state:" << int2str(state, 20) << "\taction:" << int2str(action, 8) << "\treward:" << reward << endl;
//    cout << "state:" << state <<"\taction:" << action << "\t" << reward << endl;
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

uint32_t SubHybridTSS::getRuleSize() {
    return rules.size();
}

uint32_t SubHybridTSS::getRulePrefixKey(const Rule &r) {
    return (r.prefix_length[0] << 6) + r.prefix_length[1];
}

//#define StateDEBUG


vector<Rule> SubHybridTSS::getRules() {
    return rules;
}

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
            bigClassifier->FindRule(rule);

        }

    }
}
#define DEBUGREDELETE
#ifdef DEBUGREDELETE
void SubHybridTSS::recurDelete() {
//    cout << "before delete children" << endl;
    for (auto &iter : children) {
        if (iter) {
            iter->recurDelete();
            delete iter;
        }
    }
//    cout << "before delete big" << endl;
    if(bigClassifier) {
        bigClassifier->recurDelete();
    }
//    cout << "After delete big" << endl;
//    delete pstss;
//    delete TMO;
//    cout << "before delete TMO\t" << fun << endl;
//
//    cout << "before clear children" << endl;
    children.clear();
//    cout << "before delete big point" << endl;
    delete bigClassifier;
//    cout << "finish current deletion" << endl;

}
#endif
//#undef DEBUGREDELETE

int SubHybridTSS::getState() const {
    return state;
}

int SubHybridTSS::getAction() const {
    return action;
}

void SubHybridTSS::addReward(int r) {
    this->reward += r;
}

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
//                matchPri = children[Key]->ClassifyAPacket(packet);
            }
            break;
        }

    }

    if (matchPri < maxBigPriority) {
        matchPri = max(matchPri, bigClassifier->ClassifyAPacket(packet));
    }
}

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

void SubHybridTSS::printInfo() {
    if (fun == TM || fun == PSTSS) {
        if (rules.size() > 20) {
            cout << "nodeID:# " << nodeId << endl;
            cout << "rule size: " << rules.size() << endl;
            cout << "Tuple size: " << TMO->NumTables() << endl;
            cout << "state:   " << state2str(state) << endl;
            cout << "bigOffset:   " << bigOffset[0] << "  " << bigOffset[1] << "  " << bigOffset[2] << "  " << bigOffset[3] << endl;
//            cout << "nodeID:#" << nodeId << "\trule size:" << rules.size() << "\tTuple size:" << TMO->NumTables() << "\tstate:" << state2str(state) << endl;
            for (const auto& r : rules) {
                r.Print();
            }
            cout << endl;
        }
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

