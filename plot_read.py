import numpy as np
import matplotlib.pyplot as plt
import pandas as pd

wfs = []
f = open('dumb', 'r')
for line in f:
    event = line.split()[5:]
    wf = []
    print(len(event)/16)
    for j in range(len(event)):
        if j > 0 and j % 1026 ==0:
            wfs.append(wf[2:])
            wf = []
        else:
            wf.append(int(event[j]))
fig, ax = plt.subplots()
for i in range(len(wfs)):
    ax.plot(np.arange(len(wfs[i])), wfs[i] - np.mean(wfs[i]))
fig.savefig('test.png')
