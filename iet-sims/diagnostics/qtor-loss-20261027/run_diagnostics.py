from pathlib import Path
import subprocess,os,json,concurrent.futures,hashlib
ROOT=Path(__file__).resolve().parents[3];D=Path(__file__).resolve().parent
def run(case):
 label,ver,variant=case
 binary=D/('ns-'+ver)/'driver/tcp_qt_diagnostic';out=D/'results'/label
 lib=(D/('ns-'+ver)/variant/'lib') if variant else ROOT/('ns-'+ver)/'build/lib'
 env=dict(os.environ,DYLD_LIBRARY_PATH=str(lib),DYLD_PRINT_LIBRARIES='1')
 cmd=[str(binary),'--scenario=loss','--transport_prot=TcpQtTor','--error_p=0.01','--sim_duration=100','--rngSeed=12345','--rngRun=1','--sim_name='+str(out)]
 out.mkdir(parents=True,exist_ok=True)
 with (out/'run.log').open('w') as f:p=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
 assert p.returncode==0,(label,p.returncode)
 paths=list(out.glob('loss/TcpQtTor/*/totals.txt'));assert len(paths)==1
 values=dict(x.split('=',1) for x in paths[0].read_text().splitlines() if '=' in x)
 text=(out/'run.log').read_text();expected=str(lib/('libns'+ver+'-internet.dylib'));assert expected in text,'Wrong loaded library '+label
 row={'case':label,'version':ver,'library':expected,'librarySha256':hashlib.sha256(Path(expected).read_bytes()).hexdigest(),'directory':str(paths[0].parent),'command':cmd,**values}
 (out/'run.json').write_text(json.dumps(row,indent=2)+'\n')
 print(label,values['aggregateMeanGoodputMbps'],flush=True);return row
cases=[('ns347-default','3.47',None),('ns349-default','3.49',None),('ns349-no-retrans-loss-detection','3.49','no-retrans-loss-detection'),('ns349-no-sacked-retrans-clearing','3.49','no-sacked-retrans-clearing'),('ns349-both-sack-fixes-off','3.49','both-sack-fixes-off')]
rows=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 for row in pool.map(run,cases):rows.append(row)
(D/'experiment-index.json').write_text(json.dumps(rows,indent=2)+'\n')
for path,h in json.loads((D/'default-hashes-before.json').read_text()).items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==h,path
print('Default sources and engines unchanged.',flush=True)
