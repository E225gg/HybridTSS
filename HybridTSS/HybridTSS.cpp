//
// Created by GIGABYTE on 2022/3/2.
//

#include "HybridTSS.h"
//#define DEBUG
HybridTSS::HybridTSS() {
    binth = 8;
}

void HybridTSS::ConstructClassifier(const vector<Rule> &rules) {
    train(rules);
    root = new SubHybridTSS(rules);
    queue<SubHybridTSS*> que;
    que.push(root);
    bool flag = true;
    int nNode = 0;
    while(!que.empty()) {
        SubHybridTSS *node = que.front();
        que.pop();
        if (node) {
            node->nodeId = nNode ++;
        }
        vector<int> op = getAction(node, 99);
        if (flag) {
            flag = false;
//            cout << op[0] << "\t" << op[1] << "\t" << op[2] << endl;
        }
        vector<SubHybridTSS*> children = node->ConstructClassifier(op, "build");
        for (auto iter : children) {
            if (iter) {
                que.push(iter);
            }
        }
    }
    vector<vector<int> > reward = root->getReward();


}

int HybridTSS::ClassifyAPacket(const Packet &packet) {
    return root->ClassifyAPacket(packet);
//    return 0;
}

void HybridTSS::DeleteRule(const Rule &rule) {
    root->DeleteRule(rule);
}

void HybridTSS::InsertRule(const Rule &rule) {
    root->InsertRule(rule);

}

Memory HybridTSS::MemSizeBytes() const {
    return root->MemSizeBytes();
}

int HybridTSS::MemoryAccess() const {
    return 0;
}

size_t HybridTSS::NumTables() const {
    return 0;
}

size_t HybridTSS::RulesInTable(size_t tableIndex) const {
    return 0;
}

vector<int> HybridTSS::getAction(SubHybridTSS *state, int epsilion = 100) {
    if (!state) {
        cout << "state node exist" << endl;
        exit(-1);
    }
    
    // Greedy for linear, TM,
    int s = state->getState();
    vector<Rule> nodeRules = state->getRules();
    if (nodeRules.size() < binth) {
        return {linear, -1, -1};
    }
    set<uint32_t> tupleKey;
    for (const Rule &r : nodeRules) {
        tupleKey.insert((r.prefix_length[0] << 6) + r.prefix_length[1]);
    }
    if (static_cast<double>(nodeRules.size()) <= rtssleaf * static_cast<double>(tupleKey.size())) {
        return {TM, -1, -1};
    }
    int num = rand() % 100;
    if (epsilion == 100) {
        cout << s << endl;
        // baseline
        if ((s & 1) == 0) {
            return {Hash, 0, 7};
        }
        if ((s & (1 << 5)) == 0) {
            return {Hash, 1, 8};
        }
        if ((s & (1 << 10)) == 0) {
            return {Hash, 2, 7};
        }
        if ((s & (1 << 15)) == 0) {
            return {Hash, 3, 7};
        }
        cout << "nodeID:#" << state->nodeId << "\ts:" << s << endl;
        return {TM, -1, -1};
    }
    vector<vector<int> > Actions;
    vector<double> rews;
    for (int i = 0; i < 4; i ++) {
        if (s & (1 << (5 * i))) {
            continue;
        }
        for (int j = 0; j < 16; j++) {
            if ((i == 2 || i == 3) && j > 7) {
                continue;
            }
            Actions.push_back({Hash, i, j});
            int act = (i << 4) | j;
            rews.push_back(QTable[s][act]);
        }
    }
    if (rews.empty()) {
        return {TM, -1, -1};
    }
    if (num <= epsilion) {
        // E-greedy
        vector<int> op;
        double maxReward = rews[0];
        op = Actions[0];
        for (int i = 0; i < rews.size(); i++) {
            if (rews[i] > maxReward) {
                maxReward = rews[i];
                op = Actions[i];
            }
        }
        return op;
    } else {
        int N = rand() % rews.size();
        return Actions[N];
    }


    return {linear, -1, -1};
}

void HybridTSS::ConstructBaseline(const vector<Rule> &rules) {
    root = new SubHybridTSS(rules);
    root->nodeId = 0;
    queue<SubHybridTSS*> que;
    que.push(root);
    int nNode = 0;
#ifdef DEBUG
    cout << "Base Line function" << endl;
#endif
    while(!que.empty()) {
        SubHybridTSS *node = que.front();
        if (node) {
            node->nodeId = nNode ++;
        }
        que.pop();
        if (!node) {
            continue;
        }

#ifdef DEBUG
        cout << "before get Action" << endl;
#endif
        vector<int> op = getAction(node, 100);
#ifdef DEBUG
        cout << "Node: #" << node->nodeId << "\top:" << op[0] << "\t" << op[1] << "\t" << op[2] <<"\tnode rules:" << node->getRules().size() << endl;
#endif

        vector<SubHybridTSS*> next = node->ConstructClassifier(op, "build");
        cout << "op: " << op[0] << " " << op[1] << " " << op[2] << " state:" << node->getState() << endl;
#ifdef DEBUG
        cout << "After Construct" <<endl;
        cout << endl;
#endif

        for (auto iter : next) {
            if (iter) {
                que.push(iter);
            }
        }



    }
//    root->printInfo();
    cout << "Construct Finish" << endl;

}

string int2str(int x, int len) {
    string str = "";
    stack<char> st;
    while(x != 0) {
        if (x & 1) {
            st.push('1');
        } else {
            st.push('0');
        }
        x >>= 1;
    }
    for (int i = 0; i + st.size() < len; i++) {
        str += "0";
    }
    while (!st.empty()) {
        str.push_back(st.top());
        st.pop();
    }
    return str;

}


void HybridTSS::train(const vector<Rule> &rules) {
    int stateSize = 1 << 20, actionSize = 1 << 6;
    QTable.resize(stateSize, vector<double>(actionSize, 0.0));
//    uint32_t loopNum = 10000000 / rules.size();
    uint32_t loopNum = 10000;
    int trainRate = 10;
    for (int i = 0; i < loopNum; i++) {
//        cout << "next loop" << endl;
        if(i >= loopNum / 10 && i % (loopNum / 10) == 0){
            std::cout<<"Training finish "<<trainRate<<"% ...............Remaining: "<<100 - trainRate<<"%"<<std::endl;
            trainRate += 10;
        }
        auto *tmpRoot = new SubHybridTSS(rules);
        queue<SubHybridTSS*> que;
        que.push(tmpRoot);
        while (!que.empty()) {
            SubHybridTSS* node = que.front();
            que.pop();
            vector<int> op = getAction(node, 50);
            vector<SubHybridTSS*> children = node->ConstructClassifier(op, "train");
            for (auto iter : children) {
                if (iter) {
                    que.push(iter);
                }
            }
        }
        vector<vector<int> > reward;
        reward = tmpRoot->getReward();
        for (auto iter : reward) {
            if ((iter[1] >> 6) != 3) {
                continue;
            }
            int s = iter[0], a = iter[1] & ((1 << 6) - 1), r = iter[2];
            double lr = 0.1;
            if (QTable[s][a] == 0) {
                QTable[s][a] = r;
            } else {
                QTable[s][a] += lr * (r - QTable[s][a]);
            }
        }
        int act = reward[0][1] & ((1 << 6) - 1);
        tmpRoot->recurDelete();
        delete tmpRoot;
    }
}

void HybridTSS::printInfo() {
    root->printInfo();
}
