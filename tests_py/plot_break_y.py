import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# Read data
files = [
    "dataH1H2_test/pisces-s0.csv",
    "dataH1H2_test/psi-s0.csv",
    "dataH1H2_test/htm-sgl-s0.csv",
    "dataH1H2_test/psi-strong-s0.csv",
    "dataH1H2_test/si-htm-s0.csv",
    "dataH1H2_test/spht-s0.csv"
]
solutions = [
    "Pisces",
    "PSI",
    "HTM",
    "PSI-strong",
    "SI-HTM",
    "SPHT"
]

read_all = [pd.read_csv(f, sep="\t") for f in files]
Xs = []
Ys = []
for df in read_all:
    d = df
    d = d[d["-u"] == 50]
    d = d[d["-i"] == 200000]
    #breakpoint()
    x = d["-n"]
    y = d["-d"]/d["time"]
    t = list(zip(x,y))
    t.sort(key=lambda e : e[0])
    x = []
    y = []
    for x_t,y_t in t:
        x += [x_t]
        y += [y_t]
    Xs += [x]
    Ys += [y]

# Create the first subplot
fig, axs = plt.subplots(nrows=2, ncols=1, sharex=True, gridspec_kw={'height_ratios': [1, 3]})
axs[0].spines.bottom.set_visible(False)
axs[0].xaxis.tick_top()
#axs[0].spines.top.set_visible(False)
axs[1].spines.top.set_visible(False)
#axs[0].tick_params(labeltop=False)

kwargs = dict(marker=[(-1, -0.5), (1, 0.5)], markersize=12,
              linestyle="none", color='k', mec='k', mew=1, clip_on=False)

# 200000 items
max_y = 8.0e6
break_y = 2.0e6
break_y_offset = 0.1e6

# 50000 items
#max_y = 2.0e7
#break_y = 4.5e6
#break_y_offset = 0.1e6

# 1M items
#max_y = 5.0e5
#break_y = 2.0e5
#break_y_offset = 0.1e5

axs[0].set_ylim([break_y+break_y_offset,max_y]) # top
axs[1].set_ylim([0,break_y]) # below

axs[0].plot([0,64], [break_y,break_y], **kwargs)
axs[1].plot([0,64], [break_y,break_y], **kwargs)

for x,y,s in zip(Xs,Ys,solutions):
    axs[1].plot(x, y, label=s)
    axs[0].plot(x, y, label=s)
axs[1].set_ylabel('Throughput (TXs/s)')
axs[1].set_xlabel('Threads')
#axs[1].set_xticklabels([])

# Adjust layout to prevent clipping of the second y-axis label
plt.tight_layout()
plt.subplots_adjust(hspace=0.05, wspace=0.05)
axs[0].margins(x=0)
axs[1].margins(x=0)

# Show the plot
axs[1].legend()
plt.savefig(f"hashmap_50_upts_200k_items.pdf")
#plt.show()
