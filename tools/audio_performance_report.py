#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Combine completed final runs into a reproducible matrix/report (no device IO)."""
import argparse,csv,json,itertools,hashlib
from pathlib import Path
DEADLINE=128*544/24
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--matrix',type=Path,nargs='+',required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--corners',type=Path,nargs='+');p.add_argument('--package',type=Path,required=True)
    p.add_argument('--contract',type=Path,default=Path('tools/audio_voice_contract.json'));a=p.parse_args()
    if a.output.exists():p.error('Use a new report directory; preserve earlier evidence')
    rows=[json.loads(l) for path in a.matrix for l in path.read_text().splitlines()]
    assert len(rows)==1296 and [r['assignment'] for r in rows]==[list(c) for c in itertools.product(range(6),repeat=4)]
    quotas=json.loads(a.contract.read_text())['caps'];assert [r['voices'] for r in rows]==quotas
    required=('audio_late','audio_deadline_misses','voices_shed','voices_given_up','core1_faults','core1_rejected','core1_timeouts',
              'resource_failures','missed_frames','play_underruns','play_overruns','cap_underruns','cap_overruns','bad_packets')
    safe=all(not any(r['delta'][k] for k in required+('bench_shed_requests',))
        and r['peak_complete_upper_us']<DEADLINE and r['voices_min']==sum(r['voices'])
        and r['core1_online_all'] and not r['fixed_bench']
        and r['metadata']['clock_mhz']==360 and r['metadata']['timer5_max_ticks']<2400
        and r['metadata']['half_frames']==128 and r['metadata']['caps']==r['voices']
        and r['metadata']['admission']==1 and r['metadata']['profiling']==0 and r['metadata']['benchmark_support']==0 for r in rows)
    a.output.mkdir(parents=True)
    with (a.output/'combination-matrix.csv').open('w',newline='') as f:
        w=csv.writer(f);w.writerow(('track1','track2','track3','track4','total_slots_including_tails','complete_upper_us','DMA_misses',
            'elapsed_misses','emergency_cuts','normal_capacity_replacements','remaining_us','headroom_percent','within_80_percent_target'))
        for r in rows:w.writerow((*r['engines'],'/'.join(map(str,r['voices'])),r['peak_complete_upper_us'],r['delta']['audio_late'],r['delta']['audio_deadline_misses'],
                    r['delta']['voices_shed'],r['delta']['capacity_steals'],round(DEADLINE-r['peak_complete_upper_us'],2),round(r['headroom_pct'],2),r['passed']))
    summary=dict(assignments=len(rows),normal_admission=True,deadline_safe_and_voices_preserved=safe,
        target_passes=sum(r['passed'] for r in rows),worst_complete_upper_us=max(r['peak_complete_upper_us'] for r in rows),deadline_us=DEADLINE,
        min_guaranteed_total_slots=min(sum(r['voices']) for r in rows),max_guaranteed_total_slots=max(sum(r['voices']) for r in rows),
        counters={k:sum(r['delta'][k] for r in rows) for k in required},
        diagnostic_retries=sum(r['diagnostic_retries'] for r in rows),
        package=str(a.package),package_sha256=hashlib.sha256(a.package.read_bytes()).hexdigest(),matrix_inputs=list(map(str,a.matrix)),
        shared_chorus=all(r['shared_chorus'] for r in rows),modulation_slots_per_track=min(r['modulation_slots'] for r in rows))
    if a.corners:
        corners=[json.loads(l) for path in a.corners for l in path.read_text().splitlines()]
        summary['corners']=dict(cases=len(corners),within_initial_85_percent_screen=sum(r['deadline_safe'] for r in corners),
            observed_deadline_safe=all(r['peak_complete_upper_us']<DEADLINE and r['core1_online_all']
                and r['metadata']['timer5_max_ticks']<2400 and not any(r['delta'][k] for k in required) for r in corners),
            max_us=max(r['peak_complete_upper_us'] for r in corners),inputs=list(map(str,a.corners)))
    (a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
