import sys
from random import randint
from statistics import pvariance
from functools import total_ordering
from heapq import heapify, heapreplace


@total_ordering
class Node:
    def __init__(self, id, R, M):
        self.id = id
        self._reset_R(R)
        self.coll_ceil = M
        self.num_colls = 0
        self.num_trans = 0
        self.count_down_val = 0
        self._rand_back_off()

    def __lt__(self, other):
        return (self.count_down_val, self.id) < (other.count_down_val, other.id)

    def __le__(self, other):
        return (self.count_down_val, self.id) <= (other.count_down_val, other.id)

    def __ge__(self, other):
        return (self.count_down_val, self.id) >= (other.count_down_val, other.id)

    def __gt__(self, other):
        return (self.count_down_val, self.id) > (other.count_down_val, other.id)

    def __eq__(self, other):
        return (self.count_down_val, self.id) == (other.count_down_val, other.id)

    @property
    def r_val(self):
        return self.r_list[self._r_id]

    @property
    def has_timeout(self):
        return self.count_down_val < 1

    def _reset_R(self, R):
        if R is not None:
            self.r_list = R
        self._r_id = 0

    def _permute_R(self):
        self._r_id = 0 if self._r_id + 1 > self.coll_ceil else self._r_id + 1

    def _rand_back_off(self):
        self.count_down_val = randint(0, self.r_val)

    def reset(self, R=None):
        self._reset_R(R)
        self.num_colls = 0
        self.num_trans = 0
        self._rand_back_off()

    def meet_collision(self):
        self.num_colls += 1
        self._permute_R()
        self._rand_back_off()

    def sent_packet(self):
        self.num_trans += 1
        self._rand_back_off()


class CSMA:
    def __init__(self, config):
        self.num_nodes = config['N']
        self.packet_len = config['L']
        self.r_list = config['R']
        assert type(self.r_list) is tuple
        self.coll_ceil = config['M']
        self.tol_time = config['T']
        self.nodes = [Node(i, R=self.r_list, M=self.coll_ceil) for i in range(self.num_nodes)]
        heapify(self.nodes)
        self.sending_time = 0
        self.idle_time = 0
        self.coll_time = 0

    def _reinitialize(self, N=None, L=None, R=None):
        if N is not None and N < self.num_nodes:
            self.nodes = self.nodes[:N]
        if L is not None:
            self.packet_len = L
        if R is not None:
            assert type(R) is tuple
            self.r_list = R

        for node in self.nodes:
            node.reset(R)
        for i in range(self.num_nodes, N):
            self.nodes.append(Node(i, R=self.r_list, M=self.coll_ceil))
        heapify(self.nodes)
        self.num_nodes = N
        self.sending_time = 0
        self.idle_time = 0
        self.coll_time = 0

    """
      0: counting down
      1: candidate
    """

    def simulate(self, N=None, L=None, R=None):
        if N != None or L != None or R != None:
            self._reinitialize(N=N, L=L, R=R)
        time = 0
        occupied_flag = False
        while time < self.tol_time:
            if occupied_flag:
                occupied_flag = False
                self.sending_time += self.packet_len
                self.nodes[0].sent_packet()
                heapreplace(self.nodes, self.nodes[0])

            min_candidate = self.nodes[0]
            if not min_candidate.has_timeout:
                idle = min_candidate.count_down_val
                time += idle
                self.idle_time += idle
                for node in self.nodes:
                    node.count_down_val -= idle
            elif self.nodes[1].has_timeout or self.nodes[2].has_timeout:  # collision
                time += 1
                self.coll_time += 1
                while self.nodes[0].has_timeout:
                    self.nodes[0].meet_collision()
                    heapreplace(self.nodes, self.nodes[0])
            else:
                time += self.packet_len
                occupied_flag = True

        if occupied_flag:  # Simulation terminates before tranmission finished
            self.sending_time += self.packet_len - (time - self.tol_time)

    def get_util(self):
        return self.sending_time / self.tol_time

    def get_idle_fraction(self):
        return self.idle_time / self.tol_time

    def var_trans(self):
        return pvariance(node.num_trans for node in self.nodes)

    def var_coll(self):
        return pvariance(node.num_colls for node in self.nodes)

    def get_total_collisions(self):
        return sum(node.num_colls for node in self.nodes)

    def create_report(self, file_name):
        assert self.coll_time + self.idle_time + self.sending_time == self.tol_time
        print("self.sending_time: %d" % self.sending_time)
        print("self.idle_time: %d" % self.idle_time)
        total_collisions = self.get_total_collisions()
        print("Total number of collisions: %d" % total_collisions)
        chan_util = self.get_util()
        print("chan_util: {:.2%}".format(chan_util))
        idle_frac = self.get_idle_fraction()
        print("idle_frac: {:.2%}".format(idle_frac))
        with open(file_name, "w+") as file:
            file.write(
                "Channel utilization (in percentage): {:.2%}\n".format(chan_util))
            file.write(
                "Channel idle fraction (in percentage): {:.2%}\n".format(idle_frac))
            file.write("Total number of collisions: %d\n" % total_collisions)
            file.write(
                "Variance in number of successful transmissions (across all nodes): %f\n" % self.var_trans())
            file.write(
                "Variance in number of collisions (across all nodes): %f\n" % self.var_coll())


def read_config(fname):
    with open(fname) as f:
        return {args[0]: (tuple(map(int, args[1:])) if len(args) > 2 else int(args[1]))
                for args in (line.strip().split() for line in f)}


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print('./csma input.txt')
        exit()
    else:
        config = read_config(sys.argv[1])
    test = CSMA(config)
    test.simulate()
    test.create_report("output.txt")
