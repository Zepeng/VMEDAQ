import numpy as np
import matplotlib.pyplot as plt
import pandas as pd

f = open('dumb', 'r')
linenum = 0
for line in f:
    wfs = []
    event = line.split()[5:]
    wf = []
    print(len(event)/32)
    for j in range(len(event)):
        if j > 0 and j % 1026 ==0:
            wfs.append(wf[2:])
            wf = []
        else:
            wf.append(int(event[j]))
    wfs.append(wf[2:])
    fig, ax = plt.subplots(figsize=(8,16))
    for i in range(len(wfs)):
        ax.plot(np.arange(len(wfs[i]))/0.75, wfs[i] - np.mean(wfs[i]) + i*50, label='ch' + str(i))
    ax.legend()
    ax.set_xlabel('Time (ns)')
    ax.set_ylabel('ADC count')
    fig.savefig('event_%d.png' % linenum)
    linenum += 1
