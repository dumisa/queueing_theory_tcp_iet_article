from pathlib import Path
import csv,json,hashlib,math,statistics,shutil
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(__file__).resolve().parents[3];D=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return list(csv.DictReader(p.open()))
def cfg(p):return dict(x.split('=',1) for x in p.read_text().splitlines() if '=' in x)
records=json.loads((D/'experiment-index.json').read_text());controls=json.loads((D/'qtsdr-control-index.json').read_text())
paths={r['case']:Path(r['directory']) for r in records+controls};summary=[]
for record in records+controls:
 p=Path(record['directory']);c=cfg(p/'resolved-config.txt');a=read(p/'aggregate.csv');flows=read(p/'flows.csv');rec=read(p/'recovery-summary.csv')
 assert len(a)==1001 and len(flows)==5005 and float(a[-1]['time_s'])==100
 assert c['recovery']=='TcpClassicRecovery' and c['sack']=='true' and float(c['errorRate'])==.01
 assert all(abs(float(r['time_s'])-(.001+.1*i))<1e-8 for i,r in enumerate(a[:-1]))
 assert int(c['flows'])==5 and float(c['appStart'])==.00001 and float(c['warmup'])==.00001 and float(c['samplingPhaseSeconds'])==.001
 assert int(c['seed'])==12345 and int(c['run'])==1
 delivered=sum(int(r['delivered_payload_bytes']) for r in read(p/'summary.csv'));integrated=sum(float(r['goodput_mbps'])*1e6*float(r['interval_s'])/8 for r in a);assert abs(delivered-integrated)<.5
 finite=lambda k:[float(r[k]) for r in a if math.isfinite(float(r[k]))]
 row={'case':record['case'],'algorithm':c['transport'],'goodput_mbps':float(record['aggregateMeanGoodputMbps']),'mean_rtt_ms':statistics.mean(finite('mean_rtt_ms')),'mean_power':statistics.mean(finite('power_norm')),'jain':float(record['jainMeanGoodput']),'rto_loss_entries':sum(int(r['loss_entries']) for r in rec),'retrans_packets':sum(int(r['retrans_packets']) for r in rec),'data_tx_packets':sum(int(r['tx_packets']) for r in rec),'zero_delivery_flow_intervals_after_0_2s':sum(float(r['time_s'])>.2 and float(r['goodput_mbps'])==0 for r in flows)}
 row['median_cwnd_after_1s_packets']=statistics.median(float(x['cwnd_packets']) for x in flows if float(x['time_s'])>1)
 row['mean_cwnd_after_1s_packets']=statistics.mean(float(x['cwnd_packets']) for x in flows if float(x['time_s'])>1)
 summary.append(row)
# Passive diagnostics reproduce the archived defaults exactly.
for record in [r for r in records+controls if r['case'] in ['ns347-default','ns349-default','ns347-qtsdr-control','ns349-qtsdr-control']]:
 p=Path(record['directory']);version=record['version'];alg=cfg(p/'resolved-config.txt')['transport']
 old=ROOT/'iet-sims/trace-results'/('ns-'+version)/'workflow-verification-20261027/loss'/alg/p.name
 assert sha(old/'aggregate.csv')==sha(p/'aggregate.csv')
checks={}
for left,right in [('ns347-default','ns349-both-sack-fixes-off'),('ns349-default','ns347-backport-both-sack-fixes')]:
 for name in ['aggregate.csv','flows.csv','recovery-events.csv','recovery-summary.csv','summary.csv','totals.txt']:
  equal=(paths[left]/name).read_bytes()==(paths[right]/name).read_bytes();assert equal,(left,right,name);checks[left+' == '+right+': '+name]=equal
before=json.loads((D/'default-hashes-before.json').read_text());after={name:sha(ROOT/name) for name in before};assert before==after
(D/'default-hashes-after.json').write_text(json.dumps(after,indent=2)+'\n')
with (D/'diagnostic-summary.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
(D/'validation.json').write_text(json.dumps({'result':'PASS','qtor100SecondCases':len(records),'qtsdrControl100SecondCases':len(controls),'dropConfirmationSeconds':3,'passiveDriversMatchArchivedDefaults':True,'exactCounterfactualComparisons':checks,'defaultSourceAndEngineHashesUnchanged':True,'protectedFiles':len(before),'metrics':summary},indent=2)+'\n')
# Summary of the six counterfactual QtTOR cases.
labels={'ns347-default':'3.47 default','ns349-default':'3.49 default','ns349-no-retrans-loss-detection':'3.49: detection off','ns349-no-sacked-retrans-clearing':'3.49: accounting fix off','ns349-both-sack-fixes-off':'3.49: both fixes off','ns347-backport-both-sack-fixes':'3.47: both fixes backported'}
rows=summary[:6];y=np.arange(len(rows));colors=['#D55E00','#0072B2','#E69F00','#56B4E9','#D55E00','#0072B2'];fig,axes=plt.subplots(1,2,figsize=(13,5.5));fig.subplots_adjust(left=.27,right=.96,bottom=.2,top=.8,wspace=.35)
for ax,key,title in [(axes[0],'goodput_mbps','Mean aggregate goodput (Mbps)'),(axes[1],'rto_loss_entries','Timeout-loss entries across five flows')]:
 values=[r[key] for r in rows];ax.barh(y,values,color=colors);ax.invert_yaxis();ax.set_yticks(y);ax.set_yticklabels([labels[r['case']] for r in rows] if ax==axes[0] else []);ax.set_title(title,fontsize=11);ax.set_xlim(0,max(values)*1.23);ax.grid(axis='x',alpha=.2);ax.set_axisbelow(True)
 for i,v in enumerate(values):ax.text(v+max(values)*.02,i,format(v,'.3f') if key=='goodput_mbps' else str(v),va='center',fontsize=10)
fig.suptitle('QtTOR at 1% packet loss: isolated TCP-stack counterfactuals',fontsize=15);fig.text(.27,.08,'100 s | five QtTOR flows | seed 12345 / run 1 | default Qt algorithms unchanged\nOnly the isolated transmit-buffer object differs; no PRR substitution.',fontsize=10);fig.savefig(D/'stack-ablation-goodput-timeouts.png',dpi=300);plt.close(fig)
# Exact event timeline for the first twice-lost segment.
fig,ax=plt.subplots(figsize=(11,4.3));fig.subplots_adjust(left=.16,right=.97,bottom=.22,top=.8)
for i,(v,label) in enumerate([('347','ns-3.47'),('349','ns-3.49')]):
 target=json.loads((D/'results'/('packet-drop-confirmation-ns'+v)/'target-sequence.json').read_text())
 ax.hlines(i,1.64,2.79,color='#CCCCCC')
 for e in target:
  if e['event']=='retransmission':continue
  t=float(e['time_s']);drop=e['event']=='packet_drop';ax.scatter(t,i,marker='x' if drop else 'o',color='#D55E00' if drop else '#0072B2',s=65,zorder=3)
 ax.text(1.655,i+.14,'original TX',fontsize=8);ax.text(1.696,i-.25,'1st retransmission',fontsize=8)
 retry=max(float(e['time_s']) for e in target if e['event']=='target_sequence_tx');ax.annotate('next retry: '+format(retry,'.6f')+' s',xy=(retry,i),xytext=(retry,i+.3),ha='center',fontsize=9,arrowprops={'arrowstyle':'->','color':'#555555'})
ax.set_yticks([0,1]);ax.set_yticklabels(['ns-3.47','ns-3.49']);ax.set_xlim(1.63,2.82);ax.set_ylim(-.4,1.55);ax.set_xlabel('Simulation time (seconds)');ax.grid(axis='x',alpha=.2);fig.suptitle('First causal divergence: flow 1, TCP sequence 1243833',fontsize=14);fig.text(.16,.04,'Blue circles: transmissions; orange crosses: confirmed forward-link packet drops.\nBoth the original packet and first retransmission are dropped. The later retry is about 0.998 s earlier in ns-3.49.',fontsize=9);fig.savefig(D/'first-lost-retransmission-timeline.png',dpi=300);plt.close(fig)
print(json.dumps(summary,indent=2))
print('PASS: controlled exact-match checks, defaults unchanged, receiver-byte accounting; two diagnostic PNGs generated.')
