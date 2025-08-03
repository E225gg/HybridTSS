#ifndef HYBRIDTSSV1_2_HYBRIDTSS_H
#define HYBRIDTSSV1_2_HYBRIDTSS_H

#include "SubHybridTSS.h"
using namespace std;

class HybridTSS : public PacketClassifier {
public:
    // 父类方法
    HybridTSS();
    void ConstructClassifier(const std::vector<Rule> &rules) override;

    int ClassifyAPacket(const Packet& packet) override;
    void DeleteRule(const Rule &rule) override;
    void InsertRule(const Rule &rule) override;
    Memory MemSizeBytes() const override;
    int MemoryAccess() const override;
    size_t NumTables() const override;
    size_t RulesInTable(size_t tableIndex) const override;
    string funName() override { return "class: HybridTSS"; }
    void printInfo();
    vector<int> getAction(SubHybridTSS *state, int epsilion);
    void ConstructBaseline(const vector<Rule> &rules);


private:
    // 原本訓練用變數
    int binth = 8;
    SubHybridTSS *root;
    double rtssleaf = 1.5;
    vector<vector<double>> QTable;  // QTable (Q-Table)

    // 新增：Batch Update 所需結構與參數
    struct Exp {                // Exp (experience)
        int s;                  // 狀態 (state)
        int a;                  // 動作 (action)
        int r;                  // 獎勵 (reward)
    };
    vector<Exp> batchBuf;       // 批次暫存區 (batch buffer)
    const size_t batchSize = 1024;              // 每次累積筆數 (batch size)
    static constexpr double learnRate = 0.10;  // 學習率 (learning rate)

    // ● 新增：批次更新函式宣告
    void updateBatch();

    // 原本訓練函式
    void train(const vector<Rule> &rules);
};
#endif //HYBRIDTSSV1_2_HYBRIDTSS_H
