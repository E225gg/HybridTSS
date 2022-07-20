//
// Created by lyx on 2022/3/7.
//
#ifndef HYBRIDTSSV1_2_SUBHYBRIDTSS_H
#define HYBRIDTSSV1_2_SUBHYBRIDTSS_H

#include "./../TupleMerge/TupleMergeOnline.h"

#define inflation 10

using namespace std;


enum Fun{linear, PSTSS, TM, Hash};
class SubHybridTSS {
public:
    SubHybridTSS();
    explicit SubHybridTSS(const vector<Rule> &r);
    SubHybridTSS(const vector<Rule> &r, int s, SubHybridTSS* p);
    SubHybridTSS(const vector<Rule> &r, vector<int> offsetBit);
    vector<SubHybridTSS*> ConstructClassifier(const vector<int> &op, const string& mode);
    int ClassifyAPacket(const Packet& packet);
    void DeleteRule(const Rule& rule);
    void InsertRule(const Rule& rule);
    Memory MemSizeBytes() const;
    int MemoryAccess() const;

    void addReward(int r);
    vector<vector<int> > getReward();
    uint32_t getRuleSize();
    int getState() const;
    int getAction() const;
    vector<Rule> getRules();

    void printInfo();

    void FindRule(const Rule &rule);
    void FindPacket(const Packet &p);
    void recurDelete();


    int nodeId;
    vector<int> bigOffset;


private:
    // Next
    TupleMergeOnline *TMO;
    PriorityTupleSpaceSearch *pstss;
    vector<SubHybridTSS*> children;
    SubHybridTSS* bigClassifier, *par;
    int maxBigPriority;

    int state, action, reward;

    // info
    int fun;
    vector<Rule> rules;
    int nHashTable, nHashBit;
    int dim, bit, offset; // bit: use high bits, offset use for get hashKey
    vector<int> offsetBit; // [0, 32]

    // Hash fun
//    int getRuleKey(const Rule &r, int dim, int offset);
    uint32_t getRulePrefixKey(const Rule &r);
    uint32_t getKey(const Rule &r) const;
    uint32_t getKey(const Packet &p) const;


//    int inflation;
    vector<int> threshold;
};

#endif //HYBRIDTSSV1_2_SUBHYBRIDTSS_H
