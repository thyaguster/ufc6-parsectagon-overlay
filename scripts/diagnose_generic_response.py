import sys,time
sys.path.insert(0,'scripts')
import export_complete_fight_report as e
import fight_report_export as f
pid,_=f.game_process();h=f.kernel.OpenProcess(0x1010,0,pid);fields=f.snapshot(h)[1];f.kernel.CloseHandle(h)
e.PREFIX=b'\x08Success\x00\x02\x01\x08Output\x00\x01'
n=0
def matcher(r,fields):
 global n
 out=r.get('Output',{})
 if not isinstance(out,dict):return False
 n+=1
 if {'RED_CORNER_FIGHTER','BLUE_CORNER_FIGHTER'}<=set(out):
  print('both',n,'keys',list(out)[:7],'round',out.get('ROUND_NUMBER'),'valid',e.matches_current_fight(r,fields),flush=True)
 else: print('other',n,'keys',list(out)[:4],flush=True)
 return e.matches_current_fight(r,fields)
try:
 r,nbytes=e.find_response(pid,fields,time.monotonic()+35,matcher)
 print('FOUND',nbytes,list(r['Output'])[:10],flush=True)
except RuntimeError:print('none count',n,flush=True)
