#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Final original-project comparison and a musical recording. No new backups."""
import argparse,contextlib,json,time
from pathlib import Path
import mido
from audio_performance_device import (ReliableDevice,restore_runtime,restore_settings,verify,inventory,stats,metadata,setup,usb_streams,COUNTERS,DEADLINE_US,param_ids)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--backup',type=Path,required=True);p.add_argument('--stress',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--seconds',type=float,default=60);p.add_argument('--demo-only',action='store_true');a=p.parse_args()
    if a.output.exists():p.error('Output must be new; existing artifacts are preserved')
    if not (a.backup/'manifest.json').is_file() or not (a.stress/'0.bin').is_file():p.error('Existing original/stress state required')
    a.output.mkdir(parents=True);d=ReliableDevice('Melodee');d.performance_production=d.performance_metadata=True;d.performance_fx=d.performance_modulation=False
    try:
        verify(d,a.backup)
        m=metadata(d)
        if m['profiling'] or m['benchmark_support'] or m['admission']!=1 or m['half_frames']!=128:
            raise RuntimeError(f'Wrong production build: {m}')
        if not a.demo_only:
            d.silence();restore_runtime(d,a.stress)
            with usb_streams(a.output/'stress-project.wav','int24'):
                time.sleep(1)
                before=stats(d,True);d.request(74,[2]);start=time.monotonic();rows=[]
                while time.monotonic()-start<a.seconds:
                    time.sleep(min(1,max(0,a.seconds-(time.monotonic()-start))));rows.append(stats(d))
                d.request(74,[3]);elapsed=time.monotonic()-start
                result=dict(seconds=elapsed,peak_complete_upper_us=max(r['audio_full_max_us'] for r in rows),
                    peak_render_without_nested_us=max(r['audio_max_us'] for r in rows),voices_min=min(r['voices_active'] for r in rows),
                    voices_max=max(r['voices_active'] for r in rows),delta={k:(rows[-1][k]-before[k])&0xffffffff for k in COUNTERS},
                    metadata=metadata(d),deadline_us=DEADLINE_US)
                (a.output/'stress-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
        d.silence();restore_runtime(d,a.backup)
        cat=(0,1,2,4);quotas=json.loads(Path('tools/audio_voice_contract.json').read_text())['caps'];q=quotas[((cat[0]*6+cat[1])*6+cat[2])*6+cat[3]]
        _,g=param_ids()
        for key,value in dict(G_ROUTE=0,G_CLOCK=0,G_MFXTYPE=0,G_MFX2TYPE=0,G_MFX3TYPE=0).items():
            v=value+8192;d.request(3,[1,g[key],v&127,v>>7])
        setup(d,cat,q,0)
        time.sleep(1.8) # finish deferred kept-global persistence while stopped
        with usb_streams(a.output/'musical-demo.wav','int24'):
            time.sleep(1)
            before=stats(d,True);rows=[];previous=[]
            for step,root in enumerate((48,48,53,53,55,55,51,51)):
                for ch,n in previous:d.outgoing.send(mido.Message('note_off',channel=ch,note=n))
                notes=[(0,root+12),(1,root)]+[(2,root+19+5*k) for k in range(q[2])]+[(3,36),(3,38 if step%2 else 42)]
                previous=[n for n in notes if n[0]<3]
                for ch,n in notes:d.outgoing.send(mido.Message('note_on',channel=ch,note=n,velocity=100))
                time.sleep(.4);rows.append(stats(d))
            for ch,n in previous:d.outgoing.send(mido.Message('note_off',channel=ch,note=n))
            time.sleep(.6);rows.append(stats(d))
            (a.output/'demo-results.json').write_text(json.dumps(dict(assignment=list(cat),caps=q,
                peak_complete_upper_us=max(r['audio_full_max_us'] for r in rows),
                delta={k:(rows[-1][k]-before[k])&0xffffffff for k in COUNTERS},metadata=metadata(d)),indent=2)+'\n')
    finally:
        try:
            d.silence();restore_settings(d,a.backup);restore_runtime(d,a.backup);verify(d,a.backup)
            expected=next(e for e in json.loads((a.backup/'manifest.json').read_text()) if e['id']==0)
            assert next(row for row in inventory(d) if row[0]==0)[1:]==(expected['length'],expected['crc'])
            (a.output/'restoration.json').write_text(json.dumps(dict(verified=True,runtime_crc=expected['crc'],persistent_objects=27))+'\n')
        finally:d.close()
if __name__=='__main__':main()
