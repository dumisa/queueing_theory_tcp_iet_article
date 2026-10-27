#!/usr/bin/env python3
"""Run the IET experiment matrix with separate processes and reproducible inputs."""
import argparse
import concurrent.futures
import csv
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


def complete(directory, duration, flows=5):
    """Require finished, nonempty flow summaries and the final measurement."""
    try:
        with (directory / 'aggregate.csv').open() as stream:
            samples = list(csv.DictReader(stream))
        with (directory / 'summary.csv').open() as stream:
            summary = list(csv.DictReader(stream))
        return (bool(samples) and abs(float(samples[-1]['time_s']) - duration) < 1e-8
                and len(summary) == flows
                and (directory / 'totals.txt').is_file()
                and all(int(row['delivered_payload_bytes']) > 0 for row in summary))
    except (OSError, KeyError, ValueError, csv.Error):
        return False


def snapshot(root):
    """Preserve the exact local driver and Qt model sources used for this matrix."""
    names = ['examples/tcp/tcp_qt_sims.cc'] + [
        'src/internet/model/tcp-qt-' + name + suffix
        for name in ('sdr', 'tor') for suffix in ('.cc', '.h')]
    digests = {name: hashlib.sha256((NS3 / name).read_bytes()).hexdigest() for name in names}
    revision = hashlib.sha256(json.dumps(digests, sort_keys=True).encode()).hexdigest()
    destination = root / '_provenance' / revision[:16]
    destination.mkdir(parents=True, exist_ok=True)
    for name in names:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(NS3 / name, target)
    (destination / 'sha256.json').write_text(json.dumps(digests, indent=2) + '\n')
    return revision


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, default=BASE / 'trace-results')
    parser.add_argument('--duration', type=float, default=100.0)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--seed', type=int, default=12345)
    parser.add_argument('--run', type=int, default=1)
    parser.add_argument('--algorithms', nargs='+', default=ALGORITHMS)
    parser.add_argument('--scenarios', nargs='+', default=['static', 'buffer', 'loss', 'stochastic'],
                        choices=['static', 'buffer', 'loss', 'stochastic'])
    parser.add_argument('--tracing', action='store_true', help='Include high-resolution socket traces')
    parser.add_argument('--plot', action='store_true', help='Generate PNGs after successful completion')
    parser.add_argument('--plot-python', default='/opt/anaconda3/bin/python')
    parser.add_argument('--graphs', type=Path, default=BASE / 'graphs')
    args = parser.parse_args()
    if args.duration <= 0 or args.jobs < 1:
        parser.error('duration and jobs must be positive')
    root = args.results.expanduser().resolve()
    root.mkdir(parents=True, exist_ok=True)
    logs = root / '_run-logs'
    logs.mkdir(exist_ok=True)
    binaries = list((NS3 / 'build/examples/tcp').glob('ns3.*-tcp_qt_sims'))
    if len(binaries) != 1:
        parser.error('Build tcp_qt_sims before running this script')
    binary = binaries[0]
    revision = snapshot(root)
    locks = root / '_locks'
    locks.mkdir(exist_ok=True)
    cases = []
    for algorithm in args.algorithms:
        for scenario in args.scenarios:
            variants = {'static': [('reference', [])],
                        'buffer': [(str(q) + 'p', ['--queuePackets=' + str(q)]) for q in (483, 690, 1381)],
                        'loss': [(p, ['--error_p=' + p]) for p in ('1e-6', '1e-4', '1e-2')],
                        'stochastic': [(r, ['--regime=' + r]) for r in ('low', 'moderate', 'high')]}
            for variant, extra in variants[scenario]:
                command = [str(binary), '--scenario=' + scenario, '--transport_prot=' + algorithm,
                           '--sim_name=' + str(root), '--sim_duration=' + str(args.duration),
                           '--rngSeed=' + str(args.seed), '--rngRun=' + str(args.run),
                           '--tracing=' + str(int(args.tracing))] + extra
                label = '{}-{}-{}-{}s-seed{}-run{}'.format(
                    scenario, algorithm, variant, args.duration, args.seed, args.run)
                cases.append((label, command))

    def execute(case):
        label, command = case
        start = time.monotonic()
        check = subprocess.run(command + ['--dryRun=1'], capture_output=True, text=True)
        if check.returncode:
            raise RuntimeError(label + ': validation failed\n' + check.stderr)
        values = [line.split('=', 1)[1] for line in check.stdout.splitlines()
                  if line.startswith('outputDirectory=')]
        if len(values) != 1:
            raise RuntimeError(label + ': cannot resolve output directory')
        directory = Path(values[0])
        lock_id = hashlib.sha256(str(directory).encode()).hexdigest()
        with (locks / (lock_id + '.lock')).open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            return execute_locked(case, directory, start)

    def execute_locked(case, directory, start):
        label, command = case
        metadata_file = directory / 'run-metadata.json'
        old_metadata = json.loads(metadata_file.read_text()) if metadata_file.exists() else {}
        if complete(directory, args.duration) and old_metadata.get('sourceRevision') == revision:
            return {'label': label, 'directory': str(directory), 'status': 'reused',
                    'sourceRevision': revision}
        if directory.exists():
            # A partial run or a changed source snapshot is regenerated explicitly.
            command = command + ['--overwrite=1']
        log = logs / (label + '.log')
        with log.open('w') as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode or not complete(directory, args.duration):
            raise RuntimeError(label + ': simulation incomplete; inspect ' + str(log))
        record = {'label': label, 'directory': str(directory), 'status': 'completed',
                  'command': command, 'sourceRevision': revision,
                  'elapsedSeconds': round(time.monotonic() - start, 3)}
        meta_tmp = metadata_file.with_suffix('.tmp')
        meta_tmp.write_text(json.dumps(record, indent=2) + '\n')
        meta_tmp.replace(metadata_file)
        return record

    records = []
    failures = []
    print('Running {} matched experiments; {} worker processes.'.format(len(cases), args.jobs), flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        pending = {pool.submit(execute, case): case[0] for case in cases}
        for future in concurrent.futures.as_completed(pending):
            try:
                record = future.result()
                records.append(record)
                print('[{}/{}] {}: {}'.format(len(records) + len(failures), len(cases),
                                              record['status'], record['label']), flush=True)
            except Exception as error:
                failures.append(str(error))
                print('FAILED: ' + str(error), file=sys.stderr, flush=True)
            index = {'sourceRevision': revision, 'runs': sorted(records, key=lambda x: x['label']),
                     'failures': failures}
            index_path = root / ('experiment-index-{}.json'.format(os.getpid()))
            temporary = index_path.with_suffix('.tmp')
            temporary.write_text(json.dumps(index, indent=2) + '\n')
            temporary.replace(index_path)
            current_tmp = root / ('experiment-index-{}.current.tmp'.format(os.getpid()))
            current_tmp.write_text(json.dumps(index, indent=2) + '\n')
            current_tmp.replace(root / 'experiment-index.json')
    if failures:
        return 1
    if args.plot:
        return subprocess.call([args.plot_python, str(BASE / 'plot_iet_sims.py'),
                                '--results', str(root), '--graphs', str(args.graphs.expanduser()),
                                '--duration', str(args.duration), '--seed', str(args.seed), '--run', str(args.run)])
    return 0


if __name__ == '__main__':
    sys.exit(main())
