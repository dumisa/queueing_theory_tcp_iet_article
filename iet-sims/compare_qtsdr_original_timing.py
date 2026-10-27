#!/usr/bin/env python3
"""Compare matched ns-3.47 static experiments for the QtSDR window rule study."""
import argparse,csv,hashlib,json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
BASE=Path(__file__).resolve().parent
ROOT=BASE/'trace-results/ns-3.47/qt-sdr-window-rule-original-timing'
OUTPUT=BASE/'graphs/ns-3.47/qt-sdr-window-rule-original-timing/comparison'
VARIANTS=['baseline-recorded','baseline-equality','proposed']
LABELS=['Recorded baseline (<=)','Equality baseline (==)','Proposed rule']
COLORS=['#4477aa','#777777','#cc6677']

def load(variant,algorithm='TcpQtSdr'):
    index=json.loads((ROOT/variant/'experiment-index.json').read_text())
    selected=[r for r in index['runs'] if Path(r['directory']).parent.name==algorithm]
    assert len(selected)==1 and not index['failures'],variant
    directory=Path(selected[0]['directory'])
    config=dict(line.split('=',1) for line in (directory/'resolved-config.txt').read_text().splitlines() if '=' in line)
    aggregate=np.genfromtxt(directory/'aggregate.csv',delimiter=',',names=True)
    totals=dict(line.split('=',1) for line in (directory/'totals.txt').read_text().splitlines() if '=' in line)
    assert len(aggregate)==1001 and aggregate['time_s'][-1]==100
    return directory,config,aggregate,totals

def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    records=[load(v) for v in VARIANTS]
    assert all(r[1]==records[0][1] for r in records), 'Resolved settings differ'
    metrics=[]
    for variant,(directory,config,a,totals) in zip(VARIANTS,records):
        selected=(a['time_s']>=1)&(a['time_s']<60)
        metrics.append(dict(variant=variant,mean_goodput_mbps=float(totals['aggregateMeanGoodputMbps']),
            median_rtt_ms=float(np.nanmedian(a['mean_rtt_ms'])),mean_rtt_ms=float(np.nanmean(a['mean_rtt_ms'])),
            median_power=float(np.nanmedian(a['power_norm'])),mean_power=float(np.nanmean(a['power_norm'])),
            jain_mean_goodput=float(totals['jainMeanGoodput']),median_inflight_packets=float(np.median(a['inflight_packets'])),
            max_inflight_1_to_60s=float(np.max(a['inflight_packets'][selected])),
            samples_above500_1_to_60s=int(np.sum(a['inflight_packets'][selected]>500)),
            received_payload_bytes=int(totals['receivedPayloadBytes'])))
    with (OUTPUT/'qtsdr-metrics.csv').open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(metrics[0]));writer.writeheader();writer.writerows(metrics)
    def save(fig,name):
        fig.text(.01,.01,'ns-3.47 | 5 QtSDR flows | 100 s | 100 Mbps | seed 12345/run 1\nStart: 10 microseconds | samples: 0.001 + 0.1*k s, plus final sample at 100 s',fontsize=8)
        fig.tight_layout(rect=(0,.04,1,1));fig.savefig(OUTPUT/name,dpi=300);plt.close(fig)
    fig,axes=plt.subplots(2,1,figsize=(10,7),sharex=True)
    for label,color,record in zip(LABELS,COLORS,records):
        a=record[2];s=a['time_s']<=60
        axes[0].plot(a['time_s'][s],a['inflight_packets'][s],label=label,color=color,linewidth=1.1)
        axes[1].plot(a['time_s'][s],a['mean_rtt_ms'][s],label=label,color=color,linewidth=1.1)
    axes[0].axhline(float(records[0][1]['baseBdpPackets']),linestyle='--',linewidth=.8,color='#222222',label='Base BDP')
    axes[0].set(title='QtSDR window-rule comparison: original timing, first 60 seconds',ylabel='Aggregate in-flight (packets)')
    axes[0].legend(ncol=2,fontsize=8)
    axes[1].set(xlabel='Simulation time (s)',ylabel='Mean flow RTT (ms)',xlim=(0,60))
    for ax in axes:ax.grid(alpha=.2)
    save(fig,'qtsdr-inflight-rtt.png')
    fig,axes=plt.subplots(1,2,figsize=(11,4.7))
    for ax,key,title,label in [(axes[0],'mean_rtt_ms','RTT distribution','Mean flow RTT (ms)'),(axes[1],'power_norm','Power distribution','Normalized interval-averaged Power')]:
        values=[r[2][key][np.isfinite(r[2][key])] for r in records]
        box=ax.boxplot(values,labels=['Recorded\n<=','Equality\n==','Proposed'],patch_artist=True,flierprops={'marker':'.','markersize':2,'alpha':.3})
        for patch,color in zip(box['boxes'],COLORS):patch.set_facecolor(color);patch.set_alpha(.6)
        ax.set(title=title,ylabel=label);ax.grid(axis='y',alpha=.2)
    save(fig,'qtsdr-rtt-power-distributions.png')
    fig,axes=plt.subplots(1,2,figsize=(11,4.7))
    for ax,key,title,ylabel in [(axes[0],'mean_goodput_mbps','Receiver goodput','Mean aggregate goodput (Mbps)'),(axes[1],'jain_mean_goodput','Flow fairness',"Jain's index")]:
        values=[m[key] for m in metrics]
        ax.bar(['Recorded\n<=','Equality\n==','Proposed'],values,color=COLORS)
        for x,value in enumerate(values):ax.text(x,value,format(value,'.4f'),ha='center',va='bottom',fontsize=9)
        ax.set(title=title,ylabel=ylabel,ylim=(0,max(values)*1.12));ax.grid(axis='y',alpha=.2)
    save(fig,'qtsdr-goodput-fairness.png')
    unchanged={}
    for algorithm in ['TcpQtTor','TcpNewReno','TcpCubic','TcpBbr']:
        before=load('baseline-recorded',algorithm)[0];after=load('proposed',algorithm)[0]
        unchanged[algorithm]={name:(before/name).read_bytes()==(after/name).read_bytes() for name in ['aggregate.csv','flows.csv','summary.csv','totals.txt']}
    metric_changes={}
    for baseline in metrics[:2]:
        metric_changes[baseline['variant']]={key:{'absolute':metrics[2][key]-baseline[key],
            'percent':100*(metrics[2][key]-baseline[key])/baseline[key] if baseline[key] else None}
            for key in ['mean_goodput_mbps','median_rtt_ms','median_power','jain_mean_goodput']}
    manifest={'settings':records[0][1],'metrics':metrics,'proposedChangesAgainst':metric_changes,
        'unmodifiedAlgorithmsExactDataMatch':unchanged,
        'sourceRuns':{v:str(r[0]) for v,r in zip(VARIANTS,records)},
        'aggregateSha256':{v:hashlib.sha256((r[0]/'aggregate.csv').read_bytes()).hexdigest() for v,r in zip(VARIANTS,records)},
        'plotScriptSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'notes':['Same setup for every variant; appStart=warmup=0.00001 s; sampling starts at absolute 0.001 s, with a final 100 s sample.',
                 'Recorded baseline already used <=; equality baseline implements the older snippet literally.',
                 'Temporal distributions from one deterministic run; no statistical significance claim.',
                 'Only the proposed implementation remains installed in ns-3.47.']}
    (OUTPUT/'comparison-index.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (OUTPUT/'findings.txt').write_text('QtSDR window-rule static study\n\n'+json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'metrics':metrics,'changes':metric_changes,'unchanged':unchanged},indent=2))

if __name__=='__main__':main()
