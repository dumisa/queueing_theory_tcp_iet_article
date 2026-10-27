#!/usr/bin/env python3
"""Run the ns-3.49 IET matrix with the default Qt models ported from ns-3.47."""
from pathlib import Path
import hashlib,json,os,shutil,sys
import run_iet_sims as runner
BASE=Path(__file__).resolve().parent
NS3=BASE.parent/'ns-3.49'
runner.NS3=NS3

def snapshot(root):
    names=['VERSION','examples/tcp/tcp_qt_sims.cc','src/internet/CMakeLists.txt','examples/tcp/CMakeLists.txt']
    names += ['src/internet/model/tcp-qt-'+name+ext for name in ('sdr','tor') for ext in ('.cc','.h')]
    names += ['build/examples/tcp/ns3.49-tcp_qt_sims','build/lib/libns3.49-internet.dylib']
    hashes={name:hashlib.sha256((NS3/name).read_bytes()).hexdigest() for name in names}
    revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    dest=root/'_provenance'/revision[:16];dest.mkdir(parents=True,exist_ok=True)
    for name in names:
        target=dest/name;target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():shutil.copy2(NS3/name,target)
    (dest/'sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
    (dest/'engine.json').write_text(json.dumps({'ns3Version':'3.49','rule':'default <=',
        'installation':str(NS3),'modelOrigin':'ns-3.47 default, only model include directives adapted',
        'library':str(NS3/'build/lib/libns3.49-internet.dylib')},indent=2)+'\n')
    return revision

if __name__=='__main__':
    ns347=BASE.parent/'ns-3.47'
    names=['src/internet/model/tcp-qt-'+name+ext for name in ('sdr','tor') for ext in ('.cc','.h')]+['examples/tcp/tcp_qt_sims.cc']
    for name in names:
        actual=(NS3/name).read_text().replace('#include "ns3/double.h"\n#include "ns3/nstime.h"\n#include "ns3/simulator.h"','#include "ns3/core-module.h"')
        assert actual==(ns347/name).read_text(),'Port differs from ns-3.47: '+name
    os.environ['DYLD_LIBRARY_PATH']=str(NS3/'build/lib')
    runner.snapshot=snapshot
    sys.argv[1:1]=['--results',str(BASE/'trace-results/ns-3.49/default-original-timing'),
        '--graphs',str(BASE/'graphs/ns-3.49/default-original-timing'),'--jobs','4']
    sys.exit(runner.main())
