import sys
import matplotlib.pyplot as plt
import numpy as np
from random import randint
from matplotlib.backends.backend_pdf import PdfPages

class Node:
    def __init__(self, R, M):
        self.r_list = R
        self.coll_ceil = M
        self.coll_ct = 0
        self.r_val = self.r_list[0]
        self.num_colls = 0
        self.num_trans = 0
        self.count_down = 0
#         self.sending_packet = 0
        self.back_off()

    def reset(self, R=None, M=None):
        if R is not None:
            self.r_list = R
        self.r_val = self.r_list[0]
        if M is not None:
            self.coll_ceil = M
        self.coll_ct = 0
        self.num_colls = 0
        self.num_trans = 0
        self.count_down = 0
        self.back_off()

    def update_R_val(self):
        self.r_val *= 2

    def reset_R_val(self):
        self.r_val = self.r_list[0]

    def back_off(self):
        self.count_down = randint(0, self.r_val)

    def meet_collision(self):
        self.num_colls += 1
        self.coll_ct += 1
        if self.coll_ct < self.coll_ceil:
            self.update_R_val()
        else:
            self.reset_R_val()
        self.back_off()

    def send_packet(self):
        self.num_trans += 1
        self.back_off()

    """
      0: counting down
      1: candidate
    """
    def my_turn(self):
        if self.count_down > 0:
            self.count_down -= 1
            return 0
        elif self.count_down == 0:
            return 1

#         if not busy:
#             if self.count_down > 0:
#                 self.count_down -= 1
#                 return 1
#             elif self.sending_packet > 0:
#                 return send_packet()
#             elif self.count_down == 0:
#                 return 2
#         else:
#             if self.count_down == 0:
#                 self.meet_collision()
#                 return 5
#             return 0







class CSMA:
    def __init__(self,N,L,R,M,T):
        self.num_nodes = self.__str_to_int(N)
        self.packet_len = self.__str_to_int(L)
        self.r_list = self.__str_to_list(R)
        self.coll_ceil = self.__str_to_int(M)
        self.tol_time = self.__str_to_int(T)
        self.nodes = self.__create_nodes(self.num_nodes, self.r_list, self.coll_ceil)
        self.sending_time = 0
        self.idle_time = 0
        self.coll_time = 0
#         self.busy = false

    def __str_to_int(self,num):
        return int(num)

    def __str_to_list(self, string):
        return [int(x) for x in string[1:-1].split(",")]

    def __create_nodes(self, num, r_list, coll_ceil):
        lst = []
        for i in range(num):
            lst.append(Node(r_list, coll_ceil))
        return lst

    def __reinitialize(self,N=None,L=None,R=None,M=None,T=None):
        add = 0
        ori_N = self.num_nodes
        if N is not None:
            N = self.__str_to_int(N)
            if N < self.num_nodes:
                self.nodes = self.nodes[:N]
            elif N > self.num_nodes:
                add = self.num_nodes - N
            self.num_nodes = N
        if L is not None:
            self.packet_len = self.__str_to_int(L)
        if R is not None:
            self.r_list = self.__str_to_list(R)
        if M is not None:
            self.coll_ceil = self.__str_to_int(M)
        if T is not None:
            self.tol_time = self.__str_to_int(T)

        for node in self.nodes:
            node.reset(self.r_list, self.coll_ceil)
        for i in range(add):
            self.num_nodes.append(Node(self.r_list, self.coll_ceil))
        self.sending_time = 0
        self.idle_time = 0
        self.coll_time = 0

    """
      0: counting down
      1: candidate
    """
    def simulate(self,N=None,L=None,R=None,M=None,T=None):
        if N != None or L != None or R != None or M != None or T != None:
            self.__reinitialize(N,L,R,M,T)
        time = 0
        while True:
            send_candidates = []
            if time >= self.tol_time:
                break
            for node in self.nodes:
                curr = node.my_turn()
                if curr == 1:
                    send_candidates.append(node)

            num_cand = len(send_candidates)
            if num_cand == 0:
                self.idle_time += 1
                time += 1
            elif num_cand == 1:
                self.sending_time += self.packet_len
                time += self.packet_len
                send_candidates[0].send_packet()
            else:
                time += self.packet_len
                work_node = randint(0, num_cand)
                self.sending_time += self.packet_len
                for i in range(num_cand):
                    if i == work_node:
                        send_candidates[i].send_packet()
                    else:
                        send_candidates[i].meet_collision()


    def __var_trans(self):
        lst = []
        for node in self.nodes:
            lst.append(node.num_trans)
        return np.var(lst)

    def __var_coll(self):
        lst = []
        for node in self.nodes:
            lst.append(node.num_colls)
        return np.var(lst)

    def get_total_collisions(self):
        val = 0
        for node in self.nodes:
            val += node.num_colls
        return val

    def create_report(self, file_name):
        print("self.sending_time: %d" % self.sending_time)
        print("self.idle_time: %d" % self.idle_time)
        total_collisions = self.get_total_collisions()
        print("Total number of collisions: %d" % total_collisions)
        chan_util = round(self.sending_time/self.tol_time, 4)
        print("chan_util: %f" % chan_util)
        idle_frac = round(self.idle_time/self.tol_time, 4)
        print("idle_frac: %f" % idle_frac)
        with open(file_name, "w+") as file:
            file.write("Channel utilization (in percentage): %f\n" % chan_util)
            file.write("Channel idle fraction (in percentage): %f\n" % idle_frac)
            file.write("Total number of collisions: %d\n" % total_collisions)
            file.write("Variance in number of successful transmissions (across all nodes): %f\n" % self.__var_trans())
            file.write("Variance in number of collisions (across all nodes): %f\n" % self.__var_coll())






if __name__ == "__main__":
    N = sys.argv[1]
    L = sys.argv[2]
    R = sys.argv[3]
    M = sys.argv[4]
    T = sys.argv[5]
    test = CSMA(N,L,R,M,T)
    test.simulate()
    test.create_report("output.txt")
