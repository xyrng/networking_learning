from csma import CSMA
from matplotlib import pyplot as plt

if __name__ == '__main__':
    config = {'N': 5,
              'L': 20,
              'R': (8, 16, 32, 64, 128, 256, 512),
              'M': 6,
              'T': 50000}
    test = CSMA(config)
    data_range = tuple(range(5, 501))
    util_data = []
    idle_data = []
    coll_data = []
    for n in data_range:
        if n % 20 == 0:
            print('.', end='', flush=True)
        # config['N'] = n
        # test = CSMA(config)
        test.simulate(N=n)
        util_data.append(test.get_util())
        idle_data.append(test.get_idle_fraction())
        coll_data.append(test.get_total_collisions())
    print()
    
    fig1 = plt.figure()
    ax1 = fig1.add_subplot()
    ax1.set(ylim=(-.05, 1.05))
    ax1.set_yticklabels(['{:,.2%}'.format(x) for x in ax1.get_yticks()])
    ax1.plot(data_range, util_data)
    ax1.set_ylabel('Channel Utilization %')
    ax1.set_xlabel('Number of Nodes (N)')
    fig1.show()

    fig2 = plt.figure()
    ax2 = fig2.add_subplot()
    ax2.set(ylim=(-.05, 1.05))
    ax2.set_yticklabels(['{:,.2%}'.format(x) for x in ax2.get_yticks()])
    ax2.plot(data_range, idle_data)
    ax2.set_ylabel('Channel Idle Fraction %')
    ax2.set_xlabel('Number of Nodes (N)')
    fig2.show()

    fig3 = plt.figure()
    ax3 = fig3.add_subplot()
    ax3.plot(data_range, coll_data)
    fig3.show()
    plt.show()