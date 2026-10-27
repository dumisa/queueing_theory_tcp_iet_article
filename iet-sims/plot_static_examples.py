#!/usr/bin/env python3
"""Create individual static-case plots using native example trace definitions."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

BASE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--index', type=Path, default=BASE / 'trace-results/qt-examples/experiment-index.json')
parser.add_argument('--graphs', type=Path, default=BASE / 'graphs/qt-examples')
args = parser.parse_args()
batch = json.loads(args.index.read_text())
records = sorted([r for r in batch['runs'] if r['scenario'] == 'static'], key=lambda r: r['algorithm'])
if len(records) != 5 or batch.get('failures'):
    raise ValueError('Require a successful five-algorithm static batch')
output = args.graphs / args.index.stem
output.mkdir(parents=True, exist_ok=True)
data = []
for record in records:
    root = Path(record['directory'])
    for relative, sha in record['traceSha256'].items():
        if hashlib.sha256((root / relative).read_bytes()).hexdigest() != sha:
            raise ValueError('Trace checksum mismatch: ' + str(root / relative))
    micro = np.genfromtxt(next((root / 'native').rglob('*aggMicroMetrics.data')), names=True)
    macro = np.genfromtxt(next((root / 'native').rglob('*aggMacroMetrics.data')), names=True)
    data.append((record, micro, macro))
paths = []

def save(fig, name):
    fig.tight_layout()
    path = output / name
    fig.savefig(path, dpi=300)
    plt.close(fig)
    paths.append(str(path.resolve()))
    print(path, flush=True)

fig, ax = plt.subplots(figsize=(8, 4.5))
for record, micro, macro in data:
    selected = micro['time_s'] <= 60
    ax.plot(micro['time_s'][selected], micro['inflight_pkts'][selected], label=record['algorithm'])
ax.set(xlabel='Simulation time (s)', ylabel='Aggregate in-flight (packets)', title='Static bottleneck: first 60 seconds')
ax.legend(ncol=3, fontsize=8); ax.grid(alpha=.25)
save(fig, 'static-inflight.png')

fig, ax = plt.subplots(figsize=(8, 4.5))
values = [micro['avg_rtt_ms'][micro['avg_rtt_ms'] > 0] for _, micro, _ in data]
ax.boxplot(values, labels=[r['algorithm'] for r, _, _ in data], showfliers=True)
ax.set(ylabel='Mean RTT across flows (ms)', title='Static bottleneck: RTT distribution')
ax.grid(axis='y', alpha=.25)
save(fig, 'static-rtt-distribution.png')

fig, ax = plt.subplots(figsize=(8, 4.5))
means = []
for record, micro, macro in data:
    # Native average is cumulative sender payload minus estimated retransmissions / elapsed time.
    means.append(float(macro['average_goodput_kbps'][-1]) / 1000)
ax.bar([r['algorithm'] for r, _, _ in data], means)
ax.set(ylabel='Sender-estimated goodput (Mbps)', title='Static bottleneck: cumulative mean at final sample')
ax.grid(axis='y', alpha=.25)
save(fig, 'static-sender-goodput.png')

fig, ax = plt.subplots(figsize=(8, 4.5))
for record, micro, macro in data:
    ax.plot(macro['time_s'], macro['fairness'], label=record['algorithm'])
ax.set(xlabel='Simulation time (s)', ylabel="Jain's index (sender trace estimate)", title='Static bottleneck: native fairness estimate', ylim=(0, 1.02))
ax.legend(ncol=3, fontsize=8); ax.grid(alpha=.25)
save(fig, 'static-fairness.png')

with (output / 'static-summary.csv').open('w', newline='') as stream:
    writer = csv.writer(stream)
    writer.writerow(['algorithm', 'duration_s', 'last_sample_s', 'sender_estimated_mean_goodput_mbps', 'median_mean_rtt_ms', 'final_native_fairness'])
    for (record, micro, macro), mean, rtts in zip(data, means, values):
        writer.writerow([record['algorithm'], record['duration'], macro['time_s'][-1], mean, float(np.median(rtts)), macro['fairness'][-1]])
(output / 'static-graphs.json').write_text(json.dumps({'index': str(args.index.resolve()), 'graphs': paths,
    'notes': ['Native PfifoFast queue configuration.', 'Goodput and fairness use sender trace estimates; receiver delivery is not measured.',
              'RTT is quantized to integer milliseconds; zero initial RTT samples excluded from boxplot.',
              'No article Power figure is inferred from the sender estimator.']}, indent=2) + '\n')
