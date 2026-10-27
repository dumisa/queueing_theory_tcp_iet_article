#!/usr/bin/env python3
"""Run matched ns-3.47 baseline/proposed QtSDR static cases in isolated folders."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
import run_iet_sims as runner
BASE=Path(__file__).resolve().parent
NS3=BASE.parent/'ns-3.47'
STUDY=BASE/'trace-results/ns-3.47/qt-sdr-window-rule-study'
BACKUP=NS3/'qt-window-rule-backup'
runner.NS3=NS3

def snapshot(root):
    names=['VERSION','examples/tcp/tcp_qt_sims.cc','src/internet/CMakeLists.txt','examples/tcp/CMakeLists.txt']
    names += ['src/internet/model/tcp-qt-'+name+ext for name in ('sdr','tor') for ext in ('.cc','.h')]
    names += [str(p.relative_to(NS3)) for p in (NS3/'build/examples/tcp').glob('ns3.*-tcp_qt_sims')]
    names += ['build/lib/libns3.47-internet.dylib']
    sha={name:hashlib.sha256((NS3/name).read_bytes()).hexdigest() for name in names}
    revision=hashlib.sha256(json.dumps(sha,sort_keys=True).encode()).hexdigest()
    dest=root/'_provenance'/revision[:16]
    dest.mkdir(parents=True,exist_ok=True)
    for name in names:
        target=dest/name; target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():shutil.copy2(NS3/name,target)
    (dest/'sha256.json').write_text(json.dumps(sha,indent=2)+'\n')
    (dest/'engine.json').write_text(json.dumps({'version':'3.47','variant':root.name,'installation':str(NS3)},indent=2)+'\n')
    return revision

def stage(variant,algorithms):
    target=NS3/'src/internet/model/tcp-qt-sdr.cc'
    shutil.copy2(BACKUP/('tcp-qt-sdr.'+variant+'.cc'),target)
    target.touch()
    env=dict(os.environ,PATH=str(BASE.parent/'tools/bin')+os.pathsep+os.environ['PATH'],CCACHE_DIR=str(NS3/'.ccache'))
    with (STUDY/(variant+'-build.log')).open('w') as log:
        subprocess.run([str(BASE.parent/'tools/bin/python3'),str(NS3/'ns3'),'build','tcp_qt_sims','-j','8'],cwd=NS3,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    print('Built '+variant,flush=True)
    runner.snapshot=snapshot
    sys.argv=['run_iet_sims.py','--scenarios','static','--duration','100','--seed','12345','--run','1','--jobs','2',
              '--results',str(STUDY/variant),'--algorithms']+algorithms
    if runner.main():raise RuntimeError('Matrix failed: '+variant)
    print('Finished '+variant,flush=True)

if __name__=='__main__':
    STUDY.mkdir(parents=True,exist_ok=True)
    target=NS3/'src/internet/model/tcp-qt-sdr.cc'
    active_source=target.read_bytes()
    try:
        stage('baseline-recorded',runner.ALGORITHMS)
        stage('baseline-equality',['TcpQtSdr'])
        stage('proposed',runner.ALGORITHMS)
    finally:
        target.write_bytes(active_source)
        env=dict(os.environ,PATH=str(BASE.parent/'tools/bin')+os.pathsep+os.environ['PATH'],CCACHE_DIR=str(NS3/'.ccache'))
        with (STUDY/'restore-active-default-build.log').open('w') as log:
            subprocess.run([str(BASE.parent/'tools/bin/python3'),str(NS3/'ns3'),'build','tcp_qt_sims','-j','8'],cwd=NS3,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        print('Restored and rebuilt the controller that was active before the study.',flush=True)
