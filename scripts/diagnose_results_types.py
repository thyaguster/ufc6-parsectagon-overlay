import sys,time,collections
sys.path.insert(0,'scripts')
import export_complete_fight_report as e
import fight_report_export as f
pid,_=f.game_process();h=f.kernel.OpenProcess(0x1010,0,pid);fields=f.snapshot(h)[1];f.kernel.CloseHandle(h)
def m(r,fields):
 v=r['Output']['RESULTS'];
 for i,x in enumerate(v['FIGHTER_STATS']):
  print(i,len(x),[(k,type(val).__name__) for k,val in x.items() if not isinstance(val,(int,float))],flush=True)
 return True
e.find_response(pid,fields,time.monotonic()+8,m)
