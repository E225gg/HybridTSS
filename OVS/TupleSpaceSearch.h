#ifndef TUPLE_H
#define TUPLE_H

#define POINTER_SIZE_BYTES 4

#include "cmap.h"
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <memory>
#include <vector>
#include "MapExtensions.h"

struct Tuple {
  public:
    Tuple(const std::vector<int>& dims, const std::vector<unsigned int>& lengths, const Rule& r) : dims(dims), lengths(lengths) {
        for (int w : lengths) {
            tuple.push_back(w);
        }
        cmap_init(&map_in_tuple);
        Insertion(r);
    }
    ~Tuple() { Destroy(); }

    // Non-copyable: cmap has internal pointers — shallow copy causes double-free
    Tuple(const Tuple&) = delete;
    Tuple& operator=(const Tuple&) = delete;

    // Move semantics: transfer cmap ownership, null out source
    Tuple(Tuple&& other) noexcept
        : map_in_tuple(other.map_in_tuple),
          dims(std::move(other.dims)),
          lengths(std::move(other.lengths)),
          tuple(std::move(other.tuple))
    {
        other.map_in_tuple.impl = nullptr;
    }
    Tuple& operator=(Tuple&& other) noexcept {
        if (this != &other) {
            Destroy();
            map_in_tuple = other.map_in_tuple;
            dims = std::move(other.dims);
            lengths = std::move(other.lengths);
            tuple = std::move(other.tuple);
            other.map_in_tuple.impl = nullptr;
        }
        return *this;
    }

    void Destroy() {
        if (!map_in_tuple.impl) return; // moved-from or already destroyed
        // Walk and delete all dynamically-allocated cmap_node objects
        cmap_cursor cursor = cmap_cursor_start(&map_in_tuple);
        while (cursor.node != nullptr) {
            cmap_node* node = cursor.node;
            cmap_cursor_advance(&cursor);
            delete node;
        }
        cmap_destroy(&map_in_tuple);
    }

    bool IsEmpty() {
        return CountNumRules() == 0;
    }

    int FindMatchPacket(const Packet& p);
    void Insertion(const Rule& r);
    void Deletion(const Rule& r);
    int WorstAccesses() const;
    int CountNumRules() const {
        return cmap_count(&map_in_tuple);
        //	return  table.size();
    }
    Memory MemSizeBytes(Memory ruleSizeBytes) const {
        return cmap_count(&map_in_tuple) * ruleSizeBytes + cmap_array_size(&map_in_tuple) * POINTER_SIZE_BYTES;
        // return table.size() * ruleSizeBytes + table.bucket_count() * POINTER_SIZE_BYTES;
    }

    void printsipdip() {
        // printf("sipdip: %d %d\n", sip_length, dip_length);
    }

  protected:
    bool inline IsPacketMatchToRule(const Packet& p, const Rule& r);
    unsigned int inline HashRule(const Rule& r) const;
    unsigned int inline HashPacket(const Packet& p) const;
    cmap map_in_tuple;
    // std::unordered_map<uint32_t, std::vector<Rule>> table;

    std::vector<int> dims;
    std::vector<unsigned int> lengths;
    std::vector<int> tuple;
};

struct PriorityTuple : public Tuple {
  public:
    PriorityTuple(const std::vector<int>& dims, const std::vector<unsigned int>& lengths, const Rule& r) : Tuple(dims, lengths, r) {
        maxPriority = r.priority;
        priority_container.insert(maxPriority);
    }
    void Insertion(const Rule& r, bool& priority_change);
    void Deletion(const Rule& r, bool& priority_change);

    int maxPriority = -1;
    std::multiset<int> priority_container;
};

class TupleSpaceSearch : public PacketClassifier {

  public:
    virtual ~TupleSpaceSearch() = default;

    void ConstructClassifier(const std::vector<Rule>& r);
    int ClassifyAPacket(const Packet& one_packet);
    void DeleteRule(const Rule& rule);
    void InsertRule(const Rule& rule);

    int MemoryAccess() const {
        return WorstAccesses();
    }
    virtual int WorstAccesses() const;
    Memory MemSizeBytes() const {
        int ruleSizeBytes = sizeof(Rule);
        int sizeBytes = 0;
        for (auto& pair : all_tuples) {
            sizeBytes += pair.second.MemSizeBytes(ruleSizeBytes);
        }
        return sizeBytes;
    }
    void PlotTupleDistribution() {

        // Sort by rule count descending using pointers (Tuple is non-copyable)
        std::vector<const Tuple*> v;
        v.reserve(all_tuples.size());
        for (const auto& pair : all_tuples) {
            v.push_back(&pair.second);
        }

        std::sort(v.begin(), v.end(), [](const Tuple* lhs, const Tuple* rhs) {
            return lhs->CountNumRules() > rhs->CountNumRules();
        });

        std::ofstream log("logfile.txt", std::ios_base::app | std::ios_base::out);
        int sum = 0;
        for (const auto* t : v) {
            sum += t->CountNumRules();
        }
        log << v.size() << " " << sum << " ";
        int left = 5;
        for (const auto* t : v) {
            log << t->CountNumRules() << " ";
            if (--left <= 0)
                break;
        }
        log << std::endl;
    }
    virtual int GetNumberOfTuples() const {
        return all_tuples.size();
    }
    size_t NumTables() const {
        return GetNumberOfTuples();
    }
    size_t RulesInTable(size_t index) const {
        if (index >= all_tuples.size()) {
            return 0;
        }
        // Provide stable ordering by iterating sorted keys
        std::vector<uint64_t> keys;
        keys.reserve(all_tuples.size());
        for (const auto& kv : all_tuples) {
            keys.push_back(kv.first);
        }
        std::sort(keys.begin(), keys.end());
        if (index >= keys.size()) {
            return 0;
        }
        auto hit = all_tuples.find(keys[index]);
        if (hit == all_tuples.end()) {
            return 0;
        }
        return hit->second.CountNumRules();
    }
  protected:
    uint64_t inline KeyRulePrefix(const Rule& r) {
        int key = 0;
        for (int d : dims) {
            key <<= 6;
            key += r.prefix_length[d];
        }
        return key;
    }
    std::unordered_map<uint64_t, Tuple> all_tuples;
    // maintain rules for monitoring purpose
    std::vector<Rule> rules;
    std::vector<int> dims;
};

class PriorityTupleSpaceSearch : public TupleSpaceSearch {

  public:
    ~PriorityTupleSpaceSearch() override {
        for (auto& pair : all_priority_tuples) {
            delete pair.second;
        }
        all_priority_tuples.clear();
        priority_tuples_vector.clear();
    }

    int ClassifyAPacket(const Packet& one_packet);
    void DeleteRule(const Rule& rule);
    void InsertRule(const Rule& one_rule);
    int WorstAccesses() const;
    Memory MemSizeBytes() const {
        int ruleSizeBytes = sizeof(Rule);
        int sizeBytes = 0;
        for (auto& tuple : priority_tuples_vector) {
            sizeBytes += tuple->MemSizeBytes(ruleSizeBytes);
        }
        return sizeBytes + rules.size() * 16;
    }

    int GetNumberOfTuples() const {
        return all_priority_tuples.size();
    }
    void PlotPriorityTupleDistribution() {

        RetainInvaraintOfPriorityVector();
        std::ofstream log("logfile.txt", std::ios_base::app | std::ios_base::out);
        int sum = 0;
        for (auto x : priority_tuples_vector) {
            sum += x->CountNumRules();
        }
        log << priority_tuples_vector.size() << " " << sum << " ";
        int left = 5;
        for (auto x : priority_tuples_vector) {
            log << x->CountNumRules() << " ";
            left--;
            if (left <= 0)
                break;
        }
        log << std::endl;
    }

    size_t NumTables() const {
        return priority_tuples_vector.size();
    }
    size_t RulesInTable(size_t index) const {
        return priority_tuples_vector[index]->CountNumRules();
    }

    void prints() {
        double memSize = double(MemSizeBytes());
        int numRules = rules.size();
        printf("\trules: %d \n", numRules);
        printf("\ttuples: %ld \n", NumTables());
        printf("\tSize(KB): %f \n", memSize / 1024);
        printf("\tByte/rule: %f \n", memSize / numRules);
    }

    std::string funName() override {
        return "class: PSTSS";
    }

  private:
    void RetainInvaraintOfPriorityVector() {
        std::sort(begin(priority_tuples_vector), end(priority_tuples_vector), [](PriorityTuple* lhs, PriorityTuple* rhs) { return lhs->maxPriority > rhs->maxPriority; });
    }
    std::unordered_map<uint64_t, PriorityTuple*> all_priority_tuples;
    std::vector<PriorityTuple*> priority_tuples_vector;
};

#endif
