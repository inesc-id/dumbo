import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from typing import Callable, Optional, Tuple
import json

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
    self.filter_x_fn = None
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

  def add_stack(self, title, y_label, y_fns:dict[str,Callable], filter_x_fn:Optional[Callable[[Tuple[float,float,float]],bool]] = None, is_percent=False, fix_100=False, label_size=0, filter_out_backends=[]):
    """
    Adds a stack to the stack plot.
    You can add an optional function fn((x,y,std_dev))->bool to discard points (when returning False).
    """
    self.filter_x_fn = filter_x_fn
    y_stack = {}
    for lbl,fn in y_fns.items():
      y_stack[lbl] = np.array([[fn(s)] for s in self.samples])
    self.y_stack[(title, y_label, is_percent, label_size, fix_100)] = y_stack

class LinesPlot:
  def __init__(self, title, filename, figsize=(5, 4), colors={}):
    print(f"LinesPlot title {title}")
    self.title = title
    self.filename = filename
    self.figsize = figsize
    self.colors = colors

  def plot(self, datasets:list[BackendDataset], filter_out_backends=[]):
    fig, ax = plt.subplots(figsize=self.figsize, nrows=1, ncols=1)
    map_c = {}
    cmap = plt.cm.get_cmap("Paired", len(datasets)+5)
    max_y = 0
    for i,d in enumerate(datasets):
      map_c[d.name] = cmap(i)
    for i,d in enumerate(datasets):
      if d.name in filter_out_backends:
        continue
      z = zip(d.x_param.transpose(), d.y_param.transpose())
      triple = [(np.average(x),np.average(y),np.std(y)) for x,y in z]
      triple.sort(key=lambda elem : elem[0]) # sort by X
      max_y_aux = max([t[1] for t in triple])
      max_y = max(max_y, max_y_aux)
      x_array, y_array, y_error = zip(*triple)
      color = self.colors[d.name] if d.name in self.colors else map_c[d.name]
      ax.errorbar(x_array, y_array, yerr = y_error, label=d.name, marker=markers[i], color=color)
      ax.set_xlabel(d.x_label)
      ax.set_ylabel(d.y_label)
      ax.set_title(self.title)
      ax.legend()
    ax.set_ylim(bottom=0, top=max_y+0.04*max_y)
    plt.tight_layout()
    plt.savefig(self.filename)
    fig.clear()
    plt.close()

  def plot_stack(self, datasets:list[BackendDataset], filter_out_backends=[]):
    nb_stacks = 0
    fix_dataset = []
    for d in datasets:
      if d.name in filter_out_backends:
        continue
      l = len(d.y_stack)
      if l > nb_stacks:
        nb_stacks = l
      if l > 0:
        fix_dataset += [d]

    width = 0.83 / len(fix_dataset)
    offset = 0.005
    datasets_idx = {}
    plots_idx = {}
    stacked_bar_idx = {}
    k = 0
    cmap = plt.cm.get_cmap("Paired", 20)
    for idx,d in enumerate(fix_dataset):
      # breakpoint()
      if d.name in filter_out_backends:
        continue
      datasets_idx[d.name] = idx
      j = 0
      for s_title, ss in d.y_stack.items():
        plots_idx[s_title[0]] = j
        j += 1
        for sn, sy in ss.items():
          if sn not in stacked_bar_idx:
            k += 1
            c = cmap(k)
            stacked_bar_idx[sn] = {"idx": k, "color": cmap(k)}
    for s1 in stacked_bar_idx: ### colors work around
      for s2 in stacked_bar_idx:
        if s1 == s2:
          continue
        c1 = stacked_bar_idx[s1]["color"]
        c2 = stacked_bar_idx[s2]["color"]
        if sum(np.abs(np.array(c1)-np.array(c2))) < 0.01:
          stacked_bar_idx[s2]["color"] = (c2[0]*0.7, c2[1]*0.3, c2[2]*0.9, c2[3])

    # print(json.dumps(stacked_bar_idx, indent=2))

    # fig, axs = plt.subplots(figsize=(f[0]*nb_stacks, f[1]), nrows=1, ncols=nb_stacks)
    # i = 0
    # for d in fix_dataset:
    first = fix_dataset[0] # TODO: some problem with the organization
    idx = 0

    for s_title, _ in first.y_stack.items():
      f = self.figsize
      fig, axs = plt.subplots(figsize=(f[0], f[1]), nrows=1, ncols=1)
      # print(s_title)
      axs.set_title(f"{self.title}\n{s_title[0]}")
      axs.margins(x=0)
      is_percent = s_title[2]
      top_extra = s_title[3]
      fix_100 = s_title[4]
      axs.set_ylabel(s_title[1])

      for d in fix_dataset:
        bottom = np.array([0 for _ in d.x_param.transpose()])
        # breakpoint()
        # (_, ss) = d.y_stack.items()
        for sn, sy in d.y_stack[s_title].items(): #ss.items():
          # breakpoint()
          i = datasets_idx[d.name]
          # print(d.name, sn, "is_percent", is_percent)
          # j = plots_idx[s_title[0]]
          try:
            if is_percent:
              triple = [(np.average(x),np.average(y*100),np.std(y*100)) for x,y in zip(d.x_param.transpose(), sy.transpose())]
            else:
              triple = [(np.average(x),np.average(y),np.std(y)) for x,y in zip(d.x_param.transpose(), sy.transpose())]
            tripleF = triple
            if not d.filter_x_fn is None:
              tripleF = list(filter(d.filter_x_fn, triple))
            tripleF.sort(key=lambda elem : elem[0]) # sort by X
            x_array, y_array, y_error = zip(*tripleF)
          except:
            # print(sn, f"idx={i} backend={d.name} {sy}")
            # breakpoint()
            continue
          xs = np.array([k for k in range(len(x_array))]) + i*width + i*offset
          ys = np.array(y_array)
          y_err = np.array(y_error)
          if len(bottom) > len(xs):
            bottom = np.array([0 for _ in xs])
          # print("X:", xs)
          # print("Y:", ys)
          # print("error:", y_error)
          # breakpoint() # TODO: there is some division by 0
          color = self.colors[sn] if sn in self.colors else stacked_bar_idx[sn]["color"]
          if i == 0: # print label
            # breakpoint()
            axs.bar(xs, ys, width, yerr = y_err, label=sn, bottom=bottom, color=color)
            axs.legend()
          else:
            axs.bar(xs, ys, width, yerr = y_err, bottom=bottom, color=color)
          bottom = bottom + ys
        for x,y in zip(xs,bottom):
          axs.annotate(d.name, (x, y), textcoords="offset points", xytext=(0,4), ha='center', va='baseline', rotation=90)
          break
        axs.set_xlabel(d.x_label)
        axs.set_xticks(np.array([k for k in range(len(x_array))]))
        axs.set_xticklabels([int(x) for x in x_array])

      bottom, top = axs.get_ylim()
      if is_percent and fix_100:
        top = 100
      # print("top", top, "top_extra", top_extra)
      axs.set_ylim(top=top+top*top_extra, bottom=0)

      # plt.tight_layout()
      # print(f"stack_{idx}_{self.filename}")
      plt.savefig(f"stack_{idx}_{self.filename}")
      fig.clear()
      plt.close()
      idx += 1