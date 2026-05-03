// tests/update_correctness_test.cpp
// Regression tests for classifier correctness after runtime rule updates.
#include <gtest/gtest.h>

#include "CutTSS/CutTSS.h"
#include "ElementaryClasses.h"
#include "HybridTSS/HybridTSS.h"
#include "OVS/TupleSpaceSearch.h"
#include "TupleMerge/TupleMergeOnline.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

Rule MakeRule(int priority,
              int id,
              Point src_low,
              Point src_high,
              unsigned src_prefix,
              Point dst_low,
              Point dst_high,
              unsigned dst_prefix,
              Point src_port_low,
              Point src_port_high,
              unsigned src_port_prefix,
              Point dst_port_low,
              Point dst_port_high,
              unsigned dst_port_prefix,
              Point proto_low,
              Point proto_high,
              unsigned proto_prefix) {
    Rule rule;
    rule.priority = priority;
    rule.id = id;
    rule.prefix_length = {src_prefix, dst_prefix, src_port_prefix, dst_port_prefix, proto_prefix};
    rule.range[FieldSA] = {{src_low, src_high}};
    rule.range[FieldDA] = {{dst_low, dst_high}};
    rule.range[FieldSP] = {{src_port_low, src_port_high}};
    rule.range[FieldDP] = {{dst_port_low, dst_port_high}};
    rule.range[FieldProto] = {{proto_low, proto_high}};
    return rule;
}

Rule MakeExactSrcRule(int priority, int id, Point src_ip) {
    return MakeRule(priority, id,
                    src_ip, src_ip, 32,
                    0, 0xFFFFFFFF, 0,
                    0, 65535, 0,
                    0, 65535, 0,
                    0, 255, 0);
}

Rule MakeExactDstPortRule(int priority, int id, Point dst_port) {
    return MakeRule(priority, id,
                    0, 0xFFFFFFFF, 0,
                    0, 0xFFFFFFFF, 0,
                    0, 65535, 0,
                    dst_port, dst_port, 16,
                    0, 255, 0);
}

Rule MakeCatchAllRule(int priority, int id) {
    return MakeRule(priority, id,
                    0, 0xFFFFFFFF, 0,
                    0, 0xFFFFFFFF, 0,
                    0, 65535, 0,
                    0, 65535, 0,
                    0, 255, 0);
}

Packet MakePacket(Point src_ip, Point dst_port) {
    return {src_ip, 0, 12345, dst_port, 6};
}

int LinearScanPriority(const std::vector<Rule>& rules, const Packet& packet) {
    int priority = -1;
    for (const auto& rule : rules) {
        if (rule.MatchesPacket(packet)) {
            priority = std::max(priority, rule.priority);
        }
    }
    return priority;
}

void RemoveRule(std::vector<Rule>& rules, const Rule& target) {
    auto it = std::find_if(rules.begin(), rules.end(), [&](const Rule& rule) {
        return rule.id == target.id && rule.priority == target.priority;
    });
    ASSERT_NE(it, rules.end()) << "test tried to remove a rule that is not active";
    rules.erase(it);
}

void ExpectClassifierMatchesBaseline(PacketClassifier& classifier,
                                     const std::vector<Rule>& active_rules,
                                     const std::vector<Packet>& packets,
                                     const std::string& stage) {
    for (const auto& packet : packets) {
        EXPECT_EQ(classifier.ClassifyAPacket(packet), LinearScanPriority(active_rules, packet))
            << stage
            << " packet src=" << packet[FieldSA]
            << " dport=" << packet[FieldDP];
    }
}

template <typename ConstructFn>
void RunStagedUpdateCorrectness(PacketClassifier& classifier, ConstructFn construct) {
    const Rule catch_all = MakeCatchAllRule(1, 1);
    const Rule src_rule = MakeExactSrcRule(10, 2, 0x0A000001);
    const Rule dst_port_rule = MakeExactDstPortRule(20, 3, 443);
    const Rule inserted_rule = MakeExactSrcRule(30, 4, 0x0A000002);

    std::vector<Rule> active_rules = {dst_port_rule, src_rule, catch_all};
    const std::vector<Packet> packets = {
        MakePacket(0x0A000001, 80),   // src_rule, then catch_all if deleted later.
        MakePacket(0x0A000003, 443),  // dst_port_rule, then catch_all after delete.
        MakePacket(0x0A000002, 80),   // inserted_rule after runtime insert.
        MakePacket(0x0A000004, 80),   // catch_all fallback.
    };

    construct(classifier, active_rules);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "after build");

    classifier.InsertRule(inserted_rule);
    active_rules.push_back(inserted_rule);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "after insert");

    classifier.DeleteRule(dst_port_rule);
    RemoveRule(active_rules, dst_port_rule);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "after deleting built rule");

    classifier.DeleteRule(inserted_rule);
    RemoveRule(active_rules, inserted_rule);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "after deleting inserted rule");
}

template <typename ConstructFn>
void RunDuplicatePriorityDeleteCorrectness(PacketClassifier& classifier, ConstructFn construct) {
    const Rule catch_all = MakeCatchAllRule(1, 1);
    const Rule first = MakeExactSrcRule(10, 2, 0x0A000001);
    const Rule second = MakeExactSrcRule(10, 3, 0x0A000002);

    std::vector<Rule> active_rules = {first, second, catch_all};
    const std::vector<Packet> packets = {
        MakePacket(0x0A000001, 80),
        MakePacket(0x0A000002, 80),
        MakePacket(0x0A000003, 80),
    };

    construct(classifier, active_rules);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "duplicate priority after build");

    classifier.DeleteRule(first);
    RemoveRule(active_rules, first);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "duplicate priority after deleting first");

    classifier.DeleteRule(second);
    RemoveRule(active_rules, second);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "duplicate priority after deleting second");
}

} // namespace

TEST(UpdateCorrectnessTest, PSTSSInsertDeleteMatchesLinearScanBaseline) {
    PriorityTupleSpaceSearch classifier;
    RunStagedUpdateCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        pc.ConstructClassifier(rules);
    });
}

TEST(UpdateCorrectnessTest, PSTSSDuplicatePriorityDeleteKeepsNeighbor) {
    PriorityTupleSpaceSearch classifier;
    RunDuplicatePriorityDeleteCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        pc.ConstructClassifier(rules);
    });
}

TEST(UpdateCorrectnessTest, CutTSSInsertDeleteMatchesLinearScanBaseline) {
    CutTSS classifier;
    RunStagedUpdateCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        pc.ConstructClassifier(rules);
    });
}

TEST(UpdateCorrectnessTest, CutTSSDuplicatePriorityDeleteKeepsNeighbor) {
    CutTSS classifier;
    RunDuplicatePriorityDeleteCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        pc.ConstructClassifier(rules);
    });
}

TEST(UpdateCorrectnessTest, CutTSSInsertIntoEmptySubsetCreatesSearchPath) {
    const Rule catch_all = MakeCatchAllRule(1, 1);
    const Rule inserted = MakeExactSrcRule(10, 2, 0x0A000001);

    CutTSS classifier;
    std::vector<Rule> active_rules = {catch_all};
    const std::vector<Packet> packets = {
        MakePacket(0x0A000001, 80),
        MakePacket(0x0A000002, 80),
    };

    classifier.ConstructClassifier(active_rules);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "empty subset before insert");

    classifier.InsertRule(inserted);
    active_rules.push_back(inserted);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "empty subset after insert");

    classifier.DeleteRule(inserted);
    RemoveRule(active_rules, inserted);
    ExpectClassifierMatchesBaseline(classifier, active_rules, packets, "empty subset after delete");
}

TEST(UpdateCorrectnessTest, TupleMergeInsertDeleteMatchesLinearScanBaseline) {
    TupleMergeOnline classifier;
    RunStagedUpdateCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        pc.ConstructClassifier(rules);
    });
}

TEST(UpdateCorrectnessTest, TupleMergeDuplicatePriorityDeleteKeepsNeighbor) {
    TupleMergeOnline classifier;
    RunDuplicatePriorityDeleteCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        pc.ConstructClassifier(rules);
    });
}

TEST(UpdateCorrectnessTest, HybridTSSHashInsertDeleteMatchesLinearScanBaseline) {
    HybridOptions options;
    options.binth = 1;
    options.rtssleaf = 0.1;
    HybridTSS classifier(options);

    RunStagedUpdateCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        static_cast<HybridTSS&>(pc).ConstructBaseline(rules);
    });
}

TEST(UpdateCorrectnessTest, HybridTSSDuplicatePriorityDeleteKeepsNeighbor) {
    HybridOptions options;
    options.binth = 1;
    options.rtssleaf = 0.1;
    HybridTSS classifier(options);

    RunDuplicatePriorityDeleteCorrectness(classifier, [](PacketClassifier& pc, const std::vector<Rule>& rules) {
        static_cast<HybridTSS&>(pc).ConstructBaseline(rules);
    });
}
