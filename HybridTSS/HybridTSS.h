#ifndef HYBRIDTSSV1_2_HYBRIDTSS_H
#define HYBRIDTSSV1_2_HYBRIDTSS_H
#include "SubHybridTSS.h"
#include <cstdint>
#include <random>
#include <string>

struct HybridOptions {
    int binth = 8;              // linear search threshold
    double rtssleaf = 1.5;      // tuple merge vs hash threshold
    int loop_num = 50;          // training episodes
    double lr = 0.05;           // learning rate
    double decay = 0.001;       // learning rate decay
    double epsilon0 = 0.5;      // initial epsilon for exploration
    double epsilon_min = 0.01;  // minimum epsilon
    double epsilon_decay = 0.003; // epsilon decay factor
    int state_bits = 20;        // bits for state space (size = 1<<state_bits)
    int action_bits = 6;        // bits for action space (size = 1<<action_bits)
    int progress_step = 10;     // progress print step (%), used in DEBUG
    int hash_inflation = 10;    // hash table inflation factor in SubHybridTSS
    uint64_t seed = 0;          // optional seed for training RNG (0 => time-based)
    bool train_online = true;   // true: train in ConstructClassifier, false: require pre-trained QTable
    std::string qtable_in_path; // optional input path for pre-trained QTable
    std::string qtable_out_path; // optional output path to persist trained QTable
};

class HybridTSS : public PacketClassifier {
public:
    explicit HybridTSS(const HybridOptions& opts = HybridOptions());
    ~HybridTSS() override;

    // Non-copyable and non-movable (owns raw pointer tree)
    HybridTSS(const HybridTSS&) = delete;
    HybridTSS& operator=(const HybridTSS&) = delete;
    HybridTSS(HybridTSS&&) = delete;
    HybridTSS& operator=(HybridTSS&&) = delete;

    void ConstructClassifier(const std::vector<Rule> &rules) override;
    bool ConstructClassifierSafe(const std::vector<Rule> &rules, std::string* err = nullptr);

    bool LoadQTable(const std::string& path, std::string* err = nullptr);
    bool SaveQTable(const std::string& path, std::string* err = nullptr) const;
    bool IsReady() const;
    const std::string& LastError() const;

    int ClassifyAPacket(const Packet& packet) override;
    void DeleteRule(const Rule& rule) override;
    void InsertRule(const Rule& rule) override;
    Memory MemSizeBytes() const override;
    int MemoryAccess() const override;
    size_t NumTables() const override;
    size_t RulesInTable(size_t tableIndex) const override;

    std::string funName() override {
        return "class: HybridTSS";
    }
    std::string prints() {
        return "";
    }

    void printInfo();
    std::vector<int> getAction(SubHybridTSS *state, int epsilion);
    std::vector<int> getAction(SubHybridTSS *state, int epsilion, std::mt19937& gen);
    void ConstructBaseline(const std::vector<Rule> &rules);


private:
    HybridOptions options;
    int binth;
    SubHybridTSS *root;
    double rtssleaf;
    std::vector<std::vector<double> > QTable;
    bool qtable_loaded_;
    std::string last_error_;

    bool prepareQTable(const std::vector<Rule>& rules, std::string* err);
    void buildTreeFromQTable(const std::vector<Rule>& rules);

    void train(const std::vector<Rule> &rules);
};
#endif //HYBRIDTSSV1_2_HYBRIDTSS_H
