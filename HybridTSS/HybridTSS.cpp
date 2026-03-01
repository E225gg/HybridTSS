#include "HybridTSS.h"
#include <omp.h>
#include <atomic>
#include <algorithm>
#include <cstring>

using namespace std;


// #define DEBUG
// #define DEBUG_NODE
#include <fstream>
#include <iomanip>

namespace {
constexpr char kQTableMagic[4] = {'H', 'T', 'Q', '1'};
constexpr uint32_t kQTableVersion = 1;
}


HybridTSS::HybridTSS(const HybridOptions& opts)
    : options(opts),
      binth(opts.binth),
      root(nullptr),
      rtssleaf(opts.rtssleaf),
      qtable_loaded_(false),
      last_error_() {
}

HybridTSS::~HybridTSS() {
    delete root;   // ~SubHybridTSS() calls recurDelete() automatically
    root = nullptr;
}

// -----------------------------------------------------------------------------
// func name: ConstructClassifier
// description: 先训练模型后构建Classifier
// To-do: 结构过于冗余，待抽象根据QTable构建Classifier代码
// -----------------------------------------------------------------------------
void HybridTSS::ConstructClassifier(const vector<Rule> &rules) {
    ConstructClassifierSafe(rules, nullptr);
}

bool HybridTSS::ConstructClassifierSafe(const vector<Rule> &rules, string* err) {
    delete root;
    root = nullptr;
    qtable_loaded_ = false;
    last_error_.clear();

    if (!prepareQTable(rules, err)) {
        return false;
    }

    buildTreeFromQTable(rules);

    if (!root) {
        last_error_ = "failed to build HybridTSS tree";
        if (err) {
            *err = last_error_;
        }
        return false;
    }
    return true;
}

bool HybridTSS::prepareQTable(const vector<Rule>& rules, string* err) {
    if (options.train_online) {
        train(rules);
        qtable_loaded_ = true;
        if (!options.qtable_out_path.empty()) {
            string saveErr;
            if (!SaveQTable(options.qtable_out_path, &saveErr)) {
                last_error_ = saveErr;
                if (err) {
                    *err = last_error_;
                }
                return false;
            }
        }
        return true;
    }

    if (options.qtable_in_path.empty()) {
        last_error_ = "inference-only mode requires --ht-qtable-in";
        if (err) {
            *err = last_error_;
        }
        return false;
    }
    return LoadQTable(options.qtable_in_path, err);
}

void HybridTSS::buildTreeFromQTable(const vector<Rule>& rules) {

    root = new SubHybridTSS(rules, options.hash_inflation);
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
    // vector<vector<int> > reward = root->getReward();

// #ifdef DEBUG
//     // 將 QTable 前 N 個 state/action 儲存到檔案
//     ofstream fout("debug_QTable.txt");
//     if (fout.is_open()) {
//         fout << fixed << setprecision(6);
//         int stateCount = 0;
//         for (int s = 0; s < QTable.size() && stateCount < 50; ++s) { // 只輸出前 50 個 state
//             for (int a = 0; a < QTable[s].size(); ++a) {
//                 if (QTable[s][a] != 0) {
//                     fout << "state:" << s << "\taction:" << a << "\treward:" << QTable[s][a] << endl;
//                 }
//             }
//             stateCount++;
//         }
//         fout.close();
//     }

//     // 將節點 reward 儲存到檔案
//     auto reward = root->getReward();
//     fout.open("debug_reward.txt");
//     if (fout.is_open()) {
//         for (auto &r : reward) {
//             fout << "state:" << r[0] << "\taction:" << r[1] << "\treward:" << r[2] << endl;
//         }
//         fout.close();
//     }
// #endif

#ifdef DEBUG
    // 訓練完成後：印出整棵樹資訊，存入 debug_tree.txt
    std::ofstream fout("debug_tree.txt", std::ios::app);
    if (fout.is_open()) {
        // 暫時重導 cout -> fout
        std::streambuf *old_buf = std::cout.rdbuf(fout.rdbuf());
        std::cout << "========== HybridTSS Tree Debug Info ==========" << std::endl;

        // 呼叫原本的 printInfo()，不改任何底層
        printInfo();

        std::cout << "========== End of Tree Info ==========" << std::endl;
        // 還原 cout
        std::cout.rdbuf(old_buf);
        fout.close();
    }
#endif
}

// -----------------------------------------------------------------------------
// func name: ClassifyAPacket
// description: 根据packet查询相应匹配的规则
// To-do:
// -----------------------------------------------------------------------------
int HybridTSS::ClassifyAPacket(const Packet &packet) {
    if (!root) {
        return -1;
    }
    return root->ClassifyAPacket(packet);
}

// -----------------------------------------------------------------------------
// func name: DeleteRule
// description: 删除相应的规则
// To-do:
// -----------------------------------------------------------------------------
void HybridTSS::DeleteRule(const Rule &rule) {
    if (root) {
        root->DeleteRule(rule);
    }
}


// -----------------------------------------------------------------------------
// func name: InsertRule
// description: 插入相应的规则
// To-do:
// -----------------------------------------------------------------------------
void HybridTSS::InsertRule(const Rule &rule) {
    if (root) {
        root->InsertRule(rule);
    }
}

// -----------------------------------------------------------------------------
// func name: MemSizeBytes
// description: 计算相应结构整体的Memory
// To-do:
// -----------------------------------------------------------------------------
Memory HybridTSS::MemSizeBytes() const {
    if (!root) {
        return 0;
    }
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

bool HybridTSS::IsReady() const {
    return root != nullptr && qtable_loaded_;
}

const string& HybridTSS::LastError() const {
    return last_error_;
}

bool HybridTSS::SaveQTable(const string& path, string* err) const {
    if (QTable.empty() || QTable[0].empty()) {
        if (err) {
            *err = "QTable is empty; nothing to save";
        }
        return false;
    }

    const uint32_t rows = static_cast<uint32_t>(QTable.size());
    const uint32_t cols = static_cast<uint32_t>(QTable[0].size());
    for (const auto& row : QTable) {
        if (row.size() != cols) {
            if (err) {
                *err = "QTable rows have inconsistent sizes";
            }
            return false;
        }
    }

    ofstream out(path, ios::binary | ios::trunc);
    if (!out.is_open()) {
        if (err) {
            *err = "failed to open QTable output file: " + path;
        }
        return false;
    }

    const uint32_t stateBits = static_cast<uint32_t>(options.state_bits);
    const uint32_t actionBits = static_cast<uint32_t>(options.action_bits);

    out.write(kQTableMagic, sizeof(kQTableMagic));
    out.write(reinterpret_cast<const char*>(&kQTableVersion), sizeof(kQTableVersion));
    out.write(reinterpret_cast<const char*>(&stateBits), sizeof(stateBits));
    out.write(reinterpret_cast<const char*>(&actionBits), sizeof(actionBits));
    out.write(reinterpret_cast<const char*>(&rows), sizeof(rows));
    out.write(reinterpret_cast<const char*>(&cols), sizeof(cols));
    for (const auto& row : QTable) {
        out.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size() * sizeof(double)));
    }

    if (!out.good()) {
        if (err) {
            *err = "failed while writing QTable file: " + path;
        }
        return false;
    }
    return true;
}

bool HybridTSS::LoadQTable(const string& path, string* err) {
    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        last_error_ = "failed to open QTable input file: " + path;
        if (err) {
            *err = last_error_;
        }
        return false;
    }

    char magic[4] = {0, 0, 0, 0};
    uint32_t version = 0;
    uint32_t stateBits = 0;
    uint32_t actionBits = 0;
    uint32_t rows = 0;
    uint32_t cols = 0;

    in.read(magic, sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&stateBits), sizeof(stateBits));
    in.read(reinterpret_cast<char*>(&actionBits), sizeof(actionBits));
    in.read(reinterpret_cast<char*>(&rows), sizeof(rows));
    in.read(reinterpret_cast<char*>(&cols), sizeof(cols));

    if (!in.good()) {
        last_error_ = "invalid QTable header: " + path;
        if (err) {
            *err = last_error_;
        }
        return false;
    }
    if (memcmp(magic, kQTableMagic, sizeof(magic)) != 0) {
        last_error_ = "QTable magic mismatch: " + path;
        if (err) {
            *err = last_error_;
        }
        return false;
    }
    if (version != kQTableVersion) {
        last_error_ = "QTable version mismatch";
        if (err) {
            *err = last_error_;
        }
        return false;
    }
    if (stateBits != static_cast<uint32_t>(options.state_bits) || actionBits != static_cast<uint32_t>(options.action_bits)) {
        last_error_ = "QTable bits mismatch with current HybridOptions";
        if (err) {
            *err = last_error_;
        }
        return false;
    }

    const uint32_t expectedRows = static_cast<uint32_t>(1u << options.state_bits);
    const uint32_t expectedCols = static_cast<uint32_t>(1u << options.action_bits);
    if (rows != expectedRows || cols != expectedCols) {
        last_error_ = "QTable dimensions mismatch with state/action bits";
        if (err) {
            *err = last_error_;
        }
        return false;
    }

    vector<vector<double>> loaded(rows, vector<double>(cols, 0.0));
    for (uint32_t r = 0; r < rows; ++r) {
        in.read(reinterpret_cast<char*>(loaded[r].data()), static_cast<std::streamsize>(cols * sizeof(double)));
        if (!in.good()) {
            last_error_ = "QTable payload truncated: " + path;
            if (err) {
                *err = last_error_;
            }
            return false;
        }
    }

    QTable.swap(loaded);
    qtable_loaded_ = true;
    last_error_.clear();
    return true;
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
vector<int> HybridTSS::getAction(SubHybridTSS *state, int epsilion) {
    if (!state) {
        cout << "state node exist" << endl;
        exit(-1);
    }

    // Greedy for linear, TM,
    int s = state->getState();
    const vector<Rule>& nodeRules = state->getRules();
    if (nodeRules.size() < binth) {
        return {linear, -1, -1};
    }
    set<uint32_t> tupleKey;
    for (const Rule &r : nodeRules) {
        tupleKey.insert((r.prefix_length[0] << 6) + r.prefix_length[1]);
    }

    // 存疑，待修正
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
    root = new SubHybridTSS(rules, options.hash_inflation);
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
// description: (重構後) 高效能並行訓練 Q-Table
// detail: 採用 Thread-local Q-Table 避免鎖競爭，物件重用避免記憶體開銷，
//         並在最後階段高效地合併 Q-Table。
// -----------------------------------------------------------------------------


// ------------------------------------------------------------
// Utility: 狀態-動作編碼壓縮為 key
// ------------------------------------------------------------
using Key = uint64_t; // key = (state << 6) | action
inline Key make_key(int s, int a) { return (static_cast<Key>(s) << 6) | (a & 0x3F); }
inline int key_state(Key k) { return static_cast<int>(k >> 6); }
inline int key_action(Key k) { return static_cast<int>(k & 0x3F); }

// ------------------------------------------------------------
// 主訓練函式 (快速收斂版 + DEBUG學習曲線)
// ------------------------------------------------------------
void HybridTSS::train(const vector<Rule> &rules) {

    cout << "Starting parallel training with " << omp_get_max_threads() << " threads..." << endl;

    const int stateSize = 1 << options.state_bits;
    const int actionSize = 1 << options.action_bits;
    const int loopNum = options.loop_num;      // 減少以便測試
    const int progressStep = options.progress_step;
    const double lr = options.lr;         // 初始 learning rate
    const double decay = options.decay;    // learning rate 衰減係數

    const double epsilon0 = options.epsilon0;
    const double epsilonMin = options.epsilon_min;
    const double epsilonDecay = options.epsilon_decay;

#ifdef DEBUG
    vector<double> rewardCurve(loopNum, 0.0);
#endif

    QTable.assign(stateSize, vector<double>(actionSize, 0.0));

    int numThreads = omp_get_max_threads();
    vector<unordered_map<Key, double>> localQ(numThreads);
    vector<unordered_map<Key, int>> localCount(numThreads);
    for (auto &m : localQ) m.reserve(1 << 16);
    for (auto &m : localCount) m.reserve(1 << 16);

    atomic<int> progressCounter(0);
    atomic<int> nextPrint(progressStep);

#ifdef DEBUG
    std::atomic<long long> lockAttempts{0};
    std::atomic<long long> lockContended{0};
    omp_lock_t qLock;
    omp_init_lock(&qLock);
#endif

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto &Qlocal = localQ[tid];
        auto &countLocal = localCount[tid];

        uint64_t seed_base;
        if (options.seed != 0) {
            seed_base = options.seed;
        } else {
            seed_base = static_cast<uint64_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
        }
        std::mt19937 gen(static_cast<unsigned>(seed_base ^ (tid * 131071u)));

        SubHybridTSS root(rules, options.hash_inflation);

        for (int i = tid; i < loopNum; i += numThreads) {

            double epsilon = std::max(epsilonMin, epsilon0 * exp(-epsilonDecay * i));

            int done = ++progressCounter;
            if (done * 100 / loopNum >= nextPrint.load()) {
                #pragma omp critical
                {
                    if (done * 100 / loopNum >= nextPrint.load()) {
                        cout << "Training " << nextPrint.load() << "% completed..." << endl;
                        nextPrint.store(nextPrint.load() + progressStep);
                    }
                }
            }

            root.reset();

            queue<SubHybridTSS*> que;
            que.push(&root);

            while (!que.empty()) {
                SubHybridTSS *node = que.front(); que.pop();
                vector<int> op = getAction(node, epsilon * 100.0);
                for (auto *child : node->ConstructClassifier(op, "train")) {
                    if (child) que.push(child);
                }
            }

            auto rewards = root.getReward();

#ifdef DEBUG
            double episodeReward = 0.0;
#endif

            // thread-local Q-table 更新
            for (auto &r : rewards) {
                if ((r[1] >> 6) != 3) continue;
                int s = r[0];
                int a = r[1] & 63;
                double val = tanh(r[2] / 100.0);
                Key k = make_key(s, a);

                int &count = countLocal[k];
                double alpha = lr / (1.0 + decay * count);
                count++;

                auto it = Qlocal.find(k);
                if (it == Qlocal.end()) {
                    Qlocal[k] = val;
                } else {
                    it->second += alpha * (val - it->second);
                }

#ifdef DEBUG
                episodeReward += val;
#endif
            }

#ifdef DEBUG
            if (i == 0)
                rewardCurve[i] = episodeReward;
            else
                rewardCurve[i] = rewardCurve[i-1] + (episodeReward - rewardCurve[i-1]) / (i+1);
#endif
        }
    }

    cout << "Training finished, merging local Q-tables..." << endl;

    vector<vector<int>> QCount(stateSize, vector<int>(actionSize, 0));

    #pragma omp parallel for schedule(static)
    for (int t = 0; t < numThreads; ++t) {
        for (const auto& kv : localQ[t]) {
            int s = key_state(kv.first);
            int a = key_action(kv.first);
            int c = localCount[t][kv.first];

#ifdef DEBUG
            lockAttempts++;
            if (!omp_test_lock(&qLock)) {
                lockContended++;
                omp_set_lock(&qLock);
            }
#endif
            #pragma omp critical
            {
                QTable[s][a] = (QTable[s][a] * QCount[s][a] + kv.second * c) / (QCount[s][a] + c);
                QCount[s][a] += c;
            }
#ifdef DEBUG
            omp_unset_lock(&qLock);
#endif
        }
    }

#ifdef DEBUG
    omp_destroy_lock(&qLock);
    cout << "[DEBUG] Lock Attempts: " << lockAttempts << endl;
    cout << "[DEBUG] Lock Contended: " << lockContended << endl;
    cout << "[DEBUG] Contention Ratio: "
         << (double)lockContended / lockAttempts * 100.0 << "%" << endl;

    cout << "Saving learning curve to learning_curve.csv ..." << endl;
    ofstream fout("learning_curve.csv");
    for (int i = 0; i < loopNum; i++) {
        fout << i << "," << rewardCurve[i] << "\n";
    }
    fout.close();
    cout << "Learning curve export finished" << endl;
#endif

    cout << "Parallel training completed successfully!" << endl;
}


// -----------------------------------------------------------------------------
// func name: printInfo
// description: 打印函数信息
// To-do:
// -----------------------------------------------------------------------------
void HybridTSS::printInfo() {
#ifdef DEBUG_NODE
    root->printInfo();
#endif
}
