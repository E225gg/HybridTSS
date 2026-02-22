// tests/tuplemerge_test.cpp
// Unit tests for TupleMergeOnline classifier
#include <gtest/gtest.h>
#include "ElementaryClasses.h"
#include "TupleMerge/TupleMergeOnline.h"
#include <cstdio>

class TupleMergeTest : public ::testing::Test {
protected:
    void SetUp() override {
        FILE* fpr = fopen("Data/test_100", "r");
        ASSERT_NE(fpr, nullptr);
        rules = loadrule(fpr);
        fclose(fpr);

        FILE* fpt = fopen("Data/test_100_trace", "r");
        ASSERT_NE(fpt, nullptr);
        packets = loadpacket(fpt);
        fclose(fpt);

        classifier = new TupleMergeOnline();
    }
    void TearDown() override {
        delete classifier;
    }

    std::vector<Rule>   rules;
    std::vector<Packet> packets;
    TupleMergeOnline* classifier = nullptr;
};

TEST_F(TupleMergeTest, ConstructDoesNotCrash) {
    ASSERT_NO_FATAL_FAILURE(classifier->ConstructClassifier(rules));
}

TEST_F(TupleMergeTest, FunNameIsCorrect) {
    EXPECT_EQ(classifier->funName(), "class: TupleMerge");
}

TEST_F(TupleMergeTest, NumTablesIsPositive) {
    classifier->ConstructClassifier(rules);
    EXPECT_GT(classifier->NumTables(), 0u);
}

TEST_F(TupleMergeTest, MemSizeIsPositive) {
    classifier->ConstructClassifier(rules);
    EXPECT_GT(classifier->MemSizeBytes(), 0u);
}

TEST_F(TupleMergeTest, ClassifiesCorrectly) {
    classifier->ConstructClassifier(rules);
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
    EXPECT_EQ(misclassified, 0)
        << misclassified << " packets misclassified out of " << packets.size();
}

TEST_F(TupleMergeTest, InsertAndClassify) {
    // Build with first 50 rules
    std::vector<Rule> subset(rules.begin(), rules.begin() + 50);
    classifier->ConstructClassifier(subset);

    // Insert remaining rules
    for (size_t i = 50; i < rules.size(); i++) {
        ASSERT_NO_FATAL_FAILURE(classifier->InsertRule(rules[i]));
    }

    // Re-classify — should still be correct
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

TEST_F(TupleMergeTest, DeleteRuleDoesNotCrash) {
    classifier->ConstructClassifier(rules);
    for (int i = 0; i < 10 && i < static_cast<int>(rules.size()); i++) {
        ASSERT_NO_FATAL_FAILURE(classifier->DeleteRule(rules[i]));
    }
}
