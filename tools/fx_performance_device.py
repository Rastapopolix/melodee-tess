#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the opt-in FX profiler. Does not edit track parameters or stored music."""
import argparse,json,time,sys
from prophet_device import Device
from usb_audio_stats import snapshot
NAMES=('OFF','PHASER','WIDTH','FORMANT','TAPE','SPIT','ECHO','ROOM','REVERB','CHORUS')
def u32(a):return sum(v<<(7*i) for i,v in enumerate(a))
def profile(d,args=()):
    b=d.request(97,args)
    if len(b) not in (66,71) or b[0]:raise RuntimeError('Requires MELODEE_FX_PROFILE=1 firmware')
    v=[u32(b[1+k*5:6+k*5]) for k in range((len(b)-1)//5)]
    return dict(control=v[0],ticks=v[1:13:3],blocks=v[2:13:3],max_ticks=v[3:13:3],eq_status=v[13] if len(v)>13 else 2)
diag_retries=0
def stats(d,window):
    global diag_retries
    for attempt in range(3):
        try:return snapshot(d.incoming,d.outgoing,window,True,True,True)
        except TimeoutError:
            if attempt==2:raise
            diag_retries+=1;print("Retrying a missed read-only USB diagnostics reply",file=sys.stderr,flush=True);time.sleep(.05)
def run(d,modes,tracks,extreme,duration,bus=False,notes=()):
    global diag_retries
    diag_retries=0
    if notes:
        import mido
        for c in range(4):d.outgoing.send(mido.Message('control_change',channel=c,control=120,value=0))
        time.sleep(.03)
        for c,n in notes:d.outgoing.send(mido.Message('note_on',channel=c,note=n,velocity=100))
    a=[(1<<tracks)-1,*modes,127 if extreme else 60,127 if extreme else 64]
    if bus:a.append(1)
    profile(d,a);time.sleep(.3)
    before=profile(d);s0=stats(d,True)
    time.sleep(duration)
    after=profile(d);s1=stats(d,False)
    costs=[]
    for k in range(1 if bus else tracks):
        blocks=after['blocks'][k]-before['blocks'][k];ticks=(after['ticks'][k]-before['ticks'][k])&0xffffffff
        if not blocks:raise RuntimeError('Profiler timed out or audio stopped')
        costs.append(ticks/blocks/32*44100/24000000*100)
    result=dict(diagnostics_retries=diag_retries,routing="send" if bus else "insert",effects=[NAMES[k] for k in modes],tracks=tracks,extreme=extreme,fx_cpu_pct=sum(costs),track_fx_pct=costs,
                max_fx_block_us=max(after['max_ticks'])/24,cpu_pct=s1['cpu_q8']/256*100,
                late=s1['audio_late']-s0['audio_late'],shed=s1['voices_shed']-s0['voices_shed'],
                timeouts=s1['core1_timeouts']-s0['core1_timeouts'],resource_failures=s1['resource_failures']-s0['resource_failures'],
                core1_jobs=s1['core1_jobs']-s0['core1_jobs'],core1_faults=s1['core1_faults']-s0['core1_faults'],core1_rejected=s1['core1_rejected']-s0['core1_rejected'],core1_online=s1['core1_online'],
                resource_used=s1['resource_used'],voices=s1['voices_active'],audio_max_us=s1['audio_max_us'],eq_status=after['eq_status'])
    print(json.dumps(result),flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',default='Melodee');p.add_argument('--seconds',type=float,default=1);p.add_argument('--bus',action='store_true');p.add_argument('--voices',action='store_true');p.add_argument('--sustain',type=float,default=6);a=p.parse_args();d=Device(a.port)
    notes=[(c,48+c*7+k*4) for c in range(3) for k in range(2)]+[(3,36)] if a.voices else []
    if notes:
        import mido
        for c,n in notes:d.outgoing.send(mido.Message('note_on',channel=c,note=n,velocity=100))
    try:
        for extreme in (False,True):
            for tracks in (1,4):
                for fx in range(len(NAMES)):run(d,[fx,0,0],tracks,extreme,a.seconds,a.bus,notes)
                for fx in range(1,len(NAMES)):run(d,[fx,fx,fx],tracks,extreme,a.seconds,a.bus,notes)
                run(d,[1,4,8],tracks,extreme,a.seconds,a.bus,notes);run(d,[3,6,8],tracks,extreme,a.seconds,a.bus,notes)
        # Keep the worst dense chain running long enough to expose deadlines.
        run(d,[8,8,8],4,True,a.sustain,a.bus,notes)
    finally:
        profile(d,[0,0,0,0,60,64])
        for c,n in notes:d.outgoing.send(mido.Message('note_off',channel=c,note=n,velocity=0))
        d.close()
if __name__=='__main__':main()
