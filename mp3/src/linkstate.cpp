#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

using namespace std;

class Link_State {
    public:
    class Node {
        public:
        int id;
        vector<int> neibours;
        unordered_map<int, int> edges;
        map<int, int> forwarding_table;

        Node(int id) {
            this->id = id;
            forwarding_table[this->id] = 0;
        }

        ~Node() {
        }
        
        Node(const Node &n1) {
            this->id = n1.id;
            this->edges = n1.edges;
            this->forwarding_table = n1.forwarding_table;
            this->neibours = n1.neibours;
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
            // cout << "add_or_updateNei: " << __LINE__ << endl;
            // cout << "this->edges: " << &this->edges << endl;
            // cout << "nei_id: " << nei_id << endl;
            auto it = edges.find(nei_id);
            // cout << "error: " << __LINE__ << endl;
            if (it != this->edges.end()) {  // found
                // cout << "add_or_updateNei update, this id: " << this->id << "; nei_id: " << nei_id << endl;
                it->second = edgeCost; // TODO: Look up update
            } else { // new comer
                // cout << "add_or_updateNei new comer, this id: " << this->id << "; nei_id: " << nei_id << endl;
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

        // TODO: consider update
        void build_forwarding_table(map<int, Node*> graph, unordered_map<int, unordered_map<int, int>* >& prevOfThatNode, 
                                    unordered_map<int, unordered_map<int, int>* >& totalDistance) {
            unordered_map<int, int> *distanceToRoot = new unordered_map<int, int>();
            unordered_map<int, int> *recordDist = new unordered_map<int, int>();
            for (map<int, Node*>::iterator itr = graph.begin(); itr != graph.end(); itr++) {
                (*distanceToRoot)[itr->first] = INT_MAX;
            }
            unordered_map<int, int> *prevs = new unordered_map<int, int>();
            set<int> visited;
            (*distanceToRoot)[this->id] = 0;
            (*prevs)[this->id] = -1;
            while (visited.size() < graph.size()) {
                int cur_id = get_idx_of_min_cost(distanceToRoot);
                visited.insert(cur_id);
                int dist = (*distanceToRoot).find(cur_id)->second;
                (*recordDist)[cur_id] = dist;
                if (dist == INT_MAX) {
                    this->forwarding_table[cur_id] = -1;
                    continue;
                }
                Node *cur = graph.find(cur_id)->second;
                (*distanceToRoot).erase(cur_id);
                for (int i = 0; i < cur->neibours.size(); i++) {
                    int nei = cur->neibours[i];
                    // cout << "nei_id: " << nei << endl;
                    // cout << (it != visited.end()) << endl;
					if (visited.find(nei) != visited.end()) {
                        continue;
                    }
					if (dist + cur->get_edge_cost(nei) < (*distanceToRoot).find(nei)->second){
                        auto prev_itr = (*prevs).find(nei);
                        if (prev_itr != (*prevs).end()) {
                            prev_itr->second = cur_id;
                        } else {
                            (*prevs)[nei] = cur_id;
                        }
						auto dis_itr = (*distanceToRoot).find(nei);
						dis_itr->second = dist + cur->get_edge_cost(nei);
					}
				}
            }

            if (totalDistance.find(this->id) != totalDistance.end()) {
                unordered_map<int, int> *old_dist = totalDistance[this->id];
                unordered_map<int, int> *old_prev = prevOfThatNode[this->id];
                for (unordered_map<int,int>::iterator itr = recordDist->begin(); itr != recordDist->end(); itr++) {
                    int to_node = itr->first;
                    cout << "from: " << this->id << " to: " << to_node << " (*prevs)[to_node]: " << (*prevs)[to_node] << endl;
                    cout << "from: " << this->id << " to: " << to_node << " (*old_prev)[to_node]: " << (*old_prev)[to_node] << endl;
                    if ((*old_dist).find(to_node) != (*old_dist).end() 
                        && (*old_dist)[to_node] == itr->second
                        && (*old_prev)[to_node] < (*prevs)[to_node]) {
                        (*prevs)[to_node] = (*old_prev)[to_node];
                    }
                }
                // totalDistance.erase(this->id);
                // prevOfThatNode.erase(this->id);
            }


            totalDistance[this->id] = recordDist;
            prevOfThatNode[this->id] = prevs; 
            int i = 0;
            for(map<int, Node*>::iterator itr = graph.begin(); itr != graph.end(); itr++) {
				auto prev_itr = (*prevs).find(itr->first);
                if (itr->first == this->id || prev_itr == (*prevs).end()) continue;
				int next = itr->first;
				for (int p = (*prevs).find(next)->second; p != this->id; next = p, p = (*prevs).find(p)->second);
				this->forwarding_table[itr->first] = next;
                i++;
			}
        }

        int get_idx_of_min_cost(unordered_map<int, int>* map) {
            int min = INT_MAX;
			int ret = -1;
			for(unordered_map<int,int>::iterator itr = (*map).begin(); itr != (*map).end(); itr++) {
				if (itr->second < min) {
					min = itr->second;
					ret = itr->first;
				} else if (itr->second == min) {
                    if (ret == -1 || itr->first < ret) {
                        ret = itr->first;
                    }
				}
			}

			return ret;
        }
    };

    map<int, Node*> graph;
    unordered_map<int, unordered_map<int, int>* > prevOfThatNode;
    unordered_map<int, unordered_map<int, int>* > totalDistance;

    // including update
    void build_graph(string input) {
        // cout << "Line: " << __LINE__ << endl;
        vector<string> splitted = str_split(input);
        
        int node1 = stoi(splitted[0]); // TODO: free
        int node2 = stoi(splitted[1]);
        int dist = stoi(splitted[2]);
        // cout << "node1: " << node1 << endl; 
        // cout << "node2: " << node2 << endl;
        // cout << "dist: " << dist << endl;
        auto it1 = graph.find(node1);
        if (it1 != graph.end()) { // contains
            if (dist > 0) {
                // cout << "Contains Node start: " << __LINE__ << endl;
                it1->second->add_or_updateNei(node2, dist);
                // cout << "Contains Node end : " << __LINE__ << endl;
            } else {
                it1->second->remove_nei(node2);
            }
        } else {
            Node* new_node = new Node(node1);
            graph[node1] = new_node;
            if (dist > 0) {
                graph.find(node1)->second->add_or_updateNei(node2, dist);
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
        }
        // cout << "Line: " << __LINE__ << endl;
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
        for(itr = graph.begin(); itr != graph.end(); itr++) {
            itr->second->build_forwarding_table(graph, prevOfThatNode, totalDistance);
        }
        // cout << __LINE__ << endl;
    }

    void print_results(FILE *file_out, char *message_file) {
        // forwarding tables
        // cout << __LINE__ << endl;
        for (map<int, Node*>::iterator itr_i = graph.begin(); itr_i != graph.end(); itr_i++) {
            for (map<int, Node*>::iterator itr_j = graph.begin(); itr_j != graph.end(); itr_j++) {
                int next = itr_i->second->forwarding_hop(itr_j->first);
                if (next != -1) {
                    fprintf(file_out, "%d %d %d\n", itr_i->first, itr_j->first, next);
                    // cout << itr_i->first << " " <<  itr_j->first << " " <<  next << endl;
                } 
            }
        }
        // cout << __LINE__ << endl;

        print_messages(file_out, message_file);
    }

    void print_messages(FILE *file_out, char *message_file) {
        // cout << __LINE__ << endl;
        string line;
        ifstream msgfile(message_file);
        while (getline(msgfile, line)) {
            // cout << "line: " << line << endl;
            vector<string> splitted = str_split(line);
            // cout << __LINE__ << endl;
            int node1 = stoi(splitted[0]); // from
            // cout << "node1: " << node1 << endl;
            int node2 = stoi(splitted[1]); // to
            // cout << "node2: " << node2 << endl;
            // cout << "totalDistance[node1]: " << &totalDistance[node1] << endl;
            unordered_map<int, int>* distance = totalDistance[node1];
            // cout << "distance: " << &distance << endl;
            int dist = (*distance)[node2];
            // cout << "dist: " << dist << endl;
            if (dist != INT_MAX) {
                // cout << "dist != INT_MAX" << endl;
                fprintf(file_out, "from %d to %d cost %d hops ", node1, node2, dist);
                vector<int> hops = next_hops(node1, node2);
                // cout << __LINE__ << endl;
                for (vector<int>::iterator itr = hops.end() - 2; itr >= hops.begin(); itr--) {
                    fprintf(file_out, "%d ", *itr);
                }
                // cout << __LINE__ << endl;
                fprintf(file_out, "message");
                for (vector<string>::iterator it = splitted.begin() + 2; it != splitted.end(); it++) {
                    fprintf(file_out, " %s", it->c_str());
                }
                fprintf(file_out, "\n");
                // cout << __LINE__ << endl;
            } else {
                // cout << "dist == INT_MAX" << endl;
                fprintf(file_out, "from %d to %d cost infinite hops unreachable message %s\n", node1, node2, splitted[2].c_str());
            }
        }
        // cout << __LINE__ << endl;
    }

    // TODO: problems track:
    vector<int> next_hops(int from, int to) {
        vector<int> result;
        unordered_map<int, int> prevs = *(prevOfThatNode.find(from)->second);
        int curr = to;
        while (curr != from) {
            int prev = prevs.find(curr)->second;
            result.push_back(prev);
            curr = prev;
        }
        result.push_back(from);
        return result;
    }
};

int main(int argc, char** argv) {
    printf("Number of arguments: %d\n", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    

    Link_State ls;
    // build graph
    string line;
    ifstream topofile(argv[1]);
    while(getline(topofile, line)) {
        // cout << line << endl;
        ls.build_graph(line);
    }
    // cout << __LINE__ << endl;
    // build forwarding table
    ls.build_nodes_forwarding_tables();

    // print tje 1st forwarding table and message 
    ls.print_results(fpOut, argv[2]);

    // print based on change
    ifstream changefile(argv[3]);
    string c_line;
    while (getline(changefile, c_line)) {
        // TODO: update?
        ls.build_graph(c_line);
        ls.build_nodes_forwarding_tables();
        ls.print_results(fpOut, argv[2]);
    }
    
    fclose(fpOut);
    return 0;
}
