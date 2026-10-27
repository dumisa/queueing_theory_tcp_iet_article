#!/usr/bin/env python3
"""Compare the default <= and isolated symmetric QtSDR static experiments."""
from pathlib import Path
import csv,hashlib,json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
BASE=Path(__file__).resolve().parent
ROOT=BASE/'trace-results/ns-3.47/qtsdr-symmetric-stall-study'
OUTPUT=BASE/'graphs/ns-3.47/qtsdr-symmetric-stall-study/comparison'
VARIANTS=['default','symmetric'];LABELS=['Default <= rule','Symmetric stall correction'];COLORS=['#4477aa','#cc6677']

def load(variant,algorithm='TcpQtSdr'):
    index=json.loads((ROOT/variant/'experiment-index.json').read_text())
    rows=[r for r in index['runs'] if Path(r['directory']).parent.name==algorithm]
    assert len(rows)==1 and not index['failures']
    directory=Path(rows[0]['directory'])
    config=dict(line.split('=',1) for line in (directory/'resolved-config.txt').read_text().splitlines() if '=' in line)
    aggregate=np.genfromtxt(directory/'aggregate.csv',delimiter=',',names=True)
    totals=dict(line.split('=',1) for line in (directory/'totals.txt').read_text().splitlines() if '=' in line)
    assert len(aggregate)==1001 and aggregate['time_s'][-1]==100
    return directory,config,aggregate,totals

def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    records=[load(v) for v in VARIANTS]
    assert records[0][1]==records[1][1],'Resolved settings differ'
    metrics=[]
    for variant,(directory,config,a,totals) in zip(VARIANTS,records):
        s=(a['time_s']>=1)&(a['time_s']<60)
        metrics.append(dict(variant=variant,mean_goodput_mbps=float(totals['aggregateMeanGoodputMbps']),
            median_rtt_ms=float(np.nanmedian(a['mean_rtt_ms'])),mean_rtt_ms=float(np.nanmean(a['mean_rtt_ms'])),
            median_power=float(np.nanmedian(a['power_norm'])),mean_power=float(np.nanmean(a['power_norm'])),
            jain_mean_goodput=float(totals['jainMeanGoodput']),median_inflight_packets=float(np.median(a['inflight_packets'])),
            max_inflight_1_to_60s=float(np.max(a['inflight_packets'][s])),samples_above500_1_to_60s=int(np.sum(a['inflight_packets'][s]>500)),
            received_payload_bytes=int(totals['receivedPayloadBytes'])))
    with (OUTPUT/'qtsdr-metrics.csv').open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(metrics[0]));writer.writeheader();writer.writerows(metrics)
    def save(fig,name):
        fig.text(.01,.01,'ns-3.47 | 5 QtSDR flows | 100 s | 100 Mbps | seed 12345/run 1\nStart: 10 microseconds | samples: 0.001 + 0.1*k s, plus final sample at 100 s',fontsize=8)
        fig.tight_layout(rect=(0,.055,1,1));fig.savefig(OUTPUT/name,dpi=300);plt.close(fig)
    fig,axes=plt.subplots(2,1,figsize=(10,7),sharex=True)
    for record,label,color in zip(records,LABELS,COLORS):
        a=record[2];s=a['time_s']<=60
        axes[0].plot(a['time_s'][s],a['inflight_packets'][s],color=color,label=label,linewidth=1.1)
        axes[1].plot(a['time_s'][s],a['mean_rtt_ms'][s],color=color,linewidth=1.1)
    axes[0].axhline(float(records[0][1]['baseBdpPackets']),color='#222222',linestyle='--',linewidth=.8,label='Base BDP')
    axes[0].set(title='QtSDR: default versus symmetric stall correction',ylabel='Aggregate in-flight (packets)');axes[0].legend(fontsize=8,ncol=3)
    axes[1].set(xlabel='Simulation time (s)',ylabel='Mean flow RTT (ms)',xlim=(0,60))
    for ax in axes:ax.grid(alpha=.2)
    save(fig,'qtsdr-inflight-rtt.png')
    fig,axes=plt.subplots(1,2,figsize=(10,4.8))
    for ax,metric,title,ylabel in [(axes[0],'mean_rtt_ms','RTT distribution','Mean flow RTT (ms)'),(axes[1],'power_norm','Power distribution','Normalized interval-averaged Power')]:
        values=[r[2][metric][np.isfinite(r[2][metric])] for r in records]
        box=ax.boxplot(values,labels=['Default <=','Symmetric'],patch_artist=True,flierprops={'marker':'.','markersize':2,'alpha':.35})
        for patch,color in zip(box['boxes'],COLORS):patch.set_facecolor(color);patch.set_alpha(.6)
        ax.set(title=title,ylabel=ylabel);ax.grid(axis='y',alpha=.2)
    save(fig,'qtsdr-rtt-power-distributions.png')
    fig,axes=plt.subplots(1,2,figsize=(10,4.8))
    for ax,key,title,ylabel in [(axes[0],'mean_goodput_mbps','Receiver goodput','Mean aggregate goodput (Mbps)'),(axes[1],'jain_mean_goodput','Flow fairness',"Jain's index")]:
        vals=[m[key] for m in metrics];ax.bar(['Default <=','Symmetric'],vals,color=COLORS)
        for i,v in enumerate(vals):ax.text(i,v,format(v,'.4f'),ha='center',va='bottom',fontsize=9)
        ax.set(title=title,ylabel=ylabel,ylim=(0,max(vals)*1.12));ax.grid(axis='y',alpha=.2)
    save(fig,'qtsdr-goodput-fairness.png')
    flow_records=[]
    for variant,record in zip(VARIANTS,records):
        for row in csv.DictReader((record[0]/'summary.csv').open()):flow_records.append(dict(variant=variant,**row))
    with (OUTPUT/'per-flow-goodput.csv').open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(flow_records[0]));writer.writeheader();writer.writerows(flow_records)
    fig,ax=plt.subplots(figsize=(9,4.8))
    x=np.arange(5);width=.36
    for offset,variant,label,color in zip([-.5,.5],VARIANTS,LABELS,COLORS):
        values=[float(r['mean_goodput_mbps']) for r in flow_records if r['variant']==variant]
        ax.bar(x+offset*width,values,width=width,label=label,color=color)
    ax.set(xticks=x,xticklabels=['Flow '+str(i) for i in range(5)],ylabel='Mean receiver goodput (Mbps)',title='QtSDR bandwidth sharing across the five flows')
    ax.legend(fontsize=8);ax.grid(axis='y',alpha=.2)
    save(fig,'qtsdr-per-flow-goodput.png')
    unchanged={}
    for algorithm in ['TcpQtTor','TcpNewReno','TcpCubic','TcpBbr']:
        a=load('default',algorithm)[0];b=load('symmetric',algorithm)[0]
        unchanged[algorithm]={name:(a/name).read_bytes()==(b/name).read_bytes() for name in ['aggregate.csv','flows.csv','summary.csv','totals.txt']}
    deltas={key:{'absolute':metrics[1][key]-metrics[0][key],'percent':100*(metrics[1][key]-metrics[0][key])/metrics[0][key]} for key in ['mean_goodput_mbps','mean_rtt_ms','median_rtt_ms','mean_power','median_power','jain_mean_goodput']}
    manifest={'settings':records[0][1],'metrics':metrics,'changes':deltas,'unmodifiedAlgorithmsExactDataMatch':unchanged,
        'sourceRuns':{v:str(r[0]) for v,r in zip(VARIANTS,records)},
        'aggregateSha256':{v:hashlib.sha256((r[0]/'aggregate.csv').read_bytes()).hexdigest() for v,r in zip(VARIANTS,records)},
        'plotScriptSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'notes':['Same driver, timestamp policy, original startup and sampling phase for both variants.',
                 'Separate library load verified per process; installed default library is unchanged.',
                 'Temporal distributions from one matched seed/run, not independent replications.']}
    (OUTPUT/'comparison-index.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__':main()
