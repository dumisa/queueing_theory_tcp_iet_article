#!/usr/bin/env python3
"""Validate and summarize the separate ns-3.47 non-static experiment matrix."""
from pathlib import Path
import csv,hashlib,json,math,statistics
BASE=Path(__file__).resolve().parent
ROOT=BASE/'trace-results/ns-3.47/buffer-loss-stochastic-default-original-timing'
GRAPHS=BASE/'graphs/ns-3.47/buffer-loss-stochastic-default-original-timing'
NS3=BASE.parent/'ns-3.47'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    index=json.loads((ROOT/'experiment-index.json').read_text())
    assert len(index['runs'])==45 and not index['failures']
    summary=[];counts={};capacity={};keys=set()
    for record in index['runs']:
        p=Path(record['directory'])
        config=dict(line.split('=',1) for line in (p/'resolved-config.txt').read_text().splitlines() if '=' in line)
        scenario=config['scenario'];algorithm=config['transport']
        assert scenario in ['buffer','loss','stochastic']
        assert config['recovery']=='TcpClassicRecovery'
        assert int(config['flows'])==5 and float(config['duration'])==100
        assert abs(float(config['appStart'])-.00001)<1e-12
        assert abs(float(config['warmup'])-.00001)<1e-12
        assert abs(float(config['samplingPhaseSeconds'])-.001)<1e-12
        assert int(config['seed'])==12345 and int(config['run'])==1
        a=list(csv.DictReader((p/'aggregate.csv').open()))
        f=list(csv.DictReader((p/'flows.csv').open()))
        delivered=list(csv.DictReader((p/'summary.csv').open()))
        assert len(a)==1001 and len(f)==5005 and len(delivered)==5
        assert float(a[-1]['time_s'])==100
        assert all(abs(float(row['time_s'])-(.001+.1*i))<1e-8 for i,row in enumerate(a[:-1]))
        assert all(int(row['delivered_payload_bytes'])>0 for row in delivered)
        total=sum(int(row['delivered_payload_bytes']) for row in delivered)
        sampled=sum(float(row['goodput_mbps'])*1e6*float(row['interval_s'])/8 for row in a)
        assert abs(sampled-total)<.5
        totals=dict(line.split('=',1) for line in (p/'totals.txt').read_text().splitlines() if '=' in line)
        variant=config['queuePackets'] if scenario=='buffer' else config['errorRate'] if scenario=='loss' else config['regime']
        key=(scenario,variant,algorithm);assert key not in keys;keys.add(key)
        counts[scenario]=counts.get(scenario,0)+1
        rtts=[float(row['mean_rtt_ms']) for row in a if math.isfinite(float(row['mean_rtt_ms']))]
        powers=[float(row['power_norm']) for row in a if math.isfinite(float(row['power_norm']))]
        assert rtts and powers
        summary.append(dict(scenario=scenario,variant=variant,algorithm=algorithm,
            mean_goodput_mbps=float(totals['aggregateMeanGoodputMbps']),jain_mean_goodput=float(totals['jainMeanGoodput']),
            median_mean_rtt_ms=statistics.median(rtts),mean_mean_rtt_ms=statistics.mean(rtts),
            median_power=statistics.median(powers),mean_power=statistics.mean(powers),
            valid_rtt_samples=len(rtts),capacity_mean_mbps=float(totals['realisedCapacityMeanBps'])/1e6,
            capacity_scv=float(totals['realisedCapacityScv']),directory=str(p)))
        if scenario=='stochastic':capacity.setdefault(variant,{})[algorithm]=sha(p/'capacity.csv')
    assert counts=={'buffer':15,'loss':15,'stochastic':15},counts
    assert len(capacity)==3 and all(len(v)==5 and len(set(v.values()))==1 for v in capacity.values())
    engine=json.loads((ROOT/'engine-check.json').read_text())
    assert sha(NS3/'src/internet/model/tcp-qt-sdr.cc')==engine['sourceSha256']
    assert sha(NS3/'examples/tcp/tcp_qt_sims.cc')==engine['driverSha256']
    assert sha(NS3/'build/lib/libns3.47-internet.dylib')==engine['librarySha256']
    GRAPHS.mkdir(parents=True,exist_ok=True)
    with (GRAPHS/'experiment-summary.csv').open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(summary[0]));writer.writeheader();writer.writerows(sorted(summary,key=lambda row:(row['scenario'],row['variant'],row['algorithm'])))
    validation={'ns3Version':'3.47','rule':'default <=','counts':counts,'runs':45,'aggregateSamplesPerRun':1001,
        'flowSamplesPerRun':5005,'samplePhaseSeconds':.001,'appStartSeconds':.00001,'seed':12345,'run':1,
        'capacitySha256ByRegime':capacity,'engineUnchanged':True,'sourceRevision':index['sourceRevision']}
    (ROOT/'validation.json').write_text(json.dumps(validation,indent=2)+'\n')
    print(json.dumps(validation,indent=2))

if __name__=='__main__':main()
