// tests/hybrid_tss_test.cpp
// Unit tests for HybridTSS classifier
// NOTE: HybridTSS runs RL training (with OpenMP), so we use a small ruleset.
#include <gtest/gtest.h>
#include "ElementaryClasses.h"
#include "HybridTSS/HybridTSS.h"
#include <cstdio>

class HybridTSSTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        FILE* fpr = fopen("Data/test_100", "r");
        ASSERT_NE(fpr, nullptr);
        rules = loadrule(fpr);
        fclose(fpr);

        FILE* fpt = fopen("Data/test_100_trace", "r");
        ASSERT_NE(fpt, nullptr);
        packets = loadpacket(fpt);
        fclose(fpt);

        classifier = new HybridTSS();
        classifier->ConstructClassifier(rules);
    }

    static void TearDownTestSuite() {
        delete classifier;
        classifier = nullptr;
        rules.clear();
        packets.clear();
    }

    // Accessors for tests
    static HybridTSS* PC() { return classifier; }
    static int NRules() { return static_cast<int>(rules.size()); }

    static std::vector<Rule> rules;
    static std::vector<Packet> packets;
    static HybridTSS* classifier;
};

std::vector<Rule> HybridTSSTest::rules;
std::vector<Packet> HybridTSSTest::packets;
HybridTSS* HybridTSSTest::classifier = nullptr;

TEST_F(HybridTSSTest, ConstructDoesNotCrash) {
    ASSERT_NE(PC(), nullptr);
}

TEST_F(HybridTSSTest, FunNameIsCorrect) {
    EXPECT_EQ(PC()->funName(), "class: HybridTSS");
}

TEST_F(HybridTSSTest, MemSizeIsPositive) {
    EXPECT_GT(PC()->MemSizeBytes(), 0u);
}

TEST_F(HybridTSSTest, ClassifiesCorrectly) {
    int nRules = NRules();
    int misclassified = 0;

    for (const auto& pkt : packets) {
        int matchPri = PC()->ClassifyAPacket(pkt);
        int result = nRules - 1 - matchPri;
        int expected = static_cast<int>(pkt[5]);
        if (result == nRules && expected == nRules) continue;
        if (result == nRules || expected < result) {
            misclassified++;
        }
    }
    EXPECT_EQ(misclassified, 0)
        << misclassified << " packets misclassified out of " << packets.size();
}

TEST_F(HybridTSSTest, InsertRuleDoesNotCrash) {
    for (size_t i = 80; i < rules.size(); i++) {
        ASSERT_NO_FATAL_FAILURE(PC()->InsertRule(rules[i]));
    }
}

TEST_F(HybridTSSTest, DeleteRuleDoesNotCrash) {
    for (int i = 0; i < 10 && i < NRules(); i++) {
        ASSERT_NO_FATAL_FAILURE(PC()->DeleteRule(rules[i]));
    }
}
