#pragma once

#include <string>
#include <vector>
#include "HybridTSS/HybridTSS.h"

struct Options {
    std::string rule_file;
    std::string packet_file;
    std::vector<std::string> classifiers; // names: pstss, cuttss, hybrid
    int trials = 10;
    bool run_updates = true;
    uint64_t seed = 1;
    std::string metrics_path = "results.csv";
    bool append_metrics = false;
    HybridOptions hybrid_opts;
};

std::vector<std::string> split(const std::string& s, char delim);
void usage(const char* prog);
bool parse_args(int argc, char* argv[], Options& opts);
