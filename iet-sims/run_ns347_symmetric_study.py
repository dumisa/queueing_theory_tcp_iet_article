#!/usr/bin/env python3
"""Compare default and isolated symmetric QtSDR without replacing the default library."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
import run_iet_sims as runner
BASE=Path(__file__).resolve().parent
NS3=BASE.parent/'ns-3.47'
VARIANT=BASE/'variants/ns-3.47/qtsdr-symmetric-stall'
STUDY=BASE/'trace-results/ns-3.47/qtsdr-symmetric-stall-study'
runner.NS3=NS3

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def snapshot(root):
    names=['VERSION','examples/tcp/tcp_qt_sims.cc','src/internet/CMakeLists.txt','examples/tcp/CMakeLists.txt']
    names += ['src/internet/model/tcp-qt-'+name+ext for name in ('sdr','tor') for ext in ('.cc','.h')]
    names += ['build/examples/tcp/ns3.47-tcp_qt_sims','build/lib/libns3.47-internet.dylib']
    paths={name:NS3/name for name in names}
    if root.name=='symmetric':
        paths['src/internet/model/tcp-qt-sdr.cc']=VARIANT/'source/tcp-qt-sdr.cc'
        paths['build/lib/libns3.47-internet.dylib']=VARIANT/'lib/libns3.47-internet.dylib'
    digests={name:sha(path) for name,path in paths.items()}
    revision=hashlib.sha256(json.dumps(digests,sort_keys=True).encode()).hexdigest()
    destination=root/'_provenance'/revision[:16];destination.mkdir(parents=True,exist_ok=True)
    for name,path in paths.items():
        target=destination/name;target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():shutil.copy2(path,target)
    (destination/'sha256.json').write_text(json.dumps(digests,indent=2)+'\n')
    (destination/'engine.json').write_text(json.dumps({'version':'3.47','variant':root.name,
        'installation':str(NS3),'loadedInternetLibrary':str(paths['build/lib/libns3.47-internet.dylib']),
        'dyldLibraryPath':os.environ.get('DYLD_LIBRARY_PATH')},indent=2)+'\n')
    return revision

def main():
    STUDY.mkdir(parents=True,exist_ok=True)
    active=NS3/'src/internet/model/tcp-qt-sdr.cc';library=NS3/'build/lib/libns3.47-internet.dylib'
    original={'sourceSha256':sha(active),'librarySha256':sha(library)}
    manifest=json.loads((VARIANT/'build-manifest.json').read_text())
    assert original['sourceSha256']==manifest['defaultSourceSha256']
    assert original['librarySha256']==manifest['defaultInternetLibrarySha256']
    binary=NS3/'build/examples/tcp/ns3.47-tcp_qt_sims'
    assert sha(binary)==manifest['driverSha256']
    for name in ('default','symmetric'):
        lib=library if name=='default' else VARIANT/'lib/libns3.47-internet.dylib'
        os.environ['DYLD_LIBRARY_PATH']=str(lib.parent)+os.pathsep+str(NS3/'build/lib')
        env=dict(os.environ,DYLD_PRINT_LIBRARIES='1')
        check=subprocess.run([str(binary),'--dryRun=1'],capture_output=True,text=True,env=env,check=True)
        (STUDY/(name+'-loader-check.log')).write_text(check.stderr)
        assert str(lib) in check.stderr,'Expected library was not loaded: '+str(lib)
        print('Verified loaded library for '+name+': '+str(lib),flush=True)
        if name=='symmetric':
            with (STUDY/'symmetric-tcp-tests.log').open('w') as log:
                subprocess.run([str(NS3/'build/utils/ns3.47-test-runner'),'--suite=tcp'],env=os.environ,stdout=log,stderr=subprocess.STDOUT,check=True)
        runner.snapshot=snapshot
        sys.argv=['run_iet_sims.py','--scenarios','static','--duration','100','--seed','12345','--run','1','--jobs','2',
                  '--results',str(STUDY/name)]
        if runner.main():raise RuntimeError('Failed matrix: '+name)
    assert sha(active)==original['sourceSha256'] and sha(library)==original['librarySha256']
    (STUDY/'default-unchanged.json').write_text(json.dumps(original,indent=2)+'\n')
    print('Both matrices complete; installed default source and library unchanged.',flush=True)
    return 0

if __name__=='__main__':sys.exit(main())
