#!/usr/bin/env python3
"""Repeat the ns-3.47 window-rule study with original startup and sampling phase."""
from pathlib import Path
import hashlib,os,re,shutil,subprocess
import run_qtsdr_window_rule_study as study

BASE=Path(__file__).resolve().parent
NS3=BASE.parent/'ns-3.47'
STUDY=BASE/'trace-results/ns-3.47/qt-sdr-window-rule-original-timing'
DRIVER=NS3/'examples/tcp/tcp_qt_sims.cc'

def timing_source(active):
    source=active.decode()
    source=re.sub(r'double appStart\{[^}]+\};','double appStart{0.00001};',source,count=1)
    source=re.sub(r'double warmup\{[^}]+\};','double warmup{0.00001};',source,count=1)
    if 'samplingPhaseSeconds' in source:
        source=re.sub(r'double samplingPhaseSeconds\{[^}]+\};','double samplingPhaseSeconds{0.001};',source,count=1)
    else:
        old='const Time next = std::min(stop, Simulator::Now() + Seconds(m_c.interval));'
        new='''const Time next = Simulator::Now() == Seconds(m_c.warmup)
                                  ? Seconds(0.001)
                                  : std::min(stop, Simulator::Now() + Seconds(m_c.interval));'''
        assert old in source
        source=source.replace(old,new)
        source=source.replace('<< "controllerVersion=','<< "samplingPhaseSeconds=0.001\\n"\n      << "controllerVersion=')
    return source

if __name__=='__main__':
    STUDY.mkdir(parents=True,exist_ok=True)
    active_driver=DRIVER.read_bytes()
    active_controller=(NS3/'src/internet/model/tcp-qt-sdr.cc').read_bytes()
    backup=NS3/'qt-window-rule-backup'/('driver-before-study-'+hashlib.sha256(active_driver).hexdigest()[:16]+'.cc')
    if not backup.exists():backup.write_bytes(active_driver)
    source=timing_source(active_driver)
    (STUDY/'timing-driver.cc').write_text(source)
    study.STUDY=STUDY
    env=dict(os.environ,PATH=str(BASE.parent/'tools/bin')+os.pathsep+os.environ['PATH'],CCACHE_DIR=str(NS3/'.ccache'))
    try:
        DRIVER.write_text(source)
        study.stage('baseline-recorded',study.runner.ALGORITHMS)
        study.stage('baseline-equality',['TcpQtSdr'])
        study.stage('proposed',study.runner.ALGORITHMS)
    finally:
        DRIVER.write_bytes(active_driver)
        (NS3/'src/internet/model/tcp-qt-sdr.cc').write_bytes(active_controller)
        with (STUDY/'restore-production-build.log').open('w') as log:
            subprocess.run([str(BASE.parent/'tools/bin/python3'),str(NS3/'ns3'),'build','tcp_qt_sims','-j','8'],cwd=NS3,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        print('Restored and rebuilt the previous active driver and controller.',flush=True)
