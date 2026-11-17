import sys
sys.path.insert(0,'scripts')
from decode_fight_stats_blob import read,decode
pid=22300
for addr in (0xfc84ab182d,0xfc84ab185d,0xfd3527227d,0xfd352722ad):
 for size in (32768,65536,262144):
  try:
   b=read(pid,addr,size)
   r,n=decode(b)
   print(hex(addr),size,n,list(r.keys()),list(r.get('Output',{}).keys())[:5]);break
  except Exception as x:print(hex(addr),size,type(x).__name__,str(x)[:50])
