#!/usr/bin/env python3
"""Create PNG comparisons corresponding to IET Figures 3-11 from measured CSVs.

Defaults: trace-results/ -> graphs/, alongside this script. No synthetic data,
interpolation, smoothing, mixing of unmatched settings, or inferred missing runs.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import sys
from collections import defaultdict

BASE = Path(__file__).resolve().parent
# Keep font caches in the experiment workspace, including under restricted hosts.
os.environ.setdefault('MPLCONFIGDIR', str(BASE / '.matplotlib-cache'))
os.environ.setdefault('XDG_CACHE_HOME', str(BASE / '.cache'))
(BASE / '.cache').mkdir(exist_ok=True)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from matplotlib.ticker import FixedLocator, FuncFormatter
import numpy as np

ALGORITHMS = ['TcpNewReno', 'TcpCubic', 'TcpBbr', 'TcpQtSdr', 'TcpQtTor']
LABELS = dict(zip(ALGORITHMS, ['NewReno', 'Cubic', 'BBR', 'QtSDR', 'QtTOR']))
COLORS = dict(zip(ALGORITHMS, ['#4477AA', '#EE6677', '#228833', '#CCAA22', '#AA3377']))
REGIMES = {'low': (8.0, 0.15), 'moderate': (2.0, 0.45), 'high': (0.5, 1.0)}
# Compare only runs with the same conditions. Intentional swept variables are
# excluded: transport, scenario, queuePackets, errorRate, ouTau, ouSigma, regime.
COMMON_KEYS = ['bottleneckRate', 'bottleneckDelay', 'accessRate', 'accessDelay',
               'flows', 'mss', 'mtu', 'accessQueuePackets', 'socketBuffer',
               'initialCwnd', 'delayedAckCount', 'duration', 'appStart', 'warmup',
               'interval', 'samplingPhaseSeconds', 'alpha', 'epsilon', 'muGain', 'muMin', 'minControlScale',
               'controlFraction', 'seed', 'run', 'ouStream', 'tcpStream', 'errorStream',
               'recovery', 'reverseErrors', 'sack', 'timestamps', 'ouStart',
               'ouInterval', 'ouLower', 'ouUpper', 'bidirectionalOu',
               'qtTorFixed', 'controllerVersion']


def keyvalues(path):
    result = {}
    for line in path.read_text().splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            result[key] = value
    return result


def close(a, b):
    return math.isclose(float(a), float(b), rel_tol=1e-9, abs_tol=1e-12)


class Run:
    def __init__(self, manifest):
        self.path = manifest.parent
        self.config = keyvalues(manifest)
        self.cache = {}
        meta = self.path / 'run-metadata.json'
        self.revision = json.loads(meta.read_text()).get('sourceRevision', 'unrecorded') if meta.exists() else 'unrecorded'

    def rows(self, name):
        if name not in self.cache:
            with (self.path / name).open() as stream:
                self.cache[name] = list(csv.DictReader(stream))
        return self.cache[name]

    def column(self, name, field, finite=False):
        values = np.asarray([float(row[field]) for row in self.rows(name)], dtype=float)
        return values[np.isfinite(values)] if finite else values

    def validate(self):
        samples = self.rows('aggregate.csv')
        summary = self.rows('summary.csv')
        if not samples or not close(samples[-1]['time_s'], self.config['duration']):
            raise ValueError('Incomplete measurement window: ' + str(self.path))
        if len(summary) != int(self.config['flows']):
            raise ValueError('Incomplete flow summary: ' + str(self.path))
        if not (self.path / 'totals.txt').exists():
            raise ValueError('Missing final totals: ' + str(self.path))
        delivered = sum(int(row['delivered_payload_bytes']) for row in summary)
        sampled = sum(float(row['goodput_mbps']) * 1e6 * float(row['interval_s']) / 8 for row in samples)
        if not math.isclose(delivered, sampled, abs_tol=0.5):
            raise ValueError('Receiver bytes and sampled goodput disagree: ' + str(self.path))

    def family(self):
        settings = {key: self.config.get(key) for key in COMMON_KEYS}
        settings['sourceRevision'] = self.revision
        return hashlib.sha256(json.dumps(settings, sort_keys=True).encode()).hexdigest()[:16]

    def cell(self):
        c = self.config
        scenario = c.get('scenario')
        q = int(c.get('queuePackets', 0))
        error = float(c.get('errorRate', 0))
        if scenario == 'static' and q == 1381 and close(error, 0):
            variant = 'reference'
        elif scenario == 'buffer' and q in (483, 690, 1381) and close(error, 0):
            variant = str(q)
        elif scenario == 'loss' and q == 1381 and any(close(error, p) for p in (1e-6, 1e-4, 1e-2)):
            variant = next(str(p) for p in (1e-6, 1e-4, 1e-2) if close(error, p))
        elif scenario == 'stochastic' and q == 1381 and close(error, 0):
            regime = c.get('regime')
            if regime not in REGIMES or not (close(c['ouTau'], REGIMES[regime][0]) and close(c['ouSigma'], REGIMES[regime][1])):
                return None
            variant = regime
        else:
            return None
        return scenario, variant, c.get('transport')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, default=BASE / 'trace-results')
    parser.add_argument('--graphs', type=Path, default=BASE / 'graphs')
    parser.add_argument('--duration', type=float, default=100.0)
    parser.add_argument('--seed', type=int, default=12345)
    parser.add_argument('--run', type=int, default=1)
    parser.add_argument('--family', help='Exact settings/source family ID when multiple matrices exist')
    parser.add_argument('--allow-partial', action='store_true', help='Skip incomplete comparisons; never invent missing data')
    parser.add_argument('--dpi', type=int, default=300)
    args = parser.parse_args()
    root = args.results.expanduser().resolve()
    output = args.graphs.expanduser().resolve()
    if args.dpi < 72:
        parser.error('dpi must be at least 72')
    families = defaultdict(list)
    rejected = []
    for manifest in sorted(root.glob('*/*/*/resolved-config.txt')):
        try:
            run = Run(manifest)
            c = run.config
            if c.get('transport') not in ALGORITHMS or not close(c.get('duration', -1), args.duration):
                continue
            if int(c.get('seed', -1)) != args.seed or int(c.get('run', -1)) != args.run or run.cell() is None:
                continue
            run.validate()
            families[run.family()].append(run)
        except (OSError, KeyError, ValueError) as error:
            rejected.append(str(error))
    if not families:
        parser.error('No completed, matching experiment matrix found in ' + str(root))
    if args.family:
        if args.family not in families:
            parser.error('Unknown family. Available: ' + ', '.join(families))
        family = args.family
    else:
        ranked = sorted(families, key=lambda key: len(set(r.cell() for r in families[key])), reverse=True)
        family = ranked[0]
        if len(ranked) > 1 and len(set(r.cell() for r in families[ranked[0]])) == len(set(r.cell() for r in families[ranked[1]])):
            parser.error('Multiple equally complete settings families; select --family=' + '|'.join(ranked))
    runs = families[family]
    cells = {}
    for run in runs:
        if run.cell() in cells:
            parser.error('Duplicate comparison cell; separate result roots for alternate tracing/other settings: ' + str(run.cell()))
        cells[run.cell()] = run
    c = runs[0].config
    subtitle = '{} flows | {} | {} s | seed {}/run {}'.format(c['flows'], c['bottleneckRate'], format(float(c['duration']), '.6g'), c['seed'], c['run'])
    footer = 'Sampling interval: {} s; measurement window: [{}, {}] s; boxplots show temporal distributions.'.format(format(float(c['interval']), '.6g'), format(float(c['warmup']), '.6g'), format(float(c['duration']), '.6g'))
    output.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10,
                         'axes.titlesize': 14, 'axes.labelsize': 11,
                         'axes.spines.top': False, 'axes.spines.right': False,
                         'axes.axisbelow': True, 'savefig.facecolor': 'white'})
    graph_records = []
    box_statistics = []
    skipped = []

    def cases(scenario, variants):
        keys = [(scenario, variant, alg) for variant in variants for alg in ALGORITHMS]
        missing = [key for key in keys if key not in cells]
        if missing:
            message = '{} missing cells: {}'.format(scenario, missing)
            if not args.allow_partial:
                raise ValueError(message + '. Finish the experiment matrix or use --allow-partial.')
            skipped.append(message)
            return None
        return {(variant, alg): cells[(scenario, variant, alg)] for variant in variants for alg in ALGORITHMS}

    # Check required coverage before writing a new set of figures.
    static = cases('static', ['reference'])
    buffers = cases('buffer', ['483', '690', '1381'])
    losses = cases('loss', [str(p) for p in (1e-6, 1e-4, 1e-2)])
    stochastic = cases('stochastic', ['low', 'moderate', 'high'])

    def decorate(ax, title, ylabel, xlabel=None):
        ax.set_title(title + '\n' + subtitle, loc='left', pad=14)
        ax.set_ylabel(ylabel)
        if xlabel:
            ax.set_xlabel(xlabel)
        ax.grid(axis='y', color='#DDE1E6', linewidth=0.7)

    def save(fig, name, used, metric):
        fig.text(0.01, 0.012, footer, fontsize=8, color='#555555')
        fig.tight_layout(rect=(0, 0.045, 1, 1))
        path = output / name
        fig.savefig(path, dpi=args.dpi)
        plt.close(fig)
        source_runs = sorted(set(used), key=lambda item: str(item.path))
        graph_records.append({'png': str(path), 'metric': metric,
                              'sourceRuns': [str(item.path) for item in source_runs],
                              'aggregateSha256': {str(item.path): hashlib.sha256((item.path / 'aggregate.csv').read_bytes()).hexdigest() for item in source_runs}})
        print('Saved ' + str(path), flush=True)

    def stats(run, metric, label):
        data = run.column('aggregate.csv', metric, finite=True)
        if not len(data):
            raise ValueError('No finite observations for ' + metric + ': ' + str(run.path))
        q = np.percentile(data, [0, 25, 50, 75, 100])
        box_statistics.append({'scenario': run.config['scenario'], 'variant': label,
                               'algorithm': run.config['transport'], 'metric': metric,
                               'samples': len(data), 'minimum': q[0], 'q1': q[1],
                               'median': q[2], 'q3': q[3], 'maximum': q[4],
                               'source': str(run.path)})
        return data

    def power_axis(ax, values):
        # Preserve rare large values without flattening the main 0-1 distribution.
        peak = max(float(np.max(v)) for v in values)
        if peak > 1.5:
            ax.set_yscale('symlog', linthresh=1.0, linscale=1.5, base=2)
            ticks = [0, 0.25, 0.5, 0.75, 1]
            exponent = 1
            while 2 ** exponent <= peak * 1.05:
                ticks.append(2 ** exponent)
                exponent += 1
            ax.yaxis.set_major_locator(FixedLocator(ticks))
            ax.yaxis.set_major_formatter(FuncFormatter(lambda value, _: '{:g}'.format(value)))
            ax.set_ylim(0, peak * 1.05)
            ax.set_ylabel('Normalized interval-averaged Power\n(linear below 1; logarithmic above 1)')

    def style_boxes(box, colors):
        for patch, color in zip(box['boxes'], colors):
            patch.set_facecolor(color)
            patch.set_alpha(0.65)
            patch.set_edgecolor('#333333')
        for median in box['medians']:
            median.set_color('#111111')
            median.set_linewidth(1.5)

    if static:
        used = [static[('reference', alg)] for alg in ALGORITHMS]
        fig, ax = plt.subplots(figsize=(9.2, 4.9))
        for alg, run in zip(ALGORITHMS, used):
            times = run.column('aggregate.csv', 'time_s')
            values = run.column('aggregate.csv', 'inflight_packets')
            selected = times <= min(60.0, args.duration)
            ax.plot(times[selected], values[selected], color=COLORS[alg], label=LABELS[alg], linewidth=1.5)
        ax.axhline(float(c['baseBdpPackets']), color='#222222', linestyle='--', linewidth=1.2, label='Base BDP')
        ax.set_xlim(0, min(60, args.duration))
        ax.set_ylim(bottom=0)
        decorate(ax, 'Static bottleneck: aggregate in-flight data', 'In-flight data (packets)', 'Time (s)')
        ax.legend(ncol=3, fontsize=9, loc='upper right', framealpha=0.95)
        save(fig, 'fig03_static_inflight.png', used, 'inflight_packets; first 60 seconds')

        for name, metric, ylabel, title in [
                ('fig04_static_rtt.png', 'mean_rtt_ms', 'Mean flow RTT (ms)', 'Static bottleneck: RTT distribution'),
                ('fig05_static_power.png', 'power_norm', 'Normalized interval-averaged Power', 'Static bottleneck: throughput-delay efficiency')]:
            fig, ax = plt.subplots(figsize=(8.4, 4.9))
            data = [stats(run, metric, 'reference') for run in used]
            boxes = ax.boxplot(data, labels=[LABELS[alg] for alg in ALGORITHMS], patch_artist=True,
                               widths=0.55, flierprops={'marker': '.', 'markersize': 2, 'alpha': 0.4})
            style_boxes(boxes, [COLORS[alg] for alg in ALGORITHMS])
            decorate(ax, title, ylabel)
            ax.set_ylim(bottom=0)
            if metric == 'power_norm':
                power_axis(ax, data)
            save(fig, name, used, metric + '; temporal distribution of finite samples')

        for name, ylabel, title, fairness in [
                ('fig06_static_goodput.png', 'Time-averaged aggregate goodput (Mbps)', 'Static bottleneck: receiver-delivered goodput', False),
                ('fig07_static_fairness.png', "Jain's fairness index", 'Static bottleneck: flow-level fairness', True)]:
            values = []
            for run in used:
                if fairness:
                    value = float(keyvalues(run.path / 'totals.txt')['jainMeanGoodput'])
                else:
                    value = sum(float(row['mean_goodput_mbps']) for row in run.rows('summary.csv'))
                values.append(value)
            fig, ax = plt.subplots(figsize=(8.4, 4.9))
            bars = ax.bar(range(len(values)), values, color=[COLORS[alg] for alg in ALGORITHMS], width=0.62)
            ax.set_xticks(range(len(values)))
            ax.set_xticklabels([LABELS[alg] for alg in ALGORITHMS])
            ax.set_ylim(0, 1.12 if fairness else max(values) * 1.18)
            for bar, value in zip(bars, values):
                ax.annotate(('{:.3f}' if fairness else '{:.2f}').format(value),
                            (bar.get_x() + bar.get_width()/2, value), xytext=(0, 5),
                            textcoords='offset points', ha='center', fontsize=10)
            decorate(ax, title, ylabel)
            save(fig, name, used, 'Jain of time-averaged per-flow receiver goodput' if fairness else 'sum of per-flow mean receiver goodput')

    def grouped(group, variants, labels, name, metric, title, ylabel, xlabel):
        if not group:
            return
        fig, ax = plt.subplots(figsize=(10.2, 5.1))
        centers = np.arange(len(variants), dtype=float)
        width = 0.13
        used = []
        all_values = []
        for i, alg in enumerate(ALGORITHMS):
            positions = centers + (i - 2) * width * 1.15
            data = []
            for variant in variants:
                run = group[(variant, alg)]
                used.append(run)
                data.append(stats(run, metric, variant))
            all_values.extend(data)
            box = ax.boxplot(data, positions=positions, widths=width, patch_artist=True,
                             manage_ticks=False,
                             flierprops={'marker': '.', 'markersize': 1.8, 'alpha': 0.35})
            style_boxes(box, [COLORS[alg]] * len(variants))
        ax.set_xticks(centers)
        ax.set_xticklabels(labels)
        ax.set_xlim(-0.6, len(variants) - 0.4)
        ax.set_ylim(bottom=0)
        decorate(ax, title, ylabel, xlabel)
        if metric == 'power_norm':
            power_axis(ax, all_values)
        ax.legend(handles=[Patch(facecolor=COLORS[alg], alpha=0.65, label=LABELS[alg]) for alg in ALGORITHMS],
                  ncol=5, loc='upper center', bbox_to_anchor=(0.5, 1.02), fontsize=9, framealpha=0.95)
        save(fig, name, used, metric + '; matched temporal distributions by category')

    grouped(buffers, ['483', '690', '1381'], ['483', '690', '1381'],
            'fig08_buffer_power.png', 'power_norm', 'Buffer sensitivity',
            'Normalized interval-averaged Power', 'Bottleneck DropTail capacity (packets)')
    grouped(losses, [str(p) for p in (1e-6, 1e-4, 1e-2)], ['$10^{-6}$', '$10^{-4}$', '$10^{-2}$'],
            'fig09_loss_power.png', 'power_norm', 'Noncongestive packet-loss sensitivity',
            'Normalized interval-averaged Power', 'Forward bottleneck packet error probability')
    grouped(stochastic, ['low', 'moderate', 'high'], ['Low', 'Moderate', 'High'],
            'fig10_stochastic_power.png', 'power_norm', 'Stochastic service: throughput-delay efficiency',
            'Normalized interval-averaged Power', 'OU service volatility regime')
    grouped(stochastic, ['low', 'moderate', 'high'], ['Low', 'Moderate', 'High'],
            'fig11_stochastic_rtt.png', 'mean_rtt_ms', 'Stochastic service: RTT distributions',
            'Mean flow RTT (ms)', 'OU service volatility regime')

    if stochastic:
        fig, axes = plt.subplots(3, 1, figsize=(10.2, 7.4), sharex=True)
        used = []
        for ax, regime in zip(axes, ['low', 'moderate', 'high']):
            representatives = [stochastic[(regime, alg)] for alg in ALGORITHMS]
            signatures = {hashlib.sha256((item.path / 'capacity.csv').read_bytes()).hexdigest() for item in representatives}
            if len(signatures) != 1:
                raise ValueError('OU capacity paths differ across algorithms for ' + regime)
            run = representatives[0]
            used.extend(representatives)
            times = run.column('capacity.csv', 'time_s')
            rates = run.column('capacity.csv', 'forward_bps') * 1e-6
            ax.plot(times, rates, linewidth=0.85, color='#4477AA')
            ax.set_title('{} volatility: tau={} s, sigma={}'.format(regime.capitalize(), *REGIMES[regime]), loc='left', fontsize=11)
            ax.set_ylabel('Capacity (Mbps)')
            ax.grid(axis='y', color='#DDE1E6')
        axes[0].set_title('Realised OU capacity paths\n' + subtitle + '\nLow volatility: tau=8 s, sigma=0.15', loc='left', fontsize=12)
        axes[-1].set_xlabel('Time (s)')
        axes[-1].set_xlim(0, args.duration)
        save(fig, 'supplement_ou_capacity.png', used, 'physical forward capacity; matching SHA-256 across algorithms')

    if box_statistics:
        with (output / 'boxplot-statistics.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(box_statistics[0]))
            writer.writeheader()
            writer.writerows(box_statistics)
    index = {'plotScriptSha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
             'matplotlibVersion': matplotlib.__version__, 'numpyVersion': np.__version__,
             'family': family, 'settings': {key: c.get(key) for key in COMMON_KEYS},
             'sourceRevision': runs[0].revision, 'figures': graph_records,
             'skippedComparisons': skipped, 'rejectedRuns': rejected,
             'notes': ['Measured CSV data only; NaNs omitted from distributions.',
                       'Boxplots use temporal 0.1-second samples, not independent replications.',
                       'Whiskers: 1.5 IQR; outliers shown. No smoothing or y-value clipping.',
                       'Power plots with peaks above 1.5 use a labelled linear-below-1/log-above-1 scale.',
                       'Local improved Qt models; figures do not assert reproduction of published values.']}
    (output / 'graph-index.json').write_text(json.dumps(index, indent=2) + '\n')
    print('{} PNG graphs generated in {}'.format(len(graph_records), output), flush=True)
    for message in skipped:
        print('Skipped: ' + message)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print('plot_iet_sims: ' + str(error), file=sys.stderr)
        sys.exit(1)
