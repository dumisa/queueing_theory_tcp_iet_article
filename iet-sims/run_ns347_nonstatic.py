#!/usr/bin/env python3
"""Run ns-3.47 buffer/loss/stochastic matrices with isolated versioned outputs."""
from pathlib import Path
import hashlib,json,os,shutil,sys
import run_iet_sims as runner
BASE=Path(__file__).resolve().parent
NS3=BASE.parent/'ns-3.47'
runner.NS3=NS3

def snapshot(root):
    names=['VERSION','examples/tcp/tcp_qt_sims.cc','src/internet/CMakeLists.txt','examples/tcp/CMakeLists.txt']
    names += ['src/internet/model/tcp-qt-'+name+ext for name in ('sdr','tor') for ext in ('.cc','.h')]
    names += ['build/examples/tcp/ns3.47-tcp_qt_sims','build/lib/libns3.47-internet.dylib']
    hashes={name:hashlib.sha256((NS3/name).read_bytes()).hexdigest() for name in names}
    revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    dest=root/'_provenance'/revision[:16];dest.mkdir(parents=True,exist_ok=True)
    for name in names:
        target=dest/name;target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():shutil.copy2(NS3/name,target)
    (dest/'sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
    (dest/'engine.json').write_text(json.dumps({'ns3Version':'3.47','rule':'default <=',
        'installation':str(NS3),'library':str(NS3/'build/lib/libns3.47-internet.dylib')},indent=2)+'\n')
    return revision

if __name__=='__main__':
    default=NS3/'qt-window-rule-backup/tcp-qt-sdr.default-original-le.cc'
    assert (NS3/'src/internet/model/tcp-qt-sdr.cc').read_bytes()==default.read_bytes(),'Default <= source changed'
    # Explicitly choose the installed library, excluding any experimental override.
    os.environ['DYLD_LIBRARY_PATH']=str(NS3/'build/lib')
    runner.snapshot=snapshot
    sys.argv[1:1]=['--results',str(BASE/'trace-results/ns-3.47/buffer-loss-stochastic-default-original-timing'),
                  '--graphs',str(BASE/'graphs/ns-3.47/buffer-loss-stochastic-default-original-timing'),
                  '--scenarios','buffer','loss','stochastic','--jobs','4']
    sys.exit(runner.main())
