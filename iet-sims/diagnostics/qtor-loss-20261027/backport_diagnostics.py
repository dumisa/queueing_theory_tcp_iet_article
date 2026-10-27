from pathlib import Path
import json,shlex,subprocess,shutil,hashlib
prefix=Path(__file__).with_name('build_diagnostics.py').read_text().split('protected={}')[0];exec(prefix)
ns=ROOT/'ns-3.47';s47=(ns/'src/internet/model/tcp-tx-buffer.cc').read_text();s49=(ROOT/'ns-3.49/src/internet/model/tcp-tx-buffer.cc').read_text()
a='uint32_t\nTcpTxBuffer::Update(';b='\nvoid\nTcpTxBuffer::UpdateLostCount()'
update=s49[s49.index(a):s49.index(b)]
start=s49.index('TcpTxBuffer::MarkRetransmittedSegmentsLost(');start=s49.index('{',start)+1;end=s49.index('\n}\n\nbool\nTcpTxBuffer::IsLost',start)
body=s49[start:end]
update=update.replace('        MarkRetransmittedSegmentsLost(newestSackedSentTime);','        {\n            const Time& sackedSentTime = newestSackedSentTime;\n'+body+'\n        }')
s=s47[:s47.index(a)]+update+s47[s47.index(b):]
folder=D/'ns-3.47/backport-both-sack-fixes';folder.mkdir(parents=True,exist_ok=True);(folder/'lib').mkdir(exist_ok=True)
cpp=folder/'tcp-tx-buffer.cc';cpp.write_text(s);shutil.copy2(ns/'src/internet/model/tcp-tx-buffer.h',folder/'tcp-tx-buffer.h')
obj=folder/'tcp-tx-buffer.o';compile_obj(ns,'tcp-tx-buffer.cc',cpp,obj)
lib=ns/'build/lib/libns3.47-internet.dylib';cmd=tokens_for(ns,lib);cmd[cmd.index('-o')+1]=str(folder/'lib/libns3.47-internet.dylib');old='src/internet/CMakeFiles/internet.dir/model/tcp-tx-buffer.cc.o';cmd[cmd.index(old)]=str(obj)
subprocess.run(cmd,cwd=ns/'cmake-cache',check=True,capture_output=True)
(folder/'build-manifest.json').write_text(json.dumps({'sourceSha256':sha(cpp),'librarySha256':sha(folder/'lib/libns3.47-internet.dylib'),'note':'Only ns-3.49 SACK Update() fixes backported to an isolated ns-3.47 library; helper inlined to preserve the installed header and ABI.'},indent=2)+'\n')
code=Path(__file__).with_name('run_diagnostics.py').read_text().split('cases=[')[0];exec(code)
r=run(('ns347-backport-both-sack-fixes','3.47','backport-both-sack-fixes'))
rows=json.loads((D/'experiment-index.json').read_text());rows.append(r);(D/'experiment-index.json').write_text(json.dumps(rows,indent=2)+'\n')
for name,h in json.loads((D/'default-hashes-before.json').read_text()).items():assert sha(ROOT/name)==h,name
print('Backport diagnostic completed; defaults unchanged.')
