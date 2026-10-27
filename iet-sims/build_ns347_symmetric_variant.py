#!/usr/bin/env python3
"""Build an isolated internet library with symmetric QtSDR stall correction."""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess
BASE=Path(__file__).resolve().parent
NS3=BASE.parent/'ns-3.47'
VARIANT=BASE/'variants/ns-3.47/qtsdr-symmetric-stall'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    for name in ('source','build','lib'): (VARIANT/name).mkdir(parents=True,exist_ok=True)
    active=NS3/'src/internet/model/tcp-qt-sdr.cc'
    default=NS3/'qt-window-rule-backup/tcp-qt-sdr.default-original-le.cc'
    if active.read_bytes()!=default.read_bytes():
        raise RuntimeError('Active QtSDR source differs from the recorded default; inspect before rebuilding the isolated variant.')
    source=default.read_text()
    old='''    if (W_new == currentWindow && R <= R_target)
    {
        ++W_new;
    }'''
    new='''    if (W_new == currentWindow)
    {
        if (R <= R_target)
        {
            ++W_new;
        }
        else
        {
            --W_new;
        }
    }'''
    assert old in source
    cpp=VARIANT/'source/tcp-qt-sdr.cc';cpp.write_text(source.replace(old,new,1))
    shutil.copy2(NS3/'src/internet/model/tcp-qt-sdr.h',VARIANT/'source/tcp-qt-sdr.h')
    shutil.copy2(default,VARIANT/'source/tcp-qt-sdr.default.cc')
    original_lib=NS3/'build/lib/libns3.47-internet.dylib'
    initial_lib_hash=sha(original_lib)
    commands=json.loads((NS3/'cmake-cache/compile_commands.json').read_text())
    spec=next(c for c in commands if c['file'].endswith('/tcp-qt-sdr.cc'))
    command=shlex.split(spec['command'])
    command=[('-DPROJECT_SOURCE_PATH="'+str(NS3)+'"') if arg.startswith('-DPROJECT_SOURCE_PATH=') else arg for arg in command]
    command[command.index('-c')+1]=str(cpp)
    command[command.index('-o')+1]=str(VARIANT/'build/tcp-qt-sdr.cc.o')
    with (VARIANT/'build/compile.log').open('w') as log:
        subprocess.run(command,cwd=spec['directory'],stdout=log,stderr=subprocess.STDOUT,check=True)
    ninja=BASE.parent/'tools/bin/ninja'
    output=subprocess.check_output([str(ninja),'-C',str(NS3/'cmake-cache'),'-t','commands',str(original_lib)],text=True)
    tokens=shlex.split(output.splitlines()[-1]);assert tokens[:2]==[':','&&'] and tokens[-2:]==['&&',':']
    link=tokens[2:-2]
    link[link.index('-o')+1]=str(VARIANT/'lib/libns3.47-internet.dylib')
    obj='src/internet/CMakeFiles/internet.dir/model/tcp-qt-sdr.cc.o'
    assert link.count(obj)==1
    link[link.index(obj)]=str(VARIANT/'build/tcp-qt-sdr.cc.o')
    with (VARIANT/'build/link.log').open('w') as log:
        subprocess.run(link,cwd=NS3/'cmake-cache',stdout=log,stderr=subprocess.STDOUT,check=True)
    assert sha(original_lib)==initial_lib_hash
    manifest={'ns3Version':'3.47','variant':'symmetric-stall-correction','sourceSha256':sha(cpp),
        'defaultSourceSha256':sha(active),'defaultInternetLibrarySha256':initial_lib_hash,
        'variantInternetLibrarySha256':sha(VARIANT/'lib/libns3.47-internet.dylib'),
        'driverSha256':sha(NS3/'build/examples/tcp/ns3.47-tcp_qt_sims'),
        'compileArguments':command,'linkArguments':link,'compileDirectory':spec['directory'],
        'note':'Only the QtSDR object is replaced; installed default library remains unchanged.'}
    (VARIANT/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Built isolated variant:',VARIANT,flush=True)

if __name__=='__main__':main()
