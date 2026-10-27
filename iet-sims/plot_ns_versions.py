#!/usr/bin/env python3
"""Plot matched ns-3.47/ns-3.49 case metrics and highlight nonidentical traces."""
import argparse,csv,hashlib,json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

METRICS=[('mean_goodput_mbps','goodput','Mean aggregate goodput','Mbps'),('mean_rtt_ms','rtt','Mean RTT','ms'),('mean_power','power','Mean normalized Power',''),('jain_mean_goodput','fairness','Jain fairness of mean flow goodput','')]
ORDER={'static':0,'buffer':1,'loss':2,'stochastic':3}
ALGS={'TcpNewReno':'NewReno','TcpCubic':'Cubic','TcpBbr':'BBR','TcpQtSdr':'QtSDR','TcpQtTor':'QtTOR'}
def label(r):
 v=r['variant'];s=r['scenario']
 condition=('buffer '+v+' packets') if s=='buffer' else ('loss '+format(float(v)*100,'.4g')+'%') if s=='loss' else ('OU '+v) if s=='stochastic' else 'static'
 return ALGS[r['algorithm']]+' | '+condition

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--metrics',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 rows=list(csv.DictReader(a.metrics.open()));assert len(rows)==50
 assert len({(r['scenario'],r['variant'],r['algorithm']) for r in rows})==50
 changed=sorted([r for r in rows if r['aggregate_csv_identical'].lower()!='true'],key=lambda r:(ORDER[r['scenario']],r['variant'],r['algorithm']))
 assert len(changed)==10
 same=[r for r in rows if r not in changed];assert len(same)==40
 a.output.mkdir(parents=True,exist_ok=True)
 plt.rcParams.update({'font.size':10,'axes.titlesize':12,'axes.labelsize':11,'savefig.facecolor':'white'})
 records=[]
 for metric,name,title,unit in METRICS:
  fig,(left,right)=plt.subplots(1,2,figsize=(15.5,6.7),gridspec_kw={'width_ratios':[1,1.15]})
  fig.subplots_adjust(left=.065,right=.965,top=.77,bottom=.2,wspace=.72)
  allx=np.array([float(r['ns347_'+metric]) for r in rows]);ally=np.array([float(r['ns349_'+metric]) for r in rows])
  delta=np.array([float(r['delta_'+metric]) for r in changed]);assert np.allclose(ally-allx,[float(r['delta_'+metric]) for r in rows],rtol=1e-9,atol=1e-12)
  lo=min(allx.min(),ally.min());hi=max(allx.max(),ally.max());span=hi-lo;pad=max(span*.07,.001);lims=(lo-pad,hi+pad)
  left.plot(lims,lims,color='#888888',linestyle='--',lw=1,label='Equal metric values')
  left.scatter([float(r['ns347_'+metric]) for r in same],[float(r['ns349_'+metric]) for r in same],c='#778899',s=42,alpha=.7,label='40 cases: identical aggregate traces',zorder=3)
  left.scatter([float(r['ns347_'+metric]) for r in changed],[float(r['ns349_'+metric]) for r in changed],c='#D55E00',marker='D',edgecolors='white',linewidth=.5,s=62,label='10 cases: differing aggregate traces',zorder=4)
  suffix=' ('+unit+')' if unit else ''
  left.set(xlim=lims,ylim=lims,xlabel='ns-3.47'+suffix,ylabel='ns-3.49'+suffix,title='All 50 matched experiment cases')
  left.set_aspect('equal',adjustable='box');left.grid(alpha=.18)
  handles,labels=left.get_legend_handles_labels()
  fig.legend(handles,labels,loc='upper center',bbox_to_anchor=(.5,.875),ncol=3,fontsize=9,frameon=False)
  y=np.arange(len(changed));colors=['#0072B2' if v>=0 else '#D55E00' for v in delta]
  right.barh(y,delta,color=colors,height=.6);right.axvline(0,color='#333333',lw=.8)
  right.set_yticks(y);right.set_yticklabels([label(r) for r in changed],fontsize=9);right.invert_yaxis()
  right.set_xlabel('Difference: ns-3.49 minus ns-3.47'+suffix);right.set_title('The ten cases with differing aggregate traces')
  maxd=max(abs(delta));right.set_xlim(min(0,float(delta.min()))-.16*maxd,max(0,float(delta.max()))+.32*maxd)
  for yi,d in zip(y,delta):
   text=('{:+.6g}'.format(d))
   right.annotate(text,xy=(d,yi),xytext=(5,0),textcoords='offset points',ha='left',va='center',fontsize=9)
  right.grid(axis='x',alpha=.18);right.set_axisbelow(True)
  right.xaxis.set_major_locator(MaxNLocator(5))
  fig.suptitle('ns-3.47 versus ns-3.49: '+title,fontsize=17,x=.5,y=.97)
  fig.text(.5,.9,'Default Qt rules and original timing | 100 s | five homogeneous flows per case | seed 12345 / run 1',ha='center',fontsize=10,color='#444444')
  fig.text(.065,.035,'Each point is one case summary; overlapping points are possible. Highlighting is based on exact aggregate.csv comparison.\nDifferences are single-run observations, not confidence intervals; lower RTT is preferable, while higher goodput, Power and fairness are preferable.',fontsize=8.5,color='#444444')
  output=a.output/('ns347-vs-ns349-'+name+'.png');fig.savefig(output,dpi=300);plt.close(fig)
  records.append({'file':output.name,'metric':metric,'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'units':unit or 'dimensionless'})
 (a.output/'graph-index.json').write_text(json.dumps({'metricsSource':str(a.metrics),'metricsSha256':hashlib.sha256(a.metrics.read_bytes()).hexdigest(),'plotScriptSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'matplotlibVersion':matplotlib.__version__,'numpyVersion':np.__version__,'cases':50,'identicalAggregateCases':40,'differingAggregateCases':10,'deltaDefinition':'ns-3.49 minus ns-3.47','figures':records},indent=2)+'\n')
 print('Created',len(records),'comparison PNGs in',a.output)
if __name__=='__main__':main()
