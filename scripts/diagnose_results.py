import sys,time
sys.path.insert(0,'scripts')
import export_complete_fight_report as e
import fight_report_export as f
pid,_=f.game_process();h=f.kernel.OpenProcess(0x1010,0,pid);fields=f.snapshot(h)[1];f.kernel.CloseHandle(h)
e.PREFIX=b'\x08Success\x00\x02\x01\x08Output\x00\x01'
def m(r,fields):
 out=r['Output'];res=out['RESULTS']; print('RESULTS', {k:v for k,v in res.items() if k!='FIGHTER_STATS'},flush=True)
 for i,x in enumerate(res['FIGHTER_STATS']):print('index',i,'kd',x.get('KNOCKDOWNS_LANDED'),'sigThrown',x.get('SIGNIFICANT_STRIKES_THROWN'),'sigLanded',x.get('SIGNIFICANT_STRIKES_LANDED'),'strikesThrown',x.get('STRIKES_THROWN'),'strikesLanded',x.get('STRIKES_LANDED'),'control',x.get('CONTROL_TIME'),flush=True)
 return True
r,n=e.find_response(pid,fields,time.monotonic()+8,m)
