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
using namespace std;

struct Options {
    string rule_file;
    string packet_file;
    vector<string> classifiers; // names: pstss, cuttss, hybrid
    int trials = 10;
    bool run_updates = true;
    uint64_t seed = 1;
    string metrics_path = "results.csv";
    bool append_metrics = false;
};

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

static vector<string> split(const string& s, char delim) {
    vector<string> out;
    string item;
    std::istringstream ss(s);
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            out.push_back(item);
        }
    }
    return out;
}

static void usage(const char* prog) {
    cerr << "Usage: " << prog << " -r <rule_file> -p <packet_file> [options]\n"
         << "  --classifier <name[,name]>   Select classifiers (pstss,cuttss,hybrid), default: all\n"
         << "  --trials <N>                Classification trials per classifier (default 10)\n"
         << "  --skip-updates              Skip update benchmark (default: run)\n"
         << "  --run-updates               Force running update benchmark\n"
         << "  --seed <u64>                RNG seed for updates (default 1)\n"
         << "  --metrics <path>            Metrics CSV path (default results.csv)\n"
         << "  --append-metrics            Append to metrics file instead of overwrite\n";
}

static bool parse_args(int argc, char* argv[], Options& opts) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-r") {
            if (i + 1 >= argc) { cerr << "-r requires a file path" << endl; return false; }
            opts.rule_file = argv[++i];
        } else if (arg == "-p") {
            if (i + 1 >= argc) { cerr << "-p requires a file path" << endl; return false; }
            opts.packet_file = argv[++i];
        } else if (arg == "--classifier" || arg == "--classifiers") {
            if (i + 1 >= argc) { cerr << "--classifier requires a value" << endl; return false; }
            auto parts = split(argv[++i], ',');
            opts.classifiers.insert(opts.classifiers.end(), parts.begin(), parts.end());
        } else if (arg == "--trials") {
            if (i + 1 >= argc) { cerr << "--trials requires a value" << endl; return false; }
            opts.trials = stoi(argv[++i]);
            if (opts.trials <= 0) { cerr << "--trials must be > 0" << endl; return false; }
        } else if (arg == "--skip-updates") {
            opts.run_updates = false;
        } else if (arg == "--run-updates") {
            opts.run_updates = true;
        } else if (arg == "--seed") {
            if (i + 1 >= argc) { cerr << "--seed requires a value" << endl; return false; }
            opts.seed = strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--metrics") {
            if (i + 1 >= argc) { cerr << "--metrics requires a value" << endl; return false; }
            opts.metrics_path = argv[++i];
        } else if (arg == "--append-metrics") {
            opts.append_metrics = true;
        } else {
            cerr << "Unknown argument: " << arg << endl;
            return false;
        }
    }

    if (opts.rule_file.empty() || opts.packet_file.empty()) {
        return false;
    }

    if (opts.classifiers.empty()) {
        opts.classifiers = {"pstss", "cuttss", "hybrid"};
    }

    // de-duplicate
    unordered_set<string> seen;
    vector<string> uniq;
    for (const auto& c : opts.classifiers) {
        if (seen.insert(c).second) {
            uniq.push_back(c);
        }
    }
    opts.classifiers.swap(uniq);
    return true;
}

void testPerformance(PacketClassifier *p, const Options& opts, const vector<int>& updatePlan) {
    cout << p->funName() << ":" << endl;
    Start = std::chrono::steady_clock::now();
    p->ConstructClassifier(rules);
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
    const int trials = 10;
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
                matchMiss ++;
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
                 << updateThroughput << "\n";
    }
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
    if (fMetrics.is_open() && !opts.append_metrics) {
        fMetrics << "classifier,ruleset,num_rules,num_packets,"
                 << "construction_time_ms,avg_classify_us,classify_mpps,"
                 << "misclassified,avg_update_us,update_mpps\n";
    }

    // Prepare deterministic update plan
    randUpdate.clear();
    nInsert = nDelete = 0;
    std::mt19937_64 gen(opts.seed);
    std::uniform_int_distribution<int> coin(0, 1);
    for (size_t i = 0; i < rules.size(); i++) {
        int t = coin(gen);
        randUpdate.push_back(t);
        t ? nDelete++ : nInsert++;
    }

    for (const auto& clf : opts.classifiers) {
        if (clf == "pstss") {
            PacketClassifier *PSTSS = new PriorityTupleSpaceSearch();
            testPerformance(PSTSS, opts, randUpdate);
            delete PSTSS;
        } else if (clf == "cuttss") {
            PacketClassifier *CT = new CutTSS();
            testPerformance(CT, opts, randUpdate);
            delete CT;
        } else if (clf == "hybrid") {
            PacketClassifier *HT = new HybridTSS();
            testPerformance(HT, opts, randUpdate);
            delete HT;
        } else {
            cerr << "Unknown classifier name: " << clf << endl;
        }
    }

    if (fMetrics.is_open()) {
        fMetrics.close();
    }

    return 0;
}
