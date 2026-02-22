// tests/pstss_test.cpp
// Unit tests for PriorityTupleSpaceSearch classifier
#include <gtest/gtest.h>
#include "ElementaryClasses.h"
#include "OVS/TupleSpaceSearch.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// Fixture: loads Data/test_100 rules and Data/test_100_trace packets
// ---------------------------------------------------------------------------
class PSTSSTest : public ::testing::Test {
protected:
    void SetUp() override {
        FILE* fpr = fopen("Data/test_100", "r");
        ASSERT_NE(fpr, nullptr);
        rules = loadrule(fpr);
        fclose(fpr);
        ASSERT_EQ(static_cast<int>(rules.size()), 100);

        FILE* fpt = fopen("Data/test_100_trace", "r");
        ASSERT_NE(fpt, nullptr);
        packets = loadpacket(fpt);
        fclose(fpt);
        ASSERT_FALSE(packets.empty());

        classifier = new PriorityTupleSpaceSearch();
    }
    void TearDown() override {
        delete classifier;
    }

    std::vector<Rule>   rules;
    std::vector<Packet> packets;
    PriorityTupleSpaceSearch* classifier = nullptr;
};

// ---------------------------------------------------------------------------
TEST_F(PSTSSTest, ConstructDoesNotCrash) {
    ASSERT_NO_FATAL_FAILURE(classifier->ConstructClassifier(rules));
}

TEST_F(PSTSSTest, FunNameIsCorrect) {
    EXPECT_EQ(classifier->funName(), "class: PSTSS");
}

TEST_F(PSTSSTest, NumTablesIsPositive) {
    classifier->ConstructClassifier(rules);
    EXPECT_GT(classifier->NumTables(), 0u);
}

TEST_F(PSTSSTest, MemSizeIsPositive) {
    classifier->ConstructClassifier(rules);
    EXPECT_GT(classifier->MemSizeBytes(), 0u);
}

TEST_F(PSTSSTest, ClassifiesCorrectly) {
    classifier->ConstructClassifier(rules);
    int nRules = static_cast<int>(rules.size());
    int misclassified = 0;

    for (const auto& pkt : packets) {
        int matchPri = classifier->ClassifyAPacket(pkt);
        int result = nRules - 1 - matchPri;
        int expected = static_cast<int>(pkt[5]);
        // result == nRules and expected == nRules both mean "no match" — that's correct
        if (result == nRules && expected == nRules) continue;
        if (result == nRules || expected < result) {
            misclassified++;
        }
    }
    EXPECT_EQ(misclassified, 0)
        << misclassified << " packets misclassified out of " << packets.size();
}

TEST_F(PSTSSTest, InsertRuleIncreasesCapacity) {
    // Build with first 50 rules
    std::vector<Rule> subset(rules.begin(), rules.begin() + 50);
    classifier->ConstructClassifier(subset);

    // Insert remaining rules one-by-one
    for (size_t i = 50; i < rules.size(); i++) {
        classifier->InsertRule(rules[i]);
    }
    // After inserting all rules, we should still classify correctly
    int nRules = static_cast<int>(rules.size());
    int misclassified = 0;
    for (const auto& pkt : packets) {
        int matchPri = classifier->ClassifyAPacket(pkt);
        int result = nRules - 1 - matchPri;
        int expected = static_cast<int>(pkt[5]);
        if (result == nRules && expected == nRules) continue;
        if (result == nRules || expected < result) {
            misclassified++;
        }
    }
    EXPECT_EQ(misclassified, 0);
}

TEST_F(PSTSSTest, DeleteRuleDoesNotCrash) {
    classifier->ConstructClassifier(rules);
    // Delete a handful of rules
    for (int i = 0; i < 10 && i < static_cast<int>(rules.size()); i++) {
        ASSERT_NO_FATAL_FAILURE(classifier->DeleteRule(rules[i]));
    }
}
