import functools

from csma import CSMA
from matplotlib import pyplot as plt
from multiprocessing import Pool

R_config = tuple(tuple(1 << k for k in range(i, i + 7)) for i in [0, 1, 2, 4])
L_config = (40, 60, 80, 100)
N_config = tuple(range(5, 501))
default_config = {'N': 0,
                  'L': 20,
                  'R': (8, 16, 32, 64, 128, 256, 512),
                  'M': 6,
                  'T': 50000}


def get_new_dict(R=None, L=None):
    r = default_config.copy()
    if R is not None:
        r['R'] = R
    if L is not None:
        r['L'] = L
    return r


def plot(data_set, names, ylabel, title, percentage=True):
    fig = plt.figure()
    ax = fig.add_subplot()
    if percentage:
        ax.set(ylim=(-.05, 1.05))
        ax.set_yticklabels(['{:,.0%}'.format(x) for x in ax.get_yticks()])
    ax.set_xlabel('Number of Nodes (N)')
    ax.set_ylabel(ylabel)
    ax.set_title(title)

    if type(data_set[0]) is not list:
        ax.plot(N_config, data_set, label=names)
    else:
        for name, data in zip(names, data_set):
            ax.plot(N_config, data, label=name)
    ax.legend()
    fig.show()

def f(model, n):
    return model.simulate(N=n)

if __name__ == '__main__':
    util_data = []
    idle_data = []
    coll_data = []

    var_r = 0
    var_l = 1
    other_util_dataset = [[[] for _ in range(4)], [[] for _ in range(4)]]
    other_models = [[CSMA(get_new_dict(R=R), name='init R={}'.format(R)) for R in R_config],
                    [CSMA(get_new_dict(L=L), name='L={}'.format(L)) for L in L_config]]
    default_model = CSMA(default_config)

    flattened_models = [model for models in other_models for model in models]
    flattened_models.append(default_model)
    p = Pool(9)
    for n in N_config:
        if n % 20 == 0:
            print('.', end='', flush=True)
        fn = functools.partial(f, n=n)
        flattened_models = p.map(fn(n), flattened_models)
        util_data.append(flattened_models[-1].get_util())
        idle_data.append(flattened_models[-1].get_idle_fraction())
        coll_data.append(flattened_models[-1].get_total_collisions())

        for i in range(len(other_models)):
            for j in range(len(other_models[0])):
                other_util_dataset[i][j].append((flattened_models[i * 4 + j].get_util()))
    print()

    plot(util_data, default_model.name, 'Channel Utilization %', 'Part (a)')
    plot(idle_data, default_model.name, 'Channel Idle Fraction %', 'Part (b)')
    plot(coll_data, default_model.name, 'Total Collisions (times)', 'Part (c)', False)

    model_names = [[model.name for model in models] for models in other_models]
    model_names[var_r].insert(3, default_model.name)
    other_util_dataset[var_r].insert(3, util_data)
    plot(other_util_dataset[var_r], model_names[var_r], 'Channel Utilization %', 'Part (d)')

    model_names[var_l].insert(0, default_model.name)
    other_util_dataset[var_l].insert(0, util_data)
    plot(other_util_dataset[var_l], model_names[var_l], 'Channel Utilization %', 'Part (e)')

    plt.show()
