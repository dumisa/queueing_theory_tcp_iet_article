#!/usr/bin/env python3
"""Validate the ns-3.49 matrix and pair its results with the existing ns-3.47 runs."""
from pathlib import Path
import csv,hashlib,json,math,statistics
BASE=Path(__file__).resolve().parent
ROOT=BASE/'trace-results/ns-3.49/default-original-timing'
GRAPHS=BASE/'graphs/ns-3.49/default-original-timing'
NS3=BASE.parent/'ns-3.49'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def config(path):return dict(line.split('=',1) for line in path.read_text().splitlines() if '=' in line)
def case_key(c):
    scenario=c['scenario']
    variant='reference' if scenario=='static' else c['queuePackets'] if scenario=='buffer' else format(float(c['errorRate']),'.9g') if scenario=='loss' else c['regime']
    return scenario,variant,c['transport']
def metrics(path,c):
    a=list(csv.DictReader((path/'aggregate.csv').open()));totals=config(path/'totals.txt')
    rtts=[float(r['mean_rtt_ms']) for r in a if math.isfinite(float(r['mean_rtt_ms']))]
    powers=[float(r['power_norm']) for r in a if math.isfinite(float(r['power_norm']))]
    assert rtts and powers
    return dict(scenario=case_key(c)[0],variant=case_key(c)[1],algorithm=c['transport'],
        mean_goodput_mbps=float(totals['aggregateMeanGoodputMbps']),jain_mean_goodput=float(totals['jainMeanGoodput']),
        median_rtt_ms=statistics.median(rtts),mean_rtt_ms=statistics.mean(rtts),
        median_power=statistics.median(powers),mean_power=statistics.mean(powers),directory=str(path))
def write_csv(path,rows):
    with path.open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)

def main():
    index=json.loads((ROOT/'experiment-index.json').read_text());assert len(index['runs'])==50 and not index['failures']
    rows=[];counts={};seen=set();capacity={}
    for record in index['runs']:
        path=Path(record['directory']);c=config(path/'resolved-config.txt');key=case_key(c)
        assert key not in seen;seen.add(key);counts[c['scenario']]=counts.get(c['scenario'],0)+1
        assert int(c['flows'])==5 and float(c['duration'])==100 and c['recovery']=='TcpClassicRecovery'
        for name,value in [('appStart',.00001),('warmup',.00001),('samplingPhaseSeconds',.001)]:assert abs(float(c[name])-value)<1e-12
        assert int(c['seed'])==12345 and int(c['run'])==1
        a=list(csv.DictReader((path/'aggregate.csv').open()));f=list(csv.DictReader((path/'flows.csv').open()));s=list(csv.DictReader((path/'summary.csv').open()))
        assert len(a)==1001 and len(f)==5005 and len(s)==5
        assert float(a[-1]['time_s'])==100
        assert all(abs(float(r['time_s'])-(.001+.1*i))<1e-8 for i,r in enumerate(a[:-1]))
        assert all(int(r['delivered_payload_bytes'])>0 for r in s)
        delivered=sum(int(r['delivered_payload_bytes']) for r in s)
        sampled=sum(float(r['goodput_mbps'])*1e6*float(r['interval_s'])/8 for r in a)
        assert abs(sampled-delivered)<.5
        rows.append(metrics(path,c))
        if c['scenario']=='stochastic':capacity.setdefault(c['regime'],{})[c['transport']]=sha(path/'capacity.csv')
    assert counts=={'static':5,'buffer':15,'loss':15,'stochastic':15}
    assert len(capacity)==3 and all(len(group)==5 and len(set(group.values()))==1 for group in capacity.values())
    previous={}
    for oldroot in ['default-original-timing','buffer-loss-stochastic-default-original-timing']:
        for file in (BASE/'trace-results/ns-3.47'/oldroot).rglob('resolved-config.txt'):
            c=config(file);previous[case_key(c)]=(file.parent,c)
    assert len(previous)==50
    comparisons=[]
    for row in rows:
        old,c=previous[(row['scenario'],row['variant'],row['algorithm'])];prior=metrics(old,c);current=Path(row['directory'])
        r={k:row[k] for k in ['scenario','variant','algorithm']}
        for metric in ['mean_goodput_mbps','jain_mean_goodput','median_rtt_ms','mean_rtt_ms','median_power','mean_power']:
            r['ns347_'+metric]=prior[metric];r['ns349_'+metric]=row[metric];r['delta_'+metric]=row[metric]-prior[metric]
        r['aggregate_csv_identical']=sha(old/'aggregate.csv')==sha(current/'aggregate.csv')
        r['totals_identical']=(old/'totals.txt').read_bytes()==(current/'totals.txt').read_bytes()
        r['capacity_csv_matches_ns347']=sha(old/'capacity.csv')==sha(current/'capacity.csv') if row['scenario']=='stochastic' else ''
        comparisons.append(r)
    engine=json.loads((ROOT/'engine-check.json').read_text())
    for name,key in [('src/internet/model/tcp-qt-sdr.cc','sourceSha256'),('examples/tcp/tcp_qt_sims.cc','driverSha256'),('build/examples/tcp/ns3.49-tcp_qt_sims','binarySha256'),('build/lib/libns3.49-internet.dylib','librarySha256')]:assert sha(NS3/name)==engine[key]
    GRAPHS.mkdir(parents=True,exist_ok=True)
    write_csv(GRAPHS/'experiment-summary.csv',sorted(rows,key=lambda r:(r['scenario'],r['variant'],r['algorithm'])))
    write_csv(GRAPHS/'ns347-vs-ns349.csv',sorted(comparisons,key=lambda r:(r['scenario'],r['variant'],r['algorithm'])))
    validation={'version':'3.49','counts':counts,'runs':50,'aggregateSamplesPerRun':1001,'flowSamplesPerRun':5005,
        'capacitySha256ByRegime':capacity,'matchedNs347Cases':50,
        'identicalAggregateCsvCases':sum(r['aggregate_csv_identical'] for r in comparisons),
        'identicalTotalsCases':sum(r['totals_identical'] for r in comparisons),
        'stochasticCapacityPathsMatchingNs347':sum(r['capacity_csv_matches_ns347'] is True for r in comparisons),'engineUnchanged':True}
    (ROOT/'validation.json').write_text(json.dumps(validation,indent=2)+'\n');print(json.dumps(validation,indent=2))

if __name__=='__main__':main()
