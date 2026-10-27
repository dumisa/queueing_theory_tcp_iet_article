#!/usr/bin/env python3
"""Run the five static IET cases with ns-3.47 and version-separated outputs."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import run_iet_sims as runner

BASE = Path(__file__).resolve().parent
runner.NS3 = BASE.parent / 'ns-3.47'


def snapshot(root):
    ns3 = runner.NS3
    names = ['VERSION', 'examples/tcp/tcp_qt_sims.cc', 'src/internet/CMakeLists.txt',
             'examples/tcp/CMakeLists.txt'] + [
        'src/internet/model/tcp-qt-' + name + suffix
        for name in ('sdr', 'tor') for suffix in ('.cc', '.h')]
    names += [str(path.relative_to(ns3)) for path in
              (ns3 / 'build/examples/tcp').glob('ns3.*-tcp_qt_sims')]
    digests = {name: hashlib.sha256((ns3 / name).read_bytes()).hexdigest() for name in names}
    revision = hashlib.sha256(json.dumps(digests, sort_keys=True).encode()).hexdigest()
    destination = root / '_provenance' / revision[:16]
    destination.mkdir(parents=True, exist_ok=True)
    for name in names:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(ns3 / name, target)
    (destination / 'sha256.json').write_text(json.dumps(digests, indent=2) + '\n')
    (destination / 'engine.json').write_text(json.dumps({'ns3Version': (ns3 / 'VERSION').read_text().strip(),
                                                       'installation': str(ns3)}, indent=2) + '\n')
    return revision


if __name__ == '__main__':
    runner.snapshot = snapshot
    sys.argv[1:1] = ['--results', str(BASE / 'trace-results/ns-3.47'),
                     '--graphs', str(BASE / 'graphs/ns-3.47'), '--scenarios', 'static']
    sys.exit(runner.main())
