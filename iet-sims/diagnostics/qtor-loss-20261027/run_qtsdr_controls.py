from pathlib import Path
import concurrent.futures,json
code=Path(__file__).with_name('run_diagnostics.py').read_text().split('cases=[')[0].replace('--transport_prot=TcpQtTor','--transport_prot=TcpQtSdr').replace("glob('loss/TcpQtTor/*/totals.txt')","glob('loss/TcpQtSdr/*/totals.txt')")
exec(code)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 rows=list(pool.map(run,[('ns347-qtsdr-control','3.47',None),('ns349-qtsdr-control','3.49',None)]))
(D/'qtsdr-control-index.json').write_text(json.dumps(rows,indent=2)+'\n')
