#!/usr/bin/env python3
"""Run the compiled Qt examples; preserve native .data traces and reproducible manifests."""
import argparse
import concurrent.futures
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

BASE = Path(__file__).resolve().parent
NS3 = BASE.parent / 'ns-3.49'
ALGORITHMS = ['TcpQtSdr', 'TcpQtTor', 'TcpNewReno', 'TcpCubic', 'TcpBbr']
TARGETS = {'static': 'tcp_qt_static', 'buffer': 'tcp_qt_static',
           'loss': 'tcp_qt_static', 'stochastic': 'tcp_qt_stochBW',
           'step-delay': 'tcp_qt_step_delay'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atomic_json(path, value):
    temp = path.with_name(path.name + '.{}.tmp'.format(os.getpid()))
    temp.write_text(json.dumps(value, indent=2) + '\n')
    temp.replace(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--results', type=Path, default=BASE / 'trace-results')
    p.add_argument('--graphs', type=Path, default=BASE / 'graphs')
    p.add_argument('--duration', type=float, default=100)
    p.add_argument('--interval', type=float, default=0.1)
    p.add_argument('--flows', type=int, default=5)
    p.add_argument('--jobs', type=int, default=2)
    p.add_argument('--seed', type=int, default=12345)
    p.add_argument('--run', type=int, default=1)
    p.add_argument('--algorithms', nargs='+', choices=ALGORITHMS, default=ALGORITHMS)
    p.add_argument('--scenarios', nargs='+', choices=list(TARGETS),
                   default=['static', 'buffer', 'loss', 'stochastic'])
    p.add_argument('--tracing', action='store_true', help='Compatibility flag: socket tracing is always enabled for metric collection')
    p.add_argument('--plot', action='store_true')
    p.add_argument('--no-plot', action='store_false', dest='plot')
    p.add_argument('--plot-python', default='/opt/anaconda3/bin/python')
    p.add_argument('--dry-run', action='store_true', help='Print commands without running simulations or writing results')
    a = p.parse_args()
    import math
    if (not math.isfinite(a.duration) or a.duration <= 3 or not math.isfinite(a.interval)
            or not 0 < a.interval < a.duration or a.jobs < 1 or not 1 <= a.flows <= 255
            or not 1 <= a.seed < 2**32 or not 1 <= a.run < 2**64):
        p.error('Require duration > 3 s, 0 < interval < duration, jobs >= 1, flows 1..255 and positive valid seed/run')
    root = a.results.expanduser().resolve() / 'qt-examples'
    sources = [NS3 / 'examples/tcp' / n for n in
               ('tcp_qt_examples.h', 'tcp_qt_static.cc', 'tcp_qt_step_delay.cc', 'tcp_qt_stochBW.cc')]
    sources += [NS3 / 'src/internet/model' / ('tcp-qt-' + n + ext)
                for n in ('sdr', 'tor') for ext in ('.cc', '.h')]
    for module in ('my-dumbbell', 'my_maths_tools'):
        sources += sorted(path for path in (NS3 / 'contrib' / module).rglob('*')
                          if path.suffix in ('.cc', '.h') or path.name == 'CMakeLists.txt')
    source_hashes = {str(path.relative_to(NS3)): digest(path) for path in sources}
    revision = hashlib.sha256(json.dumps(source_hashes, sort_keys=True).encode()).hexdigest()
    binaries = {}
    for target in set(TARGETS[s] for s in a.scenarios):
        candidates = list((NS3 / 'build/examples/tcp').glob('ns3.*-' + target))
        if len(candidates) != 1:
            p.error('Run sim_scripts/build.sh first: missing or ambiguous ' + target)
        binaries[target] = candidates[0]
    cases = []
    for scenario in dict.fromkeys(a.scenarios):
        variants = {'static': [('reference', [])],
                    'buffer': [(str(q) + 'p', ['--queueSize_bytes=' + str(q * 1448)]) for q in (483, 690, 1381)],
                    'loss': [(r, ['--error_p=' + r]) for r in ('1e-6', '1e-4', '1e-2')],
                    'stochastic': [(r, ['--stoch_tau=' + str(tau), '--stoch_sigmaLog=' + str(sigma),
                                       '--stoch_upperBW=175Mbps', '--rngSeed=' + str(a.seed), '--rngRun=' + str(a.run)])
                                   for r, tau, sigma in [('low', 8, .15), ('moderate', 2, .45), ('high', .5, 1)]],
                    'step-delay': [('reference', [])]}[scenario]
        target = TARGETS[scenario]
        for algorithm in dict.fromkeys(a.algorithms):
            for variant, extra in variants:
                options = ['--transport_prot=' + algorithm, '--sim_duration=' + str(a.duration),
                           '--num_flows=' + str(a.flows), '--data_logging_interval=' + str(a.interval),
                           '--queueSize_bytes=' + str(1381 * 1448), '--tracing=true', '--logging=true',
                           '--progress=false', '--RngSeed=' + str(a.seed), '--RngRun=' + str(a.run)] + extra
                identity = {'sourceRevision': revision, 'binarySha256': digest(binaries[target]),
                            'scenario': scenario, 'variant': variant, 'target': target, 'options': options}
                key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()[:16]
                directory = root / scenario / algorithm / variant / ('seed{}-run{}-{}'.format(a.seed, a.run, key))
                command = [str(binaries[target])] + options + ['--sim_name=' + str(directory / 'native')]
                cases.append((directory, dict(identity, algorithm=algorithm, duration=a.duration,
                              interval=a.interval, flows=a.flows, seed=a.seed, run=a.run, command=command)))
    if a.dry_run:
        print(json.dumps([dict(record, directory=str(directory)) for directory, record in cases], indent=2))
        return 0
    root.mkdir(parents=True, exist_ok=True)
    provenance = root / '_provenance' / revision
    provenance.mkdir(parents=True, exist_ok=True)
    for source in sources:
        destination = provenance / source.relative_to(NS3)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            shutil.copy2(source, destination)
    atomic_json(provenance / 'sha256.json', source_hashes)

    def execute(case):
        directory, record = case
        directory.mkdir(parents=True, exist_ok=True)
        with (directory / '.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            metadata = directory / 'run-metadata.json'
            if metadata.exists():
                previous = json.loads(metadata.read_text())
                if previous.get('status') == 'completed' and all(
                        (directory / path).is_file() and digest(directory / path) == sha
                        for path, sha in previous.get('traceSha256', {}).items()) and previous.get('traceSha256'):
                    return dict(previous, status='reused')
            native = directory / 'native'
            if native.exists():
                native.rename(directory / ('partial-' + str(time.time_ns())))
            start = time.monotonic()
            with (directory / 'console.log').open('w') as stream:
                result = subprocess.run(record['command'], cwd=NS3, stdout=stream, stderr=subprocess.STDOUT)
            files = list(native.rglob('*.data'))
            for suffix in ('aggMicroMetrics.data', 'aggMacroMetrics.data'):
                matches = list(native.rglob('*-' + suffix))
                if len(matches) != 1:
                    raise RuntimeError('Missing metrics: ' + str(directory))
                lines = matches[0].read_text().splitlines()
                last = float(lines[-1].split()[0]) if len(lines) > 1 else -1
                if last < a.duration - a.interval - 1e-6:
                    raise RuntimeError('Incomplete metrics: ' + str(directory))
            if result.returncode:
                raise RuntimeError('Simulation failed: ' + str(directory / 'console.log'))
            record = dict(record, directory=str(directory), status='completed',
                          elapsedSeconds=round(time.monotonic() - start, 3),
                          traceSha256={str(path.relative_to(directory)): digest(path) for path in files})
            atomic_json(metadata, record)
            return record

    records, failures = [], []
    print('Running {} Qt example experiments with {} workers'.format(len(cases), a.jobs), flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        pending = {pool.submit(execute, case): case for case in cases}
        for future in concurrent.futures.as_completed(pending):
            try:
                record = future.result()
                records.append(record)
                print('{}: {} {} {}'.format(record['status'], record['scenario'], record['algorithm'], record['variant']), flush=True)
            except Exception as error:
                failures.append(str(error))
                print('FAILED: ' + str(error), file=sys.stderr, flush=True)
    index = root / ('experiment-index-{}-{}.json'.format(time.time_ns(), os.getpid()))
    atomic_json(index, {'format': 'qt-examples-v1', 'sourceRevision': revision, 'runs': records, 'failures': failures})
    if failures:
        return 1
    atomic_json(root / 'experiment-index.json', json.loads(index.read_text()))
    if a.plot:
        return subprocess.call([a.plot_python, str(BASE / 'plot_qt_examples.py'), '--index', str(index),
                                '--graphs', str(a.graphs.expanduser())])
    return 0


if __name__ == '__main__':
    sys.exit(main())
