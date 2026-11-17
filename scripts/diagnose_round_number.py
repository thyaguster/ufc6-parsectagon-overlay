import sys,time,ctypes as ct
sys.path.insert(0,'scripts')
import export_complete_fight_report as e
import fight_report_export as f
pid,_=f.game_process()
h=f.kernel.OpenProcess(0x1010,0,pid)
info=e.MemoryBasicInformation();a=0x10000;deadline=time.monotonic()+30;seen=set();needle=b'\x08ROUND_NUMBER\x00\x03';n=0
try:
 while a<0x7fff00000000 and time.monotonic()<deadline:
  if not f.kernel.VirtualQueryEx(h,ct.c_void_p(a),ct.byref(info),ct.sizeof(info)):break
  base=int(info.BaseAddress or 0);size=int(info.RegionSize);a=base+max(size,0x1000)
  if info.State!=0x1000 or info.Protect&0xff not in e.READABLE or info.Protect&0x100:continue
  for off in range(0,size,e.CHUNK):
   if time.monotonic()>deadline:break
   data=e.read_chunk(h,base+off,min(size-off,e.CHUNK))
   if not data:continue
   p=data.find(needle)
   while p>=0:
    x=base+off+p
    if x not in seen:
     seen.add(x);n+=1
     start=max(0,p-200);b=data[start:min(len(data),p+110)]
     print(hex(x),'value',int.from_bytes(data[p+len(needle):p+len(needle)+8],'little'),'priorSuccess',b.rfind(b'\x08Success\x00\x02\x01'),'before',repr(b[:110]),flush=True)
    p=data.find(needle,p+1)
finally:f.kernel.CloseHandle(h)
print('count',n,flush=True)
