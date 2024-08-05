import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

markers = [
  ",",
  "o",
  "s",
  "*",
  "+",
  "x",
  "d",
  "v",
  "^",
  "h",
  "."

]

class BackendDataset:
  def __init__(self, name, samples, x_fn, x_label, y_fn, y_label, selector):
    self.name = name
    read_all = [pd.read_csv(s, sep="\t") for s in samples]
    self.samples = []
    for df in read_all:
      d = df
      for k,v in selector.items():
        d = d[d[k] == v]
      self.samples += [d]
    self.x_param = np.array([[x_fn(s)] for s in self.samples])
    self.y_param = np.array([[y_fn(s)] for s in self.samples])
    self.x_label = x_label
    self.y_label = y_label
    self.y_stack = {}

  def add_stack(self, title, y_label, y_fns:dict[str,callable]):
    y_stack = {}
    for lbl,fn in y_fns.items():
      y_stack[lbl] = np.array([[fn(s)] for s in self.samples])
    self.y_stack[(title, y_label)] = y_stack

class LinesPlot:
  def __init__(self, title, filename, figsize=(5, 4)):
    print(f"LinesPlot title {title}")
    self.title = title
    self.filename = filename
    self.figsize = figsize

  def plot(self, datasets:list[BackendDataset]):
    fig, ax = plt.subplots(figsize=self.figsize, nrows=1, ncols=1)
    for i,d in enumerate(datasets):
      triple = [(np.average(x),np.average(y),np.std(y)) for x,y in zip(d.x_param.transpose(), d.y_param.transpose())]
      triple.sort(key=lambda elem : elem[0]) # sort by X
      x_array, y_array, y_error = zip(*triple)
      ax.errorbar(x_array, y_array, yerr = y_error, label=d.name, marker=markers[i])
      ax.set_xlabel(d.x_label)
      ax.set_ylabel(d.y_label)
      ax.set_title(self.title)
      ax.legend()
    plt.tight_layout()
    plt.savefig(self.filename)
    fig.clear()
    plt.close()

  def plot_stack(self, datasets:list[BackendDataset]):
    nb_stacks = 0
    fix_dataset = []
    for d in datasets:
      l = len(d.y_stack)
      if l > nb_stacks:
        nb_stacks = l
      if l > 0:
        fix_dataset += [d]

    f = self.figsize
    fig, axs = plt.subplots(figsize=(f[0]*nb_stacks, f[1]), nrows=1, ncols=nb_stacks)
    datasets_idx = {}
    plots_idx = {}
    stacked_bar_idx = {}
    i = 0
    for d in fix_dataset:
      datasets_idx[d.name] = i
      i += 1
      j = 0
      for s_title, ss in d.y_stack.items():
        plots_idx[s_title[0]] = j
        j += 1
        k = 0
        for sn, sy in ss.items():
          cmap = plt.cm.get_cmap("hsv", len(ss)+1)
          stacked_bar_idx[sn] = {"idx": k, "color": cmap(k)}
          k += 1

    width = 0.9 / len(fix_dataset)
    for d in fix_dataset:
      for s_title, ss in d.y_stack.items():
        bottom = np.array([0 for _ in d.x_param.transpose()])
        for sn, sy in ss.items():
          i = datasets_idx[d.name]
          j = plots_idx[s_title[0]]
          triple = [(np.average(x),np.average(y),np.std(y)) for x,y in zip(d.x_param.transpose(), sy.transpose())]
          triple.sort(key=lambda elem : elem[0]) # sort by X
          x_array, y_array, y_error = zip(*triple)
          # breakpoint()
          xs = np.array([k for k in range(len(x_array))]) + i*width
          ys = np.array(y_array)
          # print("X:", xs)
          # print("Y:", ys)
          # print("error:", y_error)
          # breakpoint() # TODO: there is some division by 0
          if i == 0: # print label
            axs[j].bar(xs, ys, width, yerr = y_error, label=sn, bottom=bottom, color=stacked_bar_idx[sn]["color"])
            axs[j].legend()
          else:
            axs[j].bar(xs, ys, width, yerr = y_error, bottom=bottom, color=stacked_bar_idx[sn]["color"])
          bottom = bottom + ys
        for x,y in zip(xs,bottom):
          axs[j].annotate(d.name, (x, y), textcoords="offset points", xytext=(0,10), ha='center', rotation=90)
          break
        if i == 0:
          axs[j].set_title(f"{self.title}\n{s_title[0]}")
          axs[j].set_xticks(xs)
          axs[j].set_xticklabels([int(x) for x in x_array])
          axs[j].set_xlabel(d.x_label)
          axs[j].set_ylabel(s_title[1])
        j += 1
      i += 1
    plt.tight_layout()
    plt.savefig(f"stack_{self.filename}")
    fig.clear()
    plt.close()
