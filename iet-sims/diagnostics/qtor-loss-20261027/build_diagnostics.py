from pathlib import Path
import json,hashlib,shlex,subprocess,shutil
ROOT=Path(__file__).resolve().parents[3];D=Path(__file__).resolve().parent
NINJA=shutil.which('ninja') or str(ROOT/'tools/bin/ninja')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def tokens_for(ns,original):
 out=subprocess.check_output([NINJA,'-C',str(ns/'cmake-cache'),'-t','commands',str(original)],text=True)
 tokens=shlex.split(out.splitlines()[-1]);assert tokens[:2]==[':','&&'] and tokens[-2:]==['&&',':'];return tokens[2:-2]
def compile_obj(ns,filename,source,obj):
 spec=next(c for c in json.loads((ns/'cmake-cache/compile_commands.json').read_text()) if c['file'].endswith('/'+filename))
 cmd=shlex.split(spec['command']);cmd=[('-DPROJECT_SOURCE_PATH="'+str(ns)+'"') if x.startswith('-DPROJECT_SOURCE_PATH=') else x for x in cmd]
 cmd.extend(['-I',str(ns/'src/internet/model')])
 cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(obj)
 p=subprocess.run(cmd,cwd=spec['directory'],capture_output=True,text=True)
 (obj.parent/'compile.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr
 return cmd
protected={}
for ver in ('3.47','3.49'):
 ns=ROOT/('ns-'+ver)
 for name in ['examples/tcp/tcp_qt_sims.cc','src/internet/model/tcp-tx-buffer.cc','src/internet/model/tcp-tx-buffer.h','build/lib/libns'+ver+'-internet.dylib','build/examples/tcp/ns'+ver+'-tcp_qt_sims']+['src/internet/model/tcp-qt-'+a+e for a in ('sdr','tor') for e in ('.cc','.h')]:protected[str((ns/name).relative_to(ROOT))]=sha(ns/name)
(D/'default-hashes-before.json').write_text(json.dumps(protected,indent=2)+'\n')
# Add passive observation to a driver copy only.
for ver in ('3.47','3.49'):
 ns=ROOT/('ns-'+ver);folder=D/('ns-'+ver)/'driver';folder.mkdir(parents=True,exist_ok=True)
 s=(ns/'examples/tcp/tcp_qt_sims.cc').read_text()
 s=s.replace('struct Flow\n{','struct Flow\n{\n    uint64_t diagTx{0}, diagRetrans{0}, diagRetransBytes{0}, diagRecovery{0}, diagLoss{0};\n    double diagStateTime{0}, diagRecoveryTime{0}, diagLossTime{0};\n    TcpSocketState::TcpCongState_t diagState{TcpSocketState::CA_OPEN};')
 assert 'diagTx' in s
 s=s.replace('        Open(m_aggregate, "aggregate.csv");','        Open(m_diag, "recovery-events.csv");\n        m_diag << "time_s,flow,event,value,bytes\\n";\n        Open(m_aggregate, "aggregate.csv");')
 anchor='        const bool cwnd =\n'
 s=s.replace(anchor,'''        NS_ABORT_MSG_IF(!socket->TraceConnectWithoutContext("CongState", MakeCallback(&Experiment::DiagState, this, i)), "Missing CongState");
        NS_ABORT_MSG_IF(!socket->TraceConnectWithoutContext("Retransmission", MakeCallback(&Experiment::DiagRetrans, this, i)), "Missing Retransmission");
        NS_ABORT_MSG_IF(!socket->TraceConnectWithoutContext("Tx", MakeCallback(&Experiment::DiagTx, this, i)), "Missing Tx");
'''+anchor)
 methods='''    void DiagTx(uint32_t i, Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
    {
        if (packet->GetSize() > 0) ++m_flows[i].diagTx;
    }
    void DiagRetrans(uint32_t i, Ptr<const Packet> packet, const TcpHeader& header, const Address& local, const Address& peer, Ptr<const TcpSocketBase> socket)
    {
        auto& f = m_flows[i]; ++f.diagRetrans; f.diagRetransBytes += packet->GetSize();
        m_diag << Simulator::Now().GetSeconds() << ',' << i << ",retransmission," << header.GetSequenceNumber().GetValue() << ',' << packet->GetSize() << '\\n';
    }
    void DiagState(uint32_t i, TcpSocketState::TcpCongState_t old, TcpSocketState::TcpCongState_t value)
    {
        auto& f = m_flows[i]; const double now = Simulator::Now().GetSeconds();
        if (f.diagState == TcpSocketState::CA_RECOVERY) f.diagRecoveryTime += now - f.diagStateTime;
        if (f.diagState == TcpSocketState::CA_LOSS) f.diagLossTime += now - f.diagStateTime;
        f.diagStateTime = now; f.diagState = value;
        if (value == TcpSocketState::CA_RECOVERY) ++f.diagRecovery;
        if (value == TcpSocketState::CA_LOSS) ++f.diagLoss;
        m_diag << now << ',' << i << ",state," << static_cast<int>(value) << ',' << static_cast<int>(old) << '\\n';
    }
'''
 s=s.replace('    /** @param i Flow index. @param old Previous bytes. @param value New bytes. */\n    void Cwnd',methods+'\n    /** @param i Flow index. @param old Previous bytes. @param value New bytes. */\n    void Cwnd',1)
 s=s.replace('        std::ofstream summary;','''        std::ofstream diag; Open(diag, "recovery-summary.csv");
        diag << "flow,tx_packets,retrans_packets,retrans_bytes,recovery_entries,loss_entries,recovery_seconds,loss_seconds\\n";
        for (uint32_t i=0; i<m_c.flows; ++i) {
            auto& f=m_flows[i]; const double dt=m_c.duration-f.diagStateTime;
            if(f.diagState==TcpSocketState::CA_RECOVERY) f.diagRecoveryTime+=dt;
            if(f.diagState==TcpSocketState::CA_LOSS) f.diagLossTime+=dt;
            diag << i << ',' << f.diagTx << ',' << f.diagRetrans << ',' << f.diagRetransBytes << ',' << f.diagRecovery << ',' << f.diagLoss << ',' << f.diagRecoveryTime << ',' << f.diagLossTime << '\\n';
        }
        std::ofstream summary;''',1)
 s=s.replace('    std::ofstream m_events;', '    std::ofstream m_diag;\n    std::ofstream m_events;')
 cpp=folder/'tcp_qt_sims_diagnostic.cc';cpp.write_text(s);obj=folder/'diagnostic.o';compile_obj(ns,'tcp_qt_sims.cc',cpp,obj)
 original=ns/'build/examples/tcp'/('ns'+ver+'-tcp_qt_sims');cmd=tokens_for(ns,original);cmd[cmd.index('-o')+1]=str(folder/'tcp_qt_diagnostic')
 target='examples/tcp/CMakeFiles/tcp_qt_sims.dir/tcp_qt_sims.cc.o';assert target in cmd;cmd[cmd.index(target)]=str(obj)
 subprocess.run(cmd,cwd=ns/'cmake-cache',check=True,capture_output=True)
 print('Built passive driver',ver,flush=True)
# Counterfactual library copies: only the TCP transmit-buffer object is changed.
s49=ROOT/'ns-3.49';orig49=(s49/'src/internet/model/tcp-tx-buffer.cc').read_text()
clear_start=orig49.index('                    if ((*item_it)->m_retrans)\n',orig49.index('TcpTxBuffer::Update('))
clear_end=orig49.index('                    (*item_it)->m_sacked = true;',clear_start)
clear_block=orig49[clear_start:clear_end]
call='        MarkRetransmittedSegmentsLost(newestSackedSentTime);\n';assert call in orig49
for label,clear,detect in [('no-retrans-loss-detection',True,False),('no-sacked-retrans-clearing',False,True),('both-sack-fixes-off',False,False)]:
 folder=D/'ns-3.49'/label;folder.mkdir(parents=True,exist_ok=True);(folder/'lib').mkdir(exist_ok=True)
 s=orig49
 if not clear:s=s.replace(clear_block,'',1)
 if not detect:s=s.replace(call,'',1)
 cpp=folder/'tcp-tx-buffer.cc';cpp.write_text(s);shutil.copy2(s49/'src/internet/model/tcp-tx-buffer.h',folder/'tcp-tx-buffer.h')
 obj=folder/'tcp-tx-buffer.o';compile_obj(s49,'tcp-tx-buffer.cc',cpp,obj)
 lib=s49/'build/lib/libns3.49-internet.dylib';cmd=tokens_for(s49,lib);cmd[cmd.index('-o')+1]=str(folder/'lib/libns3.49-internet.dylib')
 old='src/internet/CMakeFiles/internet.dir/model/tcp-tx-buffer.cc.o';assert old in cmd;cmd[cmd.index(old)]=str(obj)
 subprocess.run(cmd,cwd=s49/'cmake-cache',check=True,capture_output=True)
 (folder/'build-manifest.json').write_text(json.dumps({'sourceSha256':sha(cpp),'librarySha256':sha(folder/'lib/libns3.49-internet.dylib'),'compile':str(obj),'note':'Isolated ns-3.49 TCP stack ablation; default Qt algorithms and libraries unchanged.'},indent=2)+'\n')
 print('Built isolated stack ablation',label,flush=True)
for name,h in protected.items():assert sha(ROOT/name)==h,name
print('All default source and engine hashes unchanged.',flush=True)
