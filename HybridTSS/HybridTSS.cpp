#include "HybridTSS.h"
HybridTSS::HybridTSS() {
    binth = 8;
}

// -----------------------------------------------------------------------------
// func name: ConstructClassifier
// description: 先训练模型后构建Classifier
// To-do: 结构过于冗余，待抽象根据QTable构建Classifier代码
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// func name: ClassifyAPacket
// description: 根据packet查询相应匹配的规则
// To-do: 
// -----------------------------------------------------------------------------
int HybridTSS::ClassifyAPacket(const Packet &packet) {
    return root->ClassifyAPacket(packet);
}

// -----------------------------------------------------------------------------
// func name: DeleteRule
// description: 删除相应的规则
// To-do: 
// -----------------------------------------------------------------------------
void HybridTSS::DeleteRule(const Rule &rule) {
    root->DeleteRule(rule);
}


// -----------------------------------------------------------------------------
// func name: InsertRule
// description: 插入相应的规则
// To-do: 
// -----------------------------------------------------------------------------
void HybridTSS::InsertRule(const Rule &rule) {
    root->InsertRule(rule);
}

// -----------------------------------------------------------------------------
// func name: MemSizeBytes
// description: 计算相应结构整体的Memory
// To-do: 
// -----------------------------------------------------------------------------
Memory HybridTSS::MemSizeBytes() const {
    return root->MemSizeBytes();
}

// -----------------------------------------------------------------------------
// func name: MemoryAccess
// description: 计算访存次数
// To-do: 父类设计不合理，待修改父类后实现
// -----------------------------------------------------------------------------
int HybridTSS::MemoryAccess() const {
    return 0;
}

// -----------------------------------------------------------------------------
// func name: NumTables
// description: 计算Tuple数量
// To-do: 与本方法无关，不实现
// -----------------------------------------------------------------------------
size_t HybridTSS::NumTables() const {
    return 0;
}

// -----------------------------------------------------------------------------
// func name: RulesInTable
// description: 判断第Index条规则在哪个table中
// To-do: 父类冗余方法，不实现
// -----------------------------------------------------------------------------
size_t HybridTSS::RulesInTable(size_t tableIndex) const {
    return 0;
}

// -----------------------------------------------------------------------------
// func name: getAction
// description: 根据当前节点的state获取Action
// detail: state与action编码方式详见readme, epsilion为最优与探索的标记值，epsilion
//         越大每次使用最大reward的概率越大，epsilion = 0为仅探索方式，opsilion = 99
//         为仅访问最优方法，epsilion = 100 为构造baseline方法，不涉及强化学习
// return: {action类型，维度，所选bit(不计算偏移)}
// To-do: 方法冗余待优化，编码方式待修改，目的支持单个维度多次选取
// -----------------------------------------------------------------------------
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
        return {TM, -1, -1};
    }
    
    // 记录所有action
    vector<vector<int> > Actions;
    // 记录每个action对应的reward， 结构冗余待优化
    vector<double> rews;
    for (int i = 0; i < 4; i ++) {
        // 当前维度是否选择过
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
        // random explore
        int N = rand() % rews.size();
        return Actions[N];
    }


    return {linear, -1, -1};
}

// -----------------------------------------------------------------------------
// func name: ConstructBaseline
// description: 构造baseline结构
// To-do: 
// -----------------------------------------------------------------------------
void HybridTSS::ConstructBaseline(const vector<Rule> &rules) {
    root = new SubHybridTSS(rules);
    root->nodeId = 0;
    queue<SubHybridTSS*> que;
    que.push(root);
    int nNode = 0;
    while(!que.empty()) {
        SubHybridTSS *node = que.front();
        if (node) {
            node->nodeId = nNode ++;
        }
        que.pop();
        if (!node) {
            continue;
        }

        vector<int> op = getAction(node, 100);

        vector<SubHybridTSS*> next = node->ConstructClassifier(op, "build");

        for (auto iter : next) {
            if (iter) {
                que.push(iter);
            }
        }
    }
    cout << "Construct Finish" << endl;

}

// -----------------------------------------------------------------------------
// func name: int2str
// description: int转string，对齐长度至len, 并补充前置0
// To-do: 冗余函数用to_string重新实现
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// func name: train
// description: 训练并获得QTable
// detail: 对于每个节点使用getAction获得下一步策略，直到构建完成，获得每一步的返回值，
//         对于每一步的State和Action更新Q表
// To-do: 方法构建HybridTSS方式过于冗余，待优化，训练次数与结束条件待优化
// -----------------------------------------------------------------------------
void HybridTSS::train(const vector<Rule> &rules) {
    int stateSize = 1 << 20, actionSize = 1 << 6;
    QTable.resize(stateSize, vector<double>(actionSize, 0.0));
    uint32_t loopNum = 10000;
    int trainRate = 10;
    for (int i = 0; i < loopNum; i++) {
        if(i >= loopNum / 10 && i % (loopNum / 10) == 0){
            std::cout<<"Training finish "<<trainRate<<"% ...............Remaining: "<<100 - trainRate<<"%"<<std::endl;
            trainRate += 10;
        }
        // 构造classifier
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

        // 获得reward并更新Q表
        vector<vector<int> > reward;
        reward = tmpRoot->getReward();
        for (auto iter : reward) {
            if ((iter[1] >> 6) != 3) {
                continue;
            }
            int s = iter[0], a = iter[1] & ((1 << 6) - 1), r = iter[2];
            double lr = 0.1; // 改成静态const
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

// -----------------------------------------------------------------------------
// func name: printInfo
// description: 打印函数信息
// To-do: 
// -----------------------------------------------------------------------------
void HybridTSS::printInfo() {
    root->printInfo();
}
