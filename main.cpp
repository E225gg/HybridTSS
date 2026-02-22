#include <iostream>
#include <fstream>
#include <string>
//#include "./OVS/TupleSpaceSearch.h"
#include "ElementaryClasses.h"
#include "./HybridTSS/HybridTSS.h"
#include "./CutTSS/CutTSS.h"
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

void testPerformance(PacketClassifier *p) {
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
    printf("\nUpdate Performance:\n");

    if (randUpdate.empty()) {
        nInsert = nDelete = 0;
        for (int ra = 0; ra < rules.size(); ra ++) {
            int t = rand() % 2;
            randUpdate.push_back(t);
            t ? nDelete ++ : nInsert++;
        }
    }
    Start = std::chrono::steady_clock::now();
    for (int ra = 0; ra < rules.size(); ra++) {
        randUpdate[ra] ? p->DeleteRule(rules[ra]) : p->InsertRule(rules[ra]);
    }
    End = std::chrono::steady_clock::now();
    elapsed_seconds = End - Start;
    int nrules = static_cast<int>(rules.size());
    double updateThroughput = 1 / (elapsed_seconds.count() * 1e6 / nrules);
    double avgUpdateTime = elapsed_seconds.count() * 1e6 / nrules;

    printf("\t%d rules update: insert_num = %d delete_num = %d\n", nrules, nInsert, nDelete);
    printf("\tTotal update time: %f s\n", elapsed_seconds.count());
    printf("\tAverage update time: %f us\n", avgUpdateTime);
    printf("\tThroughput: %f Mpps\n", updateThroughput);
    printf("-------------------------------\n\n");

    // Write CSV metrics row
    if (fMetrics.is_open()) {
        fMetrics << p->funName() << ","
                 << ruleFile << ","
                 << nrules << ","
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
    for (int i = 0; i < argc; i++) {
        if (string(argv[i]) == "-r") {
            if (i + 1 >= argc) {
                cerr << "Error: -r requires a file path argument" << endl;
                return 1;
            }
            ruleFile = string(argv[++ i]);
            fpr = fopen(argv[i], "r");
            if (!fpr) {
                cerr << "Error: cannot open rule file: " << ruleFile << endl;
                return 1;
            }
        }
        else if (string(argv[i]) == "-p") {
            if (i + 1 >= argc) {
                cerr << "Error: -p requires a file path argument" << endl;
                return 1;
            }
            packetFile = string(argv[++ i]);
            fpt = fopen(argv[i], "r");
            if (!fpt) {
                cerr << "Error: cannot open packet file: " << packetFile << endl;
                if (fpr) fclose(fpr);
                return 1;
            }
        }
    }

    if (!fpr || !fpt) {
        cerr << "Usage: " << argv[0] << " -r <rule_file> -p <packet_file>" << endl;
        if (fpr) fclose(fpr);
        if (fpt) fclose(fpt);
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
    fMetrics.open("results.csv", ios::out);
    if (fMetrics.is_open()) {
        fMetrics << "classifier,ruleset,num_rules,num_packets,"
                 << "construction_time_ms,avg_classify_us,classify_mpps,"
                 << "misclassified,avg_update_us,update_mpps\n";
    }

    // ---HybridTSS---Construction---
//    PacketClassifier *HT = new HybridTSS();
    // PacketClassifier *TMO = new TupleMergeOnline();
    // testPerformance(TMO);
    PacketClassifier *PSTSS = new PriorityTupleSpaceSearch();
    testPerformance(PSTSS);
    PacketClassifier *CT = new CutTSS();
    testPerformance(CT);

    // ---test---
//    cout <<"---------------------" << endl;
    PacketClassifier *HT = new HybridTSS();
    testPerformance(HT);
//    fout << endl;

    delete PSTSS;
    delete CT;
    delete HT;

    if (fMetrics.is_open()) {
        fMetrics.close();
    }

    return 0;
}
