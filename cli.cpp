#include "cli.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_set>

using namespace std;

vector<string> split(const string& s, char delim) {
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

void usage(const char* prog) {
    cerr << "Usage: " << prog << " -r <rule_file> -p <packet_file> [options]\n"
         << "  --classifier <name[,name]>   Select classifiers (pstss,cuttss,hybrid), default: all\n"
         << "  --trials <N>                Classification trials per classifier (default 10)\n"
         << "  --skip-updates              Skip update benchmark (default: run)\n"
         << "  --run-updates               Force running update benchmark\n"
         << "  --seed <u64>                RNG seed for updates (default 1)\n"
         << "  --metrics <path>            Metrics CSV path (default results.csv)\n"
         << "  --append-metrics            Append to metrics file instead of overwrite\n"
         << "  --ht-binth <int>            HybridTSS linear threshold (default 8)\n"
         << "  --ht-rtssleaf <float>       HybridTSS tuple-merge threshold (default 1.5)\n"
         << "  --ht-loop <int>             HybridTSS training episodes (default 50)\n"
         << "  --ht-lr <float>             HybridTSS learning rate (default 0.05)\n"
         << "  --ht-decay <float>          HybridTSS learning rate decay (default 0.001)\n"
         << "  --ht-epsilon0 <float>       HybridTSS initial epsilon (default 0.5)\n"
         << "  --ht-epsilon-min <float>    HybridTSS min epsilon (default 0.01)\n"
         << "  --ht-epsilon-decay <float>  HybridTSS epsilon decay (default 0.003)\n"
         << "  --ht-state-bits <int>       HybridTSS state bits (default 20)\n"
         << "  --ht-action-bits <int>      HybridTSS action bits (default 6)\n"
         << "  --ht-hash-inflation <int>   HybridTSS hash inflation (default 10)\n"
         << "  --ht-seed <u64>             HybridTSS training seed (0 => time-based)\n"
         << "  --ht-train-online <0|1>     HybridTSS train during construct (default 1)\n"
         << "  --ht-qtable-in <path>       HybridTSS QTable input for inference-only\n"
         << "  --ht-qtable-out <path>      HybridTSS QTable output after training\n";
}

bool parse_args(int argc, char* argv[], Options& opts) {
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
            try {
                opts.trials = stoi(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--trials requires an integer" << endl; return false;
            }
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
        } else if (arg == "--ht-binth") {
            if (i + 1 >= argc) { cerr << "--ht-binth requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.binth = stoi(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-binth requires an integer" << endl; return false;
            }
        } else if (arg == "--ht-rtssleaf") {
            if (i + 1 >= argc) { cerr << "--ht-rtssleaf requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.rtssleaf = stod(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-rtssleaf requires a number" << endl; return false;
            }
        } else if (arg == "--ht-loop") {
            if (i + 1 >= argc) { cerr << "--ht-loop requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.loop_num = stoi(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-loop requires an integer" << endl; return false;
            }
        } else if (arg == "--ht-lr") {
            if (i + 1 >= argc) { cerr << "--ht-lr requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.lr = stod(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-lr requires a number" << endl; return false;
            }
        } else if (arg == "--ht-decay") {
            if (i + 1 >= argc) { cerr << "--ht-decay requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.decay = stod(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-decay requires a number" << endl; return false;
            }
        } else if (arg == "--ht-epsilon0") {
            if (i + 1 >= argc) { cerr << "--ht-epsilon0 requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.epsilon0 = stod(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-epsilon0 requires a number" << endl; return false;
            }
        } else if (arg == "--ht-epsilon-min") {
            if (i + 1 >= argc) { cerr << "--ht-epsilon-min requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.epsilon_min = stod(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-epsilon-min requires a number" << endl; return false;
            }
        } else if (arg == "--ht-epsilon-decay") {
            if (i + 1 >= argc) { cerr << "--ht-epsilon-decay requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.epsilon_decay = stod(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-epsilon-decay requires a number" << endl; return false;
            }
        } else if (arg == "--ht-state-bits") {
            if (i + 1 >= argc) { cerr << "--ht-state-bits requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.state_bits = stoi(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-state-bits requires an integer" << endl; return false;
            }
        } else if (arg == "--ht-action-bits") {
            if (i + 1 >= argc) { cerr << "--ht-action-bits requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.action_bits = stoi(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-action-bits requires an integer" << endl; return false;
            }
        } else if (arg == "--ht-hash-inflation") {
            if (i + 1 >= argc) { cerr << "--ht-hash-inflation requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.hash_inflation = stoi(argv[++i]);
            } catch (const std::exception&) {
                cerr << "--ht-hash-inflation requires an integer" << endl; return false;
            }
        } else if (arg == "--ht-seed") {
            if (i + 1 >= argc) { cerr << "--ht-seed requires a value" << endl; return false; }
            try {
                opts.hybrid_opts.seed = strtoull(argv[++i], nullptr, 10);
            } catch (const std::exception&) {
                cerr << "--ht-seed requires an integer" << endl; return false;
            }
        } else if (arg == "--ht-train-online") {
            if (i + 1 >= argc) { cerr << "--ht-train-online requires 0 or 1" << endl; return false; }
            string value = argv[++i];
            if (value == "0") {
                opts.hybrid_opts.train_online = false;
            } else if (value == "1") {
                opts.hybrid_opts.train_online = true;
            } else {
                cerr << "--ht-train-online expects 0 or 1" << endl;
                return false;
            }
        } else if (arg == "--ht-qtable-in") {
            if (i + 1 >= argc) { cerr << "--ht-qtable-in requires a value" << endl; return false; }
            opts.hybrid_opts.qtable_in_path = argv[++i];
        } else if (arg == "--ht-qtable-out") {
            if (i + 1 >= argc) { cerr << "--ht-qtable-out requires a value" << endl; return false; }
            opts.hybrid_opts.qtable_out_path = argv[++i];
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

    const bool runHybrid = std::find(opts.classifiers.begin(), opts.classifiers.end(), "hybrid") != opts.classifiers.end();
    if (runHybrid && !opts.hybrid_opts.train_online && opts.hybrid_opts.qtable_in_path.empty()) {
        cerr << "--ht-train-online 0 requires --ht-qtable-in <path>" << endl;
        return false;
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
