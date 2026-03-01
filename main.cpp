#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <sstream>
#include <unordered_set>
//#include "./OVS/TupleSpaceSearch.h"
#include "ElementaryClasses.h"
#include "./HybridTSS/HybridTSS.h"
#include "./CutTSS/CutTSS.h"
#include "cli.h"
using namespace std;

string ruleFile, packetFile;
FILE *fpr = nullptr, *fpt = nullptr;
vector<Rule> rules;
vector<Packet> packets;
std::chrono::time_point<std::chrono::steady_clock> Start, End;
std::chrono::duration<double> elapsed_seconds{};
std::chrono::duration<double,std::milli> elapsed_milliseconds{};
vector<int> randUpdate;

int nInsert, nDelete;
ofstream fError("ErrorLog.csv", ios::app);
ofstream fMetrics;  // CSV metrics output


bool testPerformance(PacketClassifier *p, const Options& opts, const vector<int>& updatePlan, bool isHybrid) {
    cout << p->funName() << ":" << endl;
    Start = std::chrono::steady_clock::now();
    if (isHybrid) {
        auto* hybrid = dynamic_cast<HybridTSS*>(p);
        if (!hybrid) {
            cerr << "Internal error: expected HybridTSS instance" << endl;
            return false;
        }
        string err;
        if (!hybrid->ConstructClassifierSafe(rules, &err)) {
            cerr << "Failed to construct HybridTSS: " << err << endl;
            return false;
        }
    } else {
        p->ConstructClassifier(rules);
    }
    End = std::chrono::steady_clock::now();
    elapsed_milliseconds = End - Start;
    double constructTime = elapsed_milliseconds.count();
    cout << "\tConstruction time: " << constructTime <<" ms " << endl;

    // Classify
    printf("\nClassify Performance:\n");
    std::chrono::duration<double> sumTime(0);
    int matchPri = -1, matchMiss = 0;
    int nPacket = int(packets.size());
    int nRules = int(rules.size());
    vector<int> results(nPacket, -1);
    const int trials = opts.trials;
    for (int i = 0; i < trials; i++) {
        Start = std::chrono::steady_clock::now();
        for (int j = 0; j < nPacket; j++) {
            matchPri = p->ClassifyAPacket(packets[j]);
            results[j] = nRules - 1 - matchPri;
        }
        End = std::chrono::steady_clock::now();
        sumTime += End - Start;
        for (int j = 0; j < nPacket; j++) {
            if (results[j] == nRules || packets[j][5] < results[j]) {
                cout << rules[packets[j][5]].priority << "\t" << results[j] << "\t" << packets[j][5] << endl;
            matchMiss++;
            }
        }
    }

    double classifyThroughput = 1 / (sumTime.count() * 1e6 / (trials * nPacket));
    double avgClassifyTime = sumTime.count() * 1e6 / (trials * nPacket);

    printf("\t%d packets are classified, %d of them are misclassified\n", nPacket * trials, matchMiss);
    printf("\tTotal classification time: %f s\n", sumTime.count() / trials);
    printf("\tAverage classification time: %f us\n", avgClassifyTime);
    printf("\tThroughput: %f Mpps\n", classifyThroughput);

    // Update
    double avgUpdateTime = 0.0;
    double updateThroughput = 0.0;
    if (opts.run_updates) {
        printf("\nUpdate Performance:\n");

        Start = std::chrono::steady_clock::now();
        for (int ra = 0; ra < rules.size(); ra++) {
            updatePlan[ra] ? p->DeleteRule(rules[ra]) : p->InsertRule(rules[ra]);
        }
        End = std::chrono::steady_clock::now();
        elapsed_seconds = End - Start;
        int nrules = static_cast<int>(rules.size());
        updateThroughput = 1 / (elapsed_seconds.count() * 1e6 / nrules);
        avgUpdateTime = elapsed_seconds.count() * 1e6 / nrules;

        printf("\t%d rules update: insert_num = %d delete_num = %d\n", nrules, nInsert, nDelete);
        printf("\tTotal update time: %f s\n", elapsed_seconds.count());
        printf("\tAverage update time: %f us\n", avgUpdateTime);
        printf("\tThroughput: %f Mpps\n", updateThroughput);
        printf("-------------------------------\n\n");
    } else {
        printf("\nUpdate Performance: skipped (per --skip-updates)\n");
        printf("-------------------------------\n\n");
    }

    // Write CSV metrics row
    if (fMetrics.is_open()) {
        int nrules_for_csv = static_cast<int>(rules.size());
        fMetrics << p->funName() << ","
                 << ruleFile << ","
                 << nrules_for_csv << ","
                 << nPacket << ","
                 << constructTime << ","
                 << avgClassifyTime << ","
                 << classifyThroughput << ","
                 << matchMiss << ","
                 << avgUpdateTime << ","
                 << updateThroughput;

        if (isHybrid) {
            const auto& h = opts.hybrid_opts;
            fMetrics << "," << h.binth
                     << "," << h.rtssleaf
                     << "," << h.loop_num
                     << "," << h.lr
                     << "," << h.decay
                     << "," << h.epsilon0
                     << "," << h.epsilon_min
                     << "," << h.epsilon_decay
                     << "," << h.state_bits
                     << "," << h.action_bits
                     << "," << h.hash_inflation
                     << "," << h.seed;
        } else {
            for (int i = 0; i < 12; i++) {
                fMetrics << ",";
            }
        }
        fMetrics << "\n";
    }
    return true;
}

int main(int argc, char* argv[]) {
    Options opts;
    if (!parse_args(argc, argv, opts)) {
        usage(argv[0]);
        return 1;
    }

    ruleFile = opts.rule_file;
    packetFile = opts.packet_file;
    fpr = fopen(opts.rule_file.c_str(), "r");
    fpt = fopen(opts.packet_file.c_str(), "r");
    if (!fpr || !fpt) {
        if (fpr) fclose(fpr);
        if (fpt) fclose(fpt);
        cerr << "Error: cannot open input files" << endl;
        return 1;
    }

//    fout << ruleFile << ",";
    cout << ruleFile << endl;
    cout << packetFile << endl;
    rules = loadrule(fpr);
    fclose(fpr);
    fpr = nullptr;
    packets = loadpacket(fpt);
    fclose(fpt);
    fpt = nullptr;
    cout << rules.size() << endl;
    cout << packets.size() << endl;

    // Open CSV metrics file
    auto metricsMode = opts.append_metrics ? ios::out | ios::app : ios::out;
    fMetrics.open(opts.metrics_path, metricsMode);
    if (fMetrics.is_open()) {
        bool needHeader = !opts.append_metrics;
        if (opts.append_metrics) {
            // Write header if file is empty
            fMetrics.seekp(0, ios::end);
            if (fMetrics.tellp() == 0) {
                needHeader = true;
            }
        }
        if (needHeader) {
            fMetrics << "classifier,ruleset,num_rules,num_packets,"
                     << "construction_time_ms,avg_classify_us,classify_mpps,"
                     << "misclassified,avg_update_us,update_mpps,"
                     << "ht_binth,ht_rtssleaf,ht_loop,ht_lr,ht_decay,ht_epsilon0,ht_epsilon_min,ht_epsilon_decay,ht_state_bits,ht_action_bits,ht_hash_inflation,ht_seed\n";
        }
    }

    bool hasFailures = false;
    for (const auto& clf : opts.classifiers) {
        // fresh update plan per classifier run to avoid cross-run mutation
        randUpdate.clear();
        nInsert = nDelete = 0;
        std::mt19937_64 gen(opts.seed);
        std::uniform_int_distribution<int> coin(0, 1);
        for (size_t i = 0; i < rules.size(); i++) {
            int t = coin(gen);
            randUpdate.push_back(t);
            t ? nDelete++ : nInsert++;
        }

        if (clf == "pstss") {
            PacketClassifier *PSTSS = new PriorityTupleSpaceSearch();
            if (!testPerformance(PSTSS, opts, randUpdate, false)) {
                hasFailures = true;
            }
            delete PSTSS;
        } else if (clf == "cuttss") {
            PacketClassifier *CT = new CutTSS();
            if (!testPerformance(CT, opts, randUpdate, false)) {
                hasFailures = true;
            }
            delete CT;
        } else if (clf == "hybrid") {
            PacketClassifier *HT = new HybridTSS(opts.hybrid_opts);
            if (!testPerformance(HT, opts, randUpdate, true)) {
                hasFailures = true;
            }
            delete HT;
        } else {
            cerr << "Unknown classifier name: " << clf << endl;
            hasFailures = true;
        }
    }

    if (fMetrics.is_open()) {
        fMetrics.close();
    }

    return hasFailures ? 1 : 0;
}
