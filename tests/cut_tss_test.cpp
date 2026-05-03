// tests/cut_tss_test.cpp
// Unit tests for CutTSS classifier
#include <gtest/gtest.h>
#include "ElementaryClasses.h"
#include "CutTSS/CutTSS.h"
#include <cstdio>

static Rule MakeCutTSSRule(int priority, int id, unsigned src_prefix, unsigned dst_prefix, Point src_ip, Point dst_ip) {
    Rule r;
    r.priority = priority;
    r.id = id;
    r.prefix_length = {src_prefix, dst_prefix, 0, 0, 0};
    r.range[FieldSA] = {{src_ip, src_ip}};
    r.range[FieldDA] = {{dst_ip, dst_ip}};
    r.range[FieldSP] = {{0, 65535}};
    r.range[FieldDP] = {{0, 65535}};
    r.range[FieldProto] = {{0, 255}};
    return r;
}

static Packet MakeCutTSSPacket(Point src_ip, Point dst_ip) {
    return {src_ip, dst_ip, 0, 0, 0};
}

class CutTSSTest : public ::testing::Test {
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

        classifier = new CutTSS();
    }
    void TearDown() override {
        delete classifier;
    }

    std::vector<Rule>   rules;
    std::vector<Packet> packets;
    CutTSS* classifier = nullptr;
};

TEST_F(CutTSSTest, ConstructDoesNotCrash) {
    ASSERT_NO_FATAL_FAILURE(classifier->ConstructClassifier(rules));
}

TEST_F(CutTSSTest, FunNameIsCorrect) {
    EXPECT_EQ(classifier->funName(), "class: CutTSS");
}

TEST_F(CutTSSTest, MemSizeIsPositive) {
    classifier->ConstructClassifier(rules);
    EXPECT_GT(classifier->MemSizeBytes(), 0u);
}

TEST_F(CutTSSTest, ClassifiesCorrectly) {
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

TEST_F(CutTSSTest, InsertRuleDoesNotCrash) {
    // Build with first 80 rules, then insert remaining
    std::vector<Rule> subset(rules.begin(), rules.begin() + 80);
    classifier->ConstructClassifier(subset);

    for (size_t i = 80; i < rules.size(); i++) {
        ASSERT_NO_FATAL_FAILURE(classifier->InsertRule(rules[i]));
    }
}

TEST_F(CutTSSTest, DeleteRuleDoesNotCrash) {
    classifier->ConstructClassifier(rules);
    for (int i = 0; i < 10 && i < static_cast<int>(rules.size()); i++) {
        ASSERT_NO_FATAL_FAILURE(classifier->DeleteRule(rules[i]));
    }
}

TEST(CutTSSRegressionTest, QueryOverloadClassifiesPSbigRule) {
    CutTSS classifier;
    Rule big = MakeCutTSSRule(10, 1, 0, 0, 0x01000000, 0x02000000);
    classifier.ConstructClassifier({big});

    uint64_t query = 0;
    EXPECT_EQ(classifier.ClassifyAPacket(MakeCutTSSPacket(0x01000000, 0x02000000), query), 10);
    EXPECT_GT(query, 0u);
}

TEST(CutTSSRegressionTest, NumTablesCountsOnlyPopulatedSubsets) {
    CutTSS classifier;
    Rule sourceOnly = MakeCutTSSRule(20, 1, 32, 0, 0x01000000, 0x02000000);
    Rule big = MakeCutTSSRule(10, 2, 0, 0, 0x03000000, 0x04000000);
    classifier.ConstructClassifier({sourceOnly, big});

    EXPECT_EQ(classifier.NumTables(), 2u);
}
