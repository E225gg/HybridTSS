// tests/integration_test.cpp
// End-to-end integration test: build classifiers, classify all packets,
// verify zero misclassifications — mirrors main.cpp's testPerformance logic.
#include <gtest/gtest.h>
#include "ElementaryClasses.h"
#include "OVS/TupleSpaceSearch.h"
#include "CutTSS/CutTSS.h"
#include "TupleMerge/TupleMergeOnline.h"
#include "HybridTSS/HybridTSS.h"
#include <cstdio>
#include <memory>

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        FILE* fpr = fopen("Data/test_100", "r");
        ASSERT_NE(fpr, nullptr) << "Cannot open Data/test_100";
        rules = loadrule(fpr);
        fclose(fpr);
        ASSERT_EQ(static_cast<int>(rules.size()), 100);

        FILE* fpt = fopen("Data/test_100_trace", "r");
        ASSERT_NE(fpt, nullptr) << "Cannot open Data/test_100_trace";
        packets = loadpacket(fpt);
        fclose(fpt);
        ASSERT_EQ(static_cast<int>(packets.size()), 1000);
    }

    // Helper: classify all packets and return misclassification count
    int CountMisclassified(PacketClassifier* pc) {
        int nRules = static_cast<int>(rules.size());
        int miss = 0;
        for (const auto& pkt : packets) {
            int matchPri = pc->ClassifyAPacket(pkt);
            int result = nRules - 1 - matchPri;
            int expected = static_cast<int>(pkt[5]);
            // Both nRules means "no match" — that's correct, not a miss
            if (result == nRules && expected == nRules) continue;
            if (result == nRules || expected < result) {
                miss++;
            }
        }
        return miss;
    }

    std::vector<Rule>   rules;
    std::vector<Packet> packets;
};

TEST_F(IntegrationTest, PSTSSZeroMisclassified) {
    std::unique_ptr<PriorityTupleSpaceSearch> pc(new PriorityTupleSpaceSearch());
    pc->ConstructClassifier(rules);
    EXPECT_EQ(CountMisclassified(pc.get()), 0);
}

TEST_F(IntegrationTest, CutTSSZeroMisclassified) {
    std::unique_ptr<CutTSS> pc(new CutTSS());
    pc->ConstructClassifier(rules);
    EXPECT_EQ(CountMisclassified(pc.get()), 0);
}

TEST_F(IntegrationTest, TupleMergeZeroMisclassified) {
    std::unique_ptr<TupleMergeOnline> pc(new TupleMergeOnline());
    pc->ConstructClassifier(rules);
    EXPECT_EQ(CountMisclassified(pc.get()), 0);
}

TEST_F(IntegrationTest, HybridTSSZeroMisclassified) {
    std::unique_ptr<HybridTSS> pc(new HybridTSS());
    pc->ConstructClassifier(rules);
    EXPECT_EQ(CountMisclassified(pc.get()), 0);
}

// Verify that all classifiers agree on classification results
TEST_F(IntegrationTest, AllClassifiersAgree) {
    std::unique_ptr<PriorityTupleSpaceSearch> pstss(new PriorityTupleSpaceSearch());
    std::unique_ptr<CutTSS> cuttss(new CutTSS());
    std::unique_ptr<TupleMergeOnline> tm(new TupleMergeOnline());
    std::unique_ptr<HybridTSS> ht(new HybridTSS());

    pstss->ConstructClassifier(rules);
    cuttss->ConstructClassifier(rules);
    tm->ConstructClassifier(rules);
    ht->ConstructClassifier(rules);

    for (size_t i = 0; i < packets.size(); i++) {
        int r1 = pstss->ClassifyAPacket(packets[i]);
        int r2 = cuttss->ClassifyAPacket(packets[i]);
        int r3 = tm->ClassifyAPacket(packets[i]);
        int r4 = ht->ClassifyAPacket(packets[i]);

        EXPECT_EQ(r1, r2) << "PSTSS vs CutTSS disagree on packet " << i;
        EXPECT_EQ(r1, r3) << "PSTSS vs TupleMerge disagree on packet " << i;
        EXPECT_EQ(r1, r4) << "PSTSS vs HybridTSS disagree on packet " << i;
    }
}
