//
// Created by GIGABYTE on 2022/2/28.
//

#ifndef HYBRIDTSSV1_2_ELEMENTARYCLASSES_H
#define HYBRIDTSSV1_2_ELEMENTARYCLASSES_H
#include <vector>
#include <queue>
#include <list>
#include <set>
#include <map>
#include <stack>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <random>
#include <numeric>
#include <math.h>
#include <memory>
#include <chrono>
#include <array>
#include <climits>

// base
#define RULE_SIZE 16
#define NODE_SIZE 32
#define LEAF_NODE_SIZE 4
#define TREE_NODE_SIZE 8
#define Null -1
#define PTR_SIZE 4
#define HEADER_SIZE 4
#define RULESIZE 4.5

#define MAXDIMENSIONS 5
#define FieldSA 0
#define FieldDA 1
#define FieldSP 2
#define FieldDP 3
#define FieldProto 4

#define LowDim 0
#define HighDim 1

typedef uint32_t Point;
typedef std::vector<Point> Packet;
typedef uint32_t Memory;

struct Rule
        {
    //Rule(){};
    explicit Rule(int dim = 5) : dim(dim), range(dim, { { 0, 0 } }), prefix_length(dim, 0) { markedDelete = 0; }

    int dim;
    int	priority{};

    int id{};
    int tag{};
    bool markedDelete = 0;

    std::vector<unsigned> prefix_length;

    std::vector<std::array<Point,2>> range;

    bool inline MatchesPacket(const Packet& p) const {
        for (int i = 0; i < dim; i++) {
            if ((p[i] < range[i][LowDim]) || p[i] > range[i][HighDim]) return false;
        }
        return true;
    }

    bool operator<(const Rule& r) const{
        return priority > r.priority;
    }

    bool operator == (const Rule& r) const {
        return r.priority == priority;
    }

    void Print() const {
        printf("rulePri:%d", priority);
        for (int i = 0; i < dim; i++) {
            printf("\t%u:%u/%u ", range[i][LowDim], range[i][HighDim], prefix_length[i]);
        }
        printf("\n");
    }
};


/*
 * ===  FUNCTION  ======================================================================
 *         Name:  loadrule(FILE *fp)
 *  Description:  load rules from rule file
 * =====================================================================================
 */
static std::vector<Rule> loadrule(FILE *fp)
{
    unsigned int tmp;
    unsigned sip1,sip2,sip3,sip4,smask;
    unsigned dip1,dip2,dip3,dip4,dmask;
    unsigned sport1,sport2;
    unsigned dport1,dport2;
    unsigned protocal,protocol_mask;
    unsigned ht, htmask;
    int number_rule=0; //number of rules

    std::vector<Rule> rule;

    while(true){

        Rule r;
        std::array<Point,2> points{};
        if(fscanf(fp,"@%u.%u.%u.%u/%u\t%u.%u.%u.%u/%u\t%u : %u\t%u : %u\t%x/%x\t%x/%x\n",
                  &sip1, &sip2, &sip3, &sip4, &smask, &dip1, &dip2, &dip3, &dip4, &dmask, &sport1, &sport2,
                  &dport1, &dport2,&protocal, &protocol_mask, &ht, &htmask)!= 18) break;

        r.prefix_length[0] = smask;
        r.prefix_length[1] = dmask;

        if(smask == 0){
            points[0] = 0;
            points[1] = 0xFFFFFFFF;
        }else if(smask > 0 && smask <= 8){
            tmp = sip1<<24;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-smask)) - 1;
        }else if(smask > 8 && smask <= 16){
            tmp = sip1<<24; tmp += sip2<<16;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-smask)) - 1;
        }else if(smask > 16 && smask <= 24){
            tmp = sip1<<24; tmp += sip2<<16; tmp +=sip3<<8;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-smask)) - 1;
        }else if(smask > 24 && smask <= 32){
            tmp = sip1<<24; tmp += sip2<<16; tmp += sip3<<8; tmp += sip4;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-smask)) - 1;
        }else{
            printf("Src IP length exceeds 32\n");
            exit(-1);
        }
        r.range[0] = points;

        if(dmask == 0){
            points[0] = 0;
            points[1] = 0xFFFFFFFF;
        }else if(dmask > 0 && dmask <= 8){
            tmp = dip1<<24;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-dmask)) - 1;
        }else if(dmask > 8 && dmask <= 16){
            tmp = dip1<<24; tmp +=dip2<<16;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-dmask)) - 1;
        }else if(dmask > 16 && dmask <= 24){
            tmp = dip1<<24; tmp +=dip2<<16; tmp+=dip3<<8;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-dmask)) - 1;
        }else if(dmask > 24 && dmask <= 32){
            tmp = dip1<<24; tmp +=dip2<<16; tmp+=dip3<<8; tmp +=dip4;
            points[0] = tmp;
            points[1] = points[0] + (1<<(32-dmask)) - 1;
        }else{
            printf("Dest IP length exceeds 32\n");
            exit(-1);
        }
        r.range[1] = points;

        points[0] = sport1;
        points[1] = sport2;
        r.range[2] = points;

        points[0] = dport1;
        points[1] = dport2;
        r.range[3] = points;

        for(int i = 15; i >= 0; i--)
        {
            unsigned int Bit = 1 << i;
            unsigned sp = sport1 ^ sport2;
            if(sp & Bit)
            {
                break;
            }
            r.prefix_length[2]++;
        }

        for(int i = 15; i >= 0; i--)
        {
            unsigned int Bit = 1 << i;
            unsigned dp = dport1 ^ dport2;
            if(dp & Bit)
            {
                break;
            }
            r.prefix_length[3]++;
        }

        if(protocol_mask == 0xFF){
            points[0] = protocal;
            points[1] = protocal;
        }else if(protocol_mask== 0){
            points[0] = 0;
            points[1] = 0xFF;
//            points[1] = 8;
        }else{
            printf("Protocol mask error\n");
            exit(-1);
        }
        r.range[4] = points;
        r.prefix_length[4] = protocol_mask;
        r.id = number_rule;

        rule.push_back(r);
        number_rule++;
    }

    printf("the number of rules = %d\n", number_rule);
    int max_pri = number_rule - 1;
    for(int i=0;i<number_rule;i++){
        rule[i].priority = max_pri - i;
//         printf("%u: %u:%u %u:%u %u:%u %u:%u %u:%u %d\n", i,
//           rule[i].range[0][0], rule[i].range[0][1],
//           rule[i].range[1][0], rule[i].range[1][1],
//           rule[i].range[2][0], rule[i].range[2][1],
//           rule[i].range[3][0], rule[i].range[3][1],
//           rule[i].range[4][0], rule[i].range[4][1],
//           rule[i].priority);
    }
    return rule;
}
static std::vector<Packet> loadpacket(FILE *fp)
{
    unsigned int header[MAXDIMENSIONS];
    unsigned int proto_mask, fid;
    int number_pkt=0; //number of packets
    std::vector<Packet> packets;
    while(true){
        if(fscanf(fp,"%u %u %u %u %u %u %u\n",&header[0], &header[1], &header[2], &header[3], &header[4], &proto_mask, &fid) == Null) break;
        Packet p;
        p.push_back(header[0]);
        p.push_back(header[1]);
        p.push_back(header[2]);
        p.push_back(header[3]);
        p.push_back(header[4]);
        p.push_back(fid);

        packets.push_back(p);
        number_pkt++;
    }

    /*printf("the number of packets = %d\n", number_pkt);
    for(int i=0;i<number_pkt;i++){
        printf("%u: %u %u %u %u %u %u\n", i,
               packets[i][0],
               packets[i][1],
               packets[i][2],
               packets[i][3],
               packets[i][4],
               packets[i][5]);
			   }*/

    return packets;
}


class PacketClassifier {
public:
    virtual void ConstructClassifier(const std::vector<Rule>& rules) = 0;
    virtual int ClassifyAPacket(const Packet& packet) = 0;
    virtual void DeleteRule(const Rule& rule) = 0;
    virtual void InsertRule(const Rule& rule) = 0;
    virtual Memory MemSizeBytes() const = 0;
    virtual int MemoryAccess() const = 0;
    virtual size_t NumTables() const = 0;
    virtual size_t RulesInTable(size_t tableIndex) const = 0;

    virtual std::string funName() {
        return "base class:PacketClassifier";
    }
//    virtual std::string prints() {
//        return "";
//    }

    int TablesQueried() const {	return queryCount; }
//    int NumPacketsQueriedNTables(int n) const { return GetOrElse<int, int>(packetHistogram, n, 0); };

protected:
    void QueryUpdate(int query) {
        packetHistogram[query]++;
        queryCount += query;
    }

private:
    int queryCount = 0;
    std::unordered_map<int, int> packetHistogram;
};

inline void SortRules(std::vector<Rule>& rules) {
    sort(rules.begin(), rules.end(), [](const Rule& rx, const Rule& ry) { return rx.priority >= ry.priority; });
}

inline void SortRules(std::vector<Rule*>& rules) {
    sort(rules.begin(), rules.end(), [](const Rule* rx, const Rule* ry) { return rx->priority >= ry->priority; });
}

#endif //HYBRIDTSSV1_2_ELEMENTARYCLASSES_H
