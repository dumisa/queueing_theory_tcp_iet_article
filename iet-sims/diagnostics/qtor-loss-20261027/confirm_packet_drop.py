from pathlib import Path
import json,subprocess,os,csv
exec(Path(__file__).with_name('build_diagnostics.py').read_text().split('protected={}')[0])
for ver in ('3.47','3.49'):
 ns=ROOT/('ns-'+ver);folder=D/('ns-'+ver)/'drop-driver';folder.mkdir(parents=True,exist_ok=True)
 s=(D/('ns-'+ver)/'driver/tcp_qt_sims_diagnostic.cc').read_text()
 s=s.replace('#include "ns3/core-module.h"','#include "ns3/core-module.h"\n#include "ns3/ppp-header.h"')
 s=s.replace('        Ipv4GlobalRoutingHelper::PopulateRoutingTables();','        m_reverse->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&Experiment::DiagDrop, this));\n        Ipv4GlobalRoutingHelper::PopulateRoutingTables();')
 methods='''    void DiagDrop(Ptr<const Packet> packet)
    {
        auto p = packet->Copy(); PppHeader ppp; Ipv4Header ip; TcpHeader tcp;
        p->RemoveHeader(ppp); p->RemoveHeader(ip);
        if(ip.GetProtocol()!=6) return;
        p->RemoveHeader(tcp);
        uint32_t flow=999; for(uint32_t i=0;i<m_destinations.size();++i) if(m_destinations[i]==ip.GetDestination()) flow=i;
        m_diag << Simulator::Now().GetSeconds() << ',' << flow << ",packet_drop," << tcp.GetSequenceNumber().GetValue() << ',' << p->GetSize() << '\\n';
    }
'''
 s=s.replace('    void DiagTx(',methods+'    void DiagTx(',1)
 s=s.replace('        if (packet->GetSize() > 0) ++m_flows[i].diagTx;','''        if (packet->GetSize() > 0) ++m_flows[i].diagTx;
        if (header.GetSequenceNumber().GetValue()==1243833)
            m_diag << Simulator::Now().GetSeconds() << ',' << i << ",target_sequence_tx,1243833," << packet->GetSize() << '\\n';''')
 cpp=folder/'tcp_qt_sims_drop_diagnostic.cc';cpp.write_text(s);obj=folder/'drop.o';compile_obj(ns,'tcp_qt_sims.cc',cpp,obj)
 cmd=tokens_for(ns,ns/'build/examples/tcp'/('ns'+ver+'-tcp_qt_sims'));binary=folder/'tcp_qt_drop_diagnostic';cmd[cmd.index('-o')+1]=str(binary);old='examples/tcp/CMakeFiles/tcp_qt_sims.dir/tcp_qt_sims.cc.o';cmd[cmd.index(old)]=str(obj);subprocess.run(cmd,cwd=ns/'cmake-cache',capture_output=True,check=True)
 out=D/'results'/('packet-drop-confirmation-ns'+ver.replace('.',''));out.mkdir(exist_ok=True)
 args=[str(binary),'--scenario=loss','--transport_prot=TcpQtTor','--error_p=0.01','--sim_duration=3','--rngSeed=12345','--rngRun=1','--sim_name='+str(out)]
 with (out/'run.log').open('w') as f:subprocess.run(args,env=dict(os.environ,DYLD_LIBRARY_PATH=str(ns/'build/lib')),stdout=f,stderr=subprocess.STDOUT,check=True)
 case=next(out.glob('loss/TcpQtTor/*/recovery-events.csv'));events=list(csv.DictReader(case.open()))
 target=[e for e in events if e['flow']=='1' and e['value']=='1243833']
 assert any(e['event']=='packet_drop' and 1.7<float(e['time_s'])<1.73 for e in target)
 print(ver,json.dumps(target),flush=True)
 baseline=next((D/'results'/('ns'+ver.replace('.','')+'-default')).glob('loss/TcpQtTor/*/aggregate.csv'))
 short=list(csv.DictReader((case.parent/'aggregate.csv').open()));long=list(csv.DictReader(baseline.open()));assert short[:-1]==long[:len(short)-1]
 (out/'target-sequence.json').write_text(json.dumps(target,indent=2)+'\n')
print('Dropped retransmission confirmed in both versions; pre-stop measurements unchanged.')
