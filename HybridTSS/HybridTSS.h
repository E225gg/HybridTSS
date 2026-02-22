#ifndef HYBRIDTSSV1_2_HYBRIDTSS_H
#define HYBRIDTSSV1_2_HYBRIDTSS_H
#include "SubHybridTSS.h"

class HybridTSS : public PacketClassifier {
public:
    HybridTSS();
    ~HybridTSS() override;
    void ConstructClassifier(const std::vector<Rule> &rules) override;

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
    void ConstructBaseline(const std::vector<Rule> &rules);


private:
    int binth = 8;
    SubHybridTSS *root;
    double rtssleaf = 1.5;
    std::vector<std::vector<double> > QTable;

    void train(const std::vector<Rule> &rules);
};
#endif //HYBRIDTSSV1_2_HYBRIDTSS_H