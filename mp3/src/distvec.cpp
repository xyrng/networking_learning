#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <cstdint>
#include <climits>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <fstream>
#include <map> 
#include <set>
#include <utility>

using namespace std;

class Distance_Vector {
    public:
    class Node {
        public:
        int id;
        vector<int> neibours;
        unordered_map<int, int> edges;
        map<int, int> forwarding_table;
        unordered_map<int, int> distanceToThatNode;
        vector<pair<int, unordered_map<int,int> > > updates_from_nei;
        bool be_updated;

        Node(int id) {
            this->id = id;
            forwarding_table[this->id] = this->id;
            this->distanceToThatNode[this->id] = 0;
            be_updated = true;
        }

        ~Node() {
        }
        
        Node(const Node &n1) {
            this->id = n1.id;
            this->edges = n1.edges;
            this->forwarding_table = n1.forwarding_table;
            this->neibours = n1.neibours;
            this->updates_from_nei = n1.updates_from_nei;
        }

        // Node(const Node &n1) {id = n1.id; } 

        void remove_nei(int nei_id) {
            // remove from edges, and neibours, if exists
            if (this->edges.find(nei_id) != this->edges.end()) { // found
                vector<int>::iterator pos = find(this->neibours.begin(), this->neibours.end(), nei_id);
                if (pos != this->neibours.end()) {
                    this->neibours.erase(pos);
                }
                this->edges.erase(nei_id);
            }
        }

        void add_or_updateNei(int nei_id, int edgeCost) {
            be_updated = true;
            auto it = edges.find(nei_id);
            if (it != this->edges.end()) {  // found
                it->second = edgeCost; // TODO: Look up update
            } else { // new comer
                this->neibours.push_back(nei_id);
                this->edges[nei_id] = edgeCost;
            }
        }

        int forwarding_hop(int dest_id) {
            return forwarding_table.find(dest_id)->second; //unreachable --> -1
        }

        int get_edge_cost(int nei_id) {
            return this->edges.find(nei_id)->second;
        }

        void initial_distance_of_node(int node_id) {
        
            be_updated = true;
            this->distanceToThatNode[node_id] = INT_MAX;
            
        }
        
        bool send_update(map<int, Node*>& graph) {
            if (!be_updated) {
                return false;
            }
            int i;
            for (i = 0; i < neibours.size(); i++) {
                graph[neibours[i]]->updates_from_nei.push_back(make_pair(this->id, distanceToThatNode));
            }
            return i != 0;
        }

        bool update_distance_and_forwarding_table() {
            bool is_changed = false;
            for (int i = 0; i < updates_from_nei.size(); i++) {
                // nei_updates.first: nei_id   itr->first: dest_id   itr->second: given_cost
                pair<int, unordered_map<int,int> > nei_updates = updates_from_nei[i];
                for (auto itr = nei_updates.second.begin(); itr != nei_updates.second.end(); itr++) {
                    int nei_edge = get_edge_cost(nei_updates.first);
                    int given_dist = itr->second;
                    if (nei_edge == INT_MAX || given_dist == INT_MAX) continue; 
                    int new_dist = nei_edge + given_dist;
                    if (this->distanceToThatNode[itr->first] > new_dist) {
                        this->distanceToThatNode[itr->first] = new_dist;
                        this->forwarding_table[itr->first] = nei_updates.first;
                        is_changed = true;
                    } else if (this->distanceToThatNode[itr->first] == new_dist) {
                        if (this->forwarding_table[itr->first] > nei_updates.first) {
                            this->forwarding_table[itr->first] = nei_updates.first;
                            is_changed = true;
                        }
                    }
                }
            }
            // if not update:
            updates_from_nei.clear();
            return (be_updated = is_changed);
        }
    };

    map<int, Node*> graph;

    // including update
    void build_graph(string input, bool incremental) {
        vector<string> splitted = str_split(input);
        
        int node1 = stoi(splitted[0]); // TODO: free
        int node2 = stoi(splitted[1]);
        int dist = stoi(splitted[2]);
        auto it1 = graph.find(node1);
        if (it1 != graph.end()) { // contains
            if (dist > 0) {
                it1->second->add_or_updateNei(node2, dist);
            } else {
                it1->second->remove_nei(node2);
                initial_all_dist();
            }
        } else {
            Node* new_node = new Node(node1);
            graph[node1] = new_node;
            if (dist > 0) {
                graph.find(node1)->second->add_or_updateNei(node2, dist);
            }
            if (incremental) {
                initial_two_node_dist(node1);
            }
        }
        auto it2 = graph.find(node2);
        if (it2 != graph.end()) {
            if (dist > 0) {
                it2->second->add_or_updateNei(node1, dist);
            } else {
                it2->second->remove_nei(node1);
            }
        } else {
            Node* new_node = new Node(node2);
            graph[node2] = new_node;
            if (dist > 0) {
                graph.find(node2)->second->add_or_updateNei(node1, dist);
            }
            if (incremental) {
                initial_two_node_dist(node2);
                cout << graph[4]->distanceToThatNode[5] << endl;
            }
        }
    }

    vector<string> str_split(string input) {
        vector<string> result;
        size_t current, previous = 0;
        current = input.find_first_of(" ");
        while (current != string::npos) {
            result.push_back(input.substr(previous, current - previous));
            previous = current + 1;
            current = input.find_first_of(" ", previous);
        }
        result.push_back(input.substr(previous, current - previous));
        return result;
    }

    // including update
    void build_nodes_forwarding_tables() {
        map<int, Node*>::iterator itr; 
        cout << "build -- graph.size(): " << graph.size() << endl;
        while (1) {
            //update 
            int count = 0;
            // itr->second: node
            for(itr = graph.begin(); itr != graph.end(); itr++) {
                if (itr->second->send_update(graph)) count++;
            }
            if (count == 0) break;
            count = 0;
            for(itr = graph.begin(); itr != graph.end(); itr++) {
                if(itr->second->update_distance_and_forwarding_table()) count++;
            }
            if (count == 0) break;
        }
        
    }

    void print_results(FILE *file_out, char *message_file) {
        // forwarding tables
        for (map<int, Node*>::iterator itr_i = graph.begin(); itr_i != graph.end(); itr_i++) {
            // itr_i->first: from     itr_j->first: to
            Node *from_node = itr_i->second;
            for (map<int, Node*>::iterator itr_j = graph.begin(); itr_j != graph.end(); itr_j++) {
                int cost = from_node->distanceToThatNode[itr_j->first];
                int next = from_node->forwarding_table[itr_j->first];
                if (cost != INT_MAX && cost >= 0) {
                    // dest_id    next_hop    cost
                    fprintf(file_out, "%d %d %d\n", itr_j->first, next, cost);
                }
            }
        }
        if (graph.size() >= 8) {
            print_messages(file_out, message_file);
        }
        
    }

    void print_messages(FILE *file_out, char *message_file) {
        string line;
        ifstream msgfile(message_file);
        while (getline(msgfile, line)) {
            vector<string> splitted = str_split(line);
            int node1 = stoi(splitted[0]); // from
            int node2 = stoi(splitted[1]); // to
            Node *from_node = graph[node1];

            int dist = from_node->distanceToThatNode[node2];
            if (dist != INT_MAX && dist >= 0) {
                fprintf(file_out, "from %d to %d cost %d hops ", node1, node2, dist);
                vector<int> hops = next_hops(node1, node2);
                for (int i = 0 ; i < hops.size(); i++) {
                    fprintf(file_out, "%d ", hops[i]);
                }
                fprintf(file_out, "message");
                for (vector<string>::iterator it = splitted.begin() + 2; it != splitted.end(); it++) {
                    fprintf(file_out, " %s", it->c_str());
                }
                fprintf(file_out, "\n");
            } else {
                fprintf(file_out, "from %d to %d cost infinite hops unreachable message", node1, node2);
                for (vector<string>::iterator it = splitted.begin() + 2; it != splitted.end(); it++) {
                    fprintf(file_out, " %s", it->c_str());
                }
                fprintf(file_out, "\n");
            }
        }
    }

    void initial_all_dist() {
        for (auto itr_i = graph.begin(); itr_i != graph.end(); itr_i++){
            for (auto itr_j = graph.begin(); itr_j != graph.end(); itr_j++) {
                if (itr_i->first != itr_j->first) {
                    itr_i->second->initial_distance_of_node(itr_j->first);
                }
            }
        }
    }

    void initial_two_node_dist(int node1) {
        for (auto itr_i = graph.begin(); itr_i != graph.end(); itr_i++){
            if (itr_i->first != node1) {
                itr_i->second->initial_distance_of_node(node1);
                graph[node1]->initial_distance_of_node(itr_i->first);
            }
        }
        
    }

    // TODO: problems track:
    vector<int> next_hops(int from, int to) {
        vector<int> result;
        Node *curr = graph[from];
        do{
            result.push_back(curr->id);
            curr = graph[curr->forwarding_table[to]];
         } while (curr->id != to);
        return result;
    }
};

int main(int argc, char** argv) {
    printf("Number of arguments: %d\n", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }
    // MUTE argv warning
    (void) argv;

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");

    Distance_Vector dv;
    // build graph
    string line;
    ifstream topofile(argv[1]);
    while(getline(topofile, line)) {
        // cout << line << endl;
        dv.build_graph(line, false);
    }
    // build forwarding table
    dv.initial_all_dist();
    dv.build_nodes_forwarding_tables();
    // print tje 1st forwarding table and message 
    dv.print_results(fpOut, argv[2]);
    // print based on change
    ifstream changefile(argv[3]);
    string c_line;
    while (getline(changefile, c_line)) {
        // TODO: update?
        dv.build_graph(c_line, true);
        dv.build_nodes_forwarding_tables();
        dv.print_results(fpOut, argv[2]);
    }


    fclose(fpOut);

    return 0;
}

