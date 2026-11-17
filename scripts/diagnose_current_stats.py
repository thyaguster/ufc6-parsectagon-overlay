import sys,time
sys.path.insert(0,'scripts')
import export_complete_fight_report as e
import fight_report_export as f
pid,engine=f.game_process()
h=f.kernel.OpenProcess(0x1010,0,pid)
s=f.snapshot(h)
f.kernel.CloseHandle(h)
fields=s[1]
print('pid',pid,'round',fields['Global_RoundNum'][0]+1,'kd',fields['Online_KnockdownsLanded'],'sig_thrown',fields['Online_SignificantStrikesThrown'],'sig_landed',fields['Online_AdvanceStrikesLanded'],flush=True)
def match(response,fields):
 out=response.get('Output',{})
 if not isinstance(out,dict):return False
 print('candidate',list(out.keys())[:6],flush=True)
 for corner in ('RED_CORNER_FIGHTER','BLUE_CORNER_FIGHTER'):
  fighter=out.get(corner,{})
  if isinstance(fighter,dict):print(corner,'name',fighter.get('NAME'),'id',fighter.get('ID'),'rounds',len(fighter.get('ROUNDS',[])),'total',fighter.get('TOTAL',{}).get('KNOCKDOWNS_LANDED'),fighter.get('TOTAL',{}).get('SIGNIFICANT_STRIKES_THROWN'),fighter.get('TOTAL',{}).get('SIGNIFICANT_STRIKES_LANDED'),flush=True)
 print('round',out.get('ROUND_NUMBER'),'valid',e.matches_current_fight(response,fields),flush=True)
 return False
try:e.find_response(pid,fields,time.monotonic()+20,match)
except RuntimeError:print('done',flush=True)
