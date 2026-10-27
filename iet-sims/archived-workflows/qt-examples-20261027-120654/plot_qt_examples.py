#!/usr/bin/env python3
"""Plot native Qt example traces from one experiment index (no legacy CSV conversion)."""
import argparse
import hashlib
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

BASE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, default=BASE / 'trace-results')
    parser.add_argument('--index', type=Path, help='Specific experiment-index JSON; otherwise latest completed batch')
    parser.add_argument('--graphs', type=Path, default=BASE / 'graphs')
    parser.add_argument('--dpi', type=int, default=300)
    args = parser.parse_args()
    index = args.index or args.results.expanduser() / 'qt-examples/experiment-index.json'
    batch = json.loads(index.read_text())
    if batch.get('format') != 'qt-examples-v1' or batch.get('failures') or not batch.get('runs'):
        parser.error('Index must describe a successful Qt example batch')
    groups = {}
    for record in batch['runs']:
        groups.setdefault((record['scenario'], record['variant']), []).append(record)
    output = args.graphs.expanduser() / 'qt-examples' / index.stem
    output.mkdir(parents=True, exist_ok=True)
    generated = []
    for (scenario, variant), records in sorted(groups.items()):
        fig, axes = plt.subplots(3, 1, figsize=(9, 9), sharex=True)
        for record in sorted(records, key=lambda row: row['algorithm']):
            root = Path(record['directory'])
            for relative, checksum in record['traceSha256'].items():
                if hashlib.sha256((root / relative).read_bytes()).hexdigest() != checksum:
                    raise ValueError('Trace changed since run: ' + str(root / relative))
            micro = np.genfromtxt(next((root / 'native').rglob('*-aggMicroMetrics.data')), names=True)
            macro = np.genfromtxt(next((root / 'native').rglob('*-aggMacroMetrics.data')), names=True)
            axes[0].plot(micro['time_s'], micro['inflight_pkts'], label=record['algorithm'])
            axes[1].plot(micro['time_s'], micro['avg_rtt_ms'])
            axes[2].plot(macro['time_s'], macro['interval_goodput_kbps'] / 1000)
        for ax, label in zip(axes, ['In-flight (packets)', 'Mean RTT (ms)', 'Sender estimated\ngoodput (Mbps)']):
            ax.set_ylabel(label)
            ax.grid(alpha=.25)
        axes[0].legend(ncol=3, fontsize=8)
        axes[0].set_title('{} / {} — native Qt example metrics'.format(scenario, variant))
        axes[-1].set_xlabel('Simulation time (s)')
        fig.tight_layout()
        path = output / ('{}-{}.png'.format(scenario, variant))
        fig.savefig(path, dpi=args.dpi)
        plt.close(fig)
        generated.append(str(path.resolve()))
        print('Saved ' + str(path), flush=True)
    (output / 'graph-index.json').write_text(json.dumps({'experimentIndex': str(index.resolve()),
        'experimentIndexSha256': hashlib.sha256(index.read_bytes()).hexdigest(), 'graphs': generated,
        'metricNote': 'Goodput is sender transmitted bytes minus estimated retransmissions, not receiver delivery. RTT is integer ms. Native queue and OU behavior differ from legacy tcp_qt_sims.'}, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
