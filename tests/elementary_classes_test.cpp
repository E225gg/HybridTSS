// tests/elementary_classes_test.cpp
// Unit tests for Rule, Packet, loadrule, loadpacket, SortRules
#include <gtest/gtest.h>
#include "ElementaryClasses.h"
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Rule construction
// ---------------------------------------------------------------------------
TEST(RuleTest, DefaultConstructionHasFiveDimensions) {
    Rule r;
    EXPECT_EQ(r.dim, 5);
    EXPECT_EQ(static_cast<int>(r.prefix_length.size()), 5);
    EXPECT_EQ(static_cast<int>(r.range.size()), 5);
    EXPECT_FALSE(r.markedDelete);
}

TEST(RuleTest, CustomDimensionCount) {
    Rule r(3);
    EXPECT_EQ(r.dim, 3);
    EXPECT_EQ(static_cast<int>(r.range.size()), 3);
}

// ---------------------------------------------------------------------------
// Rule::MatchesPacket
// ---------------------------------------------------------------------------
TEST(RuleTest, MatchesPacketInRange) {
    Rule r;
    // Source IP: 10.0.0.0/8  -> [0x0A000000, 0x0AFFFFFF]
    r.range[0] = {{0x0A000000, 0x0AFFFFFF}};
    // Dest IP: wildcard
    r.range[1] = {{0, 0xFFFFFFFF}};
    // Ports: wildcard
    r.range[2] = {{0, 65535}};
    r.range[3] = {{0, 65535}};
    // Proto: wildcard
    r.range[4] = {{0, 255}};

    Packet p = {0x0A010203, 0xC0A80001, 80, 443, 6, 0};
    EXPECT_TRUE(r.MatchesPacket(p));
}

TEST(RuleTest, MatchesPacketOutOfRange) {
    Rule r;
    r.range[0] = {{0x0A000000, 0x0AFFFFFF}};
    r.range[1] = {{0, 0xFFFFFFFF}};
    r.range[2] = {{0, 65535}};
    r.range[3] = {{0, 65535}};
    r.range[4] = {{0, 255}};

    // Source IP outside 10.x.x.x
    Packet p = {0x0B000001, 0xC0A80001, 80, 443, 6, 0};
    EXPECT_FALSE(r.MatchesPacket(p));
}

TEST(RuleTest, MatchesPacketExactBoundary) {
    Rule r;
    r.range[0] = {{100, 200}};
    r.range[1] = {{0, 0xFFFFFFFF}};
    r.range[2] = {{0, 65535}};
    r.range[3] = {{0, 65535}};
    r.range[4] = {{0, 255}};

    Packet pLow  = {100, 0, 0, 0, 0, 0};
    Packet pHigh = {200, 0, 0, 0, 0, 0};
    Packet pBelow = {99, 0, 0, 0, 0, 0};
    Packet pAbove = {201, 0, 0, 0, 0, 0};

    EXPECT_TRUE(r.MatchesPacket(pLow));
    EXPECT_TRUE(r.MatchesPacket(pHigh));
    EXPECT_FALSE(r.MatchesPacket(pBelow));
    EXPECT_FALSE(r.MatchesPacket(pAbove));
}

// ---------------------------------------------------------------------------
// Rule comparison operators
// ---------------------------------------------------------------------------
TEST(RuleTest, LessThanByPriority) {
    Rule a, b;
    a.priority = 10;
    b.priority = 5;
    // operator< returns (this->priority > r.priority), i.e. higher priority = "less"
    // This gives descending sort order: a (pri=10) < b (pri=5) is true
    EXPECT_TRUE(a < b);   // 10 > 5 => true
    EXPECT_FALSE(b < a);  // 5 > 10 => false
}

TEST(RuleTest, EqualityByPriority) {
    Rule a, b;
    a.priority = 7;
    b.priority = 7;
    EXPECT_TRUE(a == b);
}

// ---------------------------------------------------------------------------
// SortRules
// ---------------------------------------------------------------------------
TEST(SortRulesTest, SortsDescendingPriority) {
    std::vector<Rule> rules;
    for (int i = 0; i < 5; i++) {
        Rule r;
        r.priority = i;
        rules.push_back(r);
    }
    SortRules(rules);
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(rules[i].priority, rules[i + 1].priority);
    }
}

TEST(SortRulesTest, SortsPointerVector) {
    std::vector<Rule> storage(5);
    std::vector<Rule*> ptrs;
    for (int i = 0; i < 5; i++) {
        storage[i].priority = i * 3;
        ptrs.push_back(&storage[i]);
    }
    SortRules(ptrs);
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(ptrs[i]->priority, ptrs[i + 1]->priority);
    }
}

// ---------------------------------------------------------------------------
// loadrule
// ---------------------------------------------------------------------------
class LoadRuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        fp = fopen("Data/test_100", "r");
        ASSERT_NE(fp, nullptr) << "Cannot open Data/test_100 — run tests from project root";
    }
    void TearDown() override {
        if (fp) fclose(fp);
    }
    FILE* fp = nullptr;
};

TEST_F(LoadRuleTest, LoadsCorrectCount) {
    auto rules = loadrule(fp);
    EXPECT_EQ(static_cast<int>(rules.size()), 100);
}

TEST_F(LoadRuleTest, PrioritiesAreDescending) {
    auto rules = loadrule(fp);
    // rule[0] has highest priority (nRules - 1)
    EXPECT_EQ(rules[0].priority, 99);
    EXPECT_EQ(rules[99].priority, 0);
    for (size_t i = 0; i + 1 < rules.size(); i++) {
        EXPECT_GT(rules[i].priority, rules[i + 1].priority);
    }
}

TEST_F(LoadRuleTest, RuleIdsAreSequential) {
    auto rules = loadrule(fp);
    for (size_t i = 0; i < rules.size(); i++) {
        EXPECT_EQ(rules[i].id, static_cast<int>(i));
    }
}

TEST_F(LoadRuleTest, RangesAreValid) {
    auto rules = loadrule(fp);
    for (const auto& r : rules) {
        for (int d = 0; d < r.dim; d++) {
            EXPECT_LE(r.range[d][LowDim], r.range[d][HighDim])
                << "Rule id=" << r.id << " dim=" << d;
        }
    }
}

TEST(LoadRuleTest_Edge, NullFilePointerReturnsEmpty) {
    auto rules = loadrule(nullptr);
    EXPECT_TRUE(rules.empty());
}

// ---------------------------------------------------------------------------
// loadpacket
// ---------------------------------------------------------------------------
class LoadPacketTest : public ::testing::Test {
protected:
    void SetUp() override {
        fp = fopen("Data/test_100_trace", "r");
        ASSERT_NE(fp, nullptr) << "Cannot open Data/test_100_trace";
    }
    void TearDown() override {
        if (fp) fclose(fp);
    }
    FILE* fp = nullptr;
};

TEST_F(LoadPacketTest, LoadsCorrectCount) {
    auto packets = loadpacket(fp);
    EXPECT_EQ(static_cast<int>(packets.size()), 1000);
}

TEST_F(LoadPacketTest, PacketHasSixFields) {
    auto packets = loadpacket(fp);
    for (const auto& p : packets) {
        EXPECT_EQ(static_cast<int>(p.size()), 6);
    }
}

TEST_F(LoadPacketTest, FidFieldIsReasonable) {
    auto packets = loadpacket(fp);
    // fid should be <= 100 (100 is sentinel for "no match")
    for (const auto& p : packets) {
        EXPECT_LE(p[5], 100u);
    }
}

TEST(LoadPacketTest_Edge, NullFilePointerReturnsEmpty) {
    auto packets = loadpacket(nullptr);
    EXPECT_TRUE(packets.empty());
}
