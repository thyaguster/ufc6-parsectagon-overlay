#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <math.h>
#include "photo_math.h"
#include "photo_mode.h"
#ifndef NATIVE_CAMERA_TEST
#ifndef QUIT_MENU_TEST
#include "engine_fingerprint.h"
#endif
#include "exact_hooks.h"
#endif
typedef void (__attribute__((sysv_abi)) *Update)(uintptr_t,float);
typedef void (__attribute__((sysv_abi)) *Mode)(uintptr_t,unsigned);
typedef void (__attribute__((sysv_abi)) *Pose)(uintptr_t,const float*);
static Update original;
static Mode set_mode;
static Pose set_pose;
static volatile LONG requested,focused,state,ticks;
static uintptr_t owner,camera;
static unsigned previous_mode;
static unsigned char saved_flags[3];
static float saved_pose[16],test_pose[16];
#ifndef PHOTO_NOW
#define PHOTO_NOW GetTickCount64
#endif
static ULONGLONG deadline;
static SRWLOCK photo_guard=SRWLOCK_INIT;
static PhotoInput published_input;
static volatile LONG commands,installed;
static volatile LONG target_fov;
static ULONGLONG input_tick;
static float initial_pose[16],saved_fov,current_fov;

static volatile LONG skel_installed,fpv_target,fpv_active,fpv_diag_code;
static uintptr_t fpv_owner,fpv_camera;
static unsigned fpv_prev_mode;
static unsigned char fpv_saved_flags[3];
static float fpv_saved_pose[16],fpv_saved_fov;
static float fpv_pitch_deg,fpv_height_m,fpv_fwd_m,fpv_grapple_dist=0.65f;
static int fpv_fov_deg=90,fpv_wobble=0;
static int fpv_cfg_loaded;
static ULONGLONG fpv_key_tick,fpv_cfg_tick;
static void fpv_cfg_load(void){
 ULONGLONG now=PHOTO_NOW();
 if(fpv_cfg_loaded&&now-fpv_cfg_tick<1000)return;
 fpv_cfg_loaded=1;
 fpv_cfg_tick=now;
 FILE *f=fopen("ufc6_fpv.ini","r");
 if(f){
  int w=0;
  int got=fscanf(f,"pitch=%f height=%f fwd=%f fov=%d grapple=%f wobble=%d",&fpv_pitch_deg,&fpv_height_m,&fpv_fwd_m,&fpv_fov_deg,&fpv_grapple_dist,&w);
  if(got>=6)fpv_wobble=w;
  fclose(f);
 }
 if(fpv_fov_deg<60||fpv_fov_deg>110)fpv_fov_deg=90;
 if(fpv_grapple_dist<0.f||fpv_grapple_dist>1.2f)fpv_grapple_dist=0.65f;
 if(fpv_wobble<0||fpv_wobble>3)fpv_wobble=0;
}
static void fpv_cfg_save(void){
 FILE *f=fopen("ufc6_fpv.ini","w");
 if(f){fprintf(f,"pitch=%.2f height=%.3f fwd=%.3f fov=%d grapple=%.2f wobble=%d\n",fpv_pitch_deg,fpv_height_m,fpv_fwd_m,fpv_fov_deg,fpv_grapple_dist,fpv_wobble);fclose(f);}
 fpv_cfg_tick=PHOTO_NOW();
}
static void fpv_poll_keys(void){
 ULONGLONG now=PHOTO_NOW();
 if(now-fpv_key_tick<90)return;
 int changed=0;
 if(GetAsyncKeyState(VK_PRIOR)&0x8000){fpv_pitch_deg+=2.5f;changed=1;}
 if(GetAsyncKeyState(VK_NEXT)&0x8000){fpv_pitch_deg-=2.5f;changed=1;}
 if(GetAsyncKeyState(VK_INSERT)&0x8000){fpv_height_m+=0.05f;changed=1;}
 if(GetAsyncKeyState(VK_DELETE)&0x8000){fpv_height_m-=0.05f;changed=1;}
 if((GetAsyncKeyState(VK_OEM_PLUS)&0x8000)||(GetAsyncKeyState(VK_ADD)&0x8000)){fpv_fov_deg+=5;changed=1;}
 if((GetAsyncKeyState(VK_OEM_MINUS)&0x8000)||(GetAsyncKeyState(VK_SUBTRACT)&0x8000)){fpv_fov_deg-=5;changed=1;}
 if(GetAsyncKeyState(VK_OEM_6)&0x8000){fpv_fwd_m+=0.05f;changed=1;}
 if(GetAsyncKeyState(VK_OEM_4)&0x8000){fpv_fwd_m-=0.05f;changed=1;}
 if((GetAsyncKeyState(VK_END)&0x8000)&&now-fpv_key_tick>=250){fpv_wobble=(fpv_wobble+1)&3;changed=1;}
 if(GetAsyncKeyState(VK_HOME)&0x8000){fpv_pitch_deg=0.f;fpv_height_m=0.f;fpv_fwd_m=0.f;changed=1;}
 if(changed){
  if(fpv_pitch_deg<-85.f)fpv_pitch_deg=-85.f;
  if(fpv_pitch_deg>85.f)fpv_pitch_deg=85.f;
  if(fpv_height_m<-1.5f)fpv_height_m=-1.5f;
  if(fpv_height_m>1.5f)fpv_height_m=1.5f;
  if(fpv_fwd_m<-0.5f)fpv_fwd_m=-0.5f;
  if(fpv_fwd_m>0.8f)fpv_fwd_m=0.8f;
  if(fpv_fov_deg<60)fpv_fov_deg=60;
  if(fpv_fov_deg>110)fpv_fov_deg=110;
  fpv_key_tick=now;
  fpv_cfg_save();
 }
}

static int readable(uintptr_t p,size_t n){
 MEMORY_BASIC_INFORMATION b;if(!p||!VirtualQuery((void*)p,&b,sizeof(b)))return 0;
 return b.State==MEM_COMMIT&&!(b.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&p+n>=p&&p+n<=(uintptr_t)b.BaseAddress+b.RegionSize;
}
/* Confirmed Scaleform geometry draws; no game HUD preference is modified. */
typedef void (__attribute__((sysv_abi)) *HudDraw)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static HudDraw hud_originals[2];
#include "photo_hud_commands.h"
static volatile LONG hud_trimmed[2],hud_trim_rejected[2];
static volatile LONG hud_requested,hud_menu_open;
static volatile LONG64 hud_poll_tick,hud_camera_tick;
static void hud_draw(int which,uintptr_t o,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){
 int hide=InterlockedCompareExchange(&hud_requested,0,0)&&InterlockedCompareExchange(&state,0,0)==2&&
    /* Opening our Vulkan tools must not flip the game's HUD visibility.
       Keep the user's choice until an explicit toggle, focus loss or exit. */
    InterlockedCompareExchange(&focused,0,0)&&
    !(InterlockedCompareExchange(&commands,0,0)&PHOTO_STOP)&&
    PHOTO_NOW()-(ULONGLONG)InterlockedCompareExchange64(&hud_poll_tick,0,0)<1000&&
    PHOTO_NOW()-(ULONGLONG)InterlockedCompareExchange64(&hud_camera_tick,0,0)<1000&&
    photo_hud_range(o,0x15b0,0)&&*(uintptr_t*)o==0x909ba4518ULL;
 uintptr_t stats=hide?*(uintptr_t*)(o+0x1580):0;
 int tracked=hide&&photo_hud_range(stats,0x80,0);
 uint32_t before=tracked?*(uint32_t*)(stats+0x7c):0;
 /* Preserve real counts and all native renderer housekeeping. Suppress only the
    verified final geometry packet before this CPU command list is submitted. */
 hud_originals[which](o,a,b,c,d,e);
 if(tracked){
  if(photo_hud_trim_draw(o,which,stats,before))InterlockedIncrement(&hud_trimmed[which]);
  else InterlockedIncrement(&hud_trim_rejected[which]);
 }
}
static void __attribute__((sysv_abi)) hud_draw_a(uintptr_t o,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){hud_draw(0,o,a,b,c,d,e);}
static void __attribute__((sysv_abi)) hud_draw_b(uintptr_t o,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e){hud_draw(1,o,a,b,c,d,e);}
#include "photo_cage.h"
static uintptr_t time_object;
static LONG saved_scale_bits;
static volatile LONG frozen;
static int time_identity(uintptr_t p){return readable(p,0x40)&&*(uintptr_t*)p==0x9098fe7d8ULL&&*(uintptr_t*)(p+8)==0x90a1d9c00ULL;}
#ifndef NATIVE_CAMERA_TEST
static uintptr_t resolve_time_object(void){return readable(0x909e41a10ULL,8)?*(uintptr_t*)0x909e41a10ULL:0;}
#else
static uintptr_t fixture_time_object;
static uintptr_t resolve_time_object(void){return fixture_time_object;}
#endif
static int freeze_capture(void){
 time_object=0;uintptr_t p=resolve_time_object();if(!time_identity(p))return 0;
 float value=*(float*)(p+0x28);if(!isfinite(value)||value<.1f||value>4)return 0;
 time_object=p;memcpy(&saved_scale_bits,&value,4);return 1;
}
static void freeze_restore(void){
 if(frozen&&time_identity(time_object))InterlockedCompareExchange((volatile LONG*)(time_object+0x28),saved_scale_bits,0);
 InterlockedExchange(&frozen,0);
}
static void freeze_toggle(void){
 if(!time_identity(time_object))return;
 if(frozen){freeze_restore();return;}
 if(InterlockedCompareExchange((volatile LONG*)(time_object+0x28),0,saved_scale_bits)==saved_scale_bits)InterlockedExchange(&frozen,1);
}

static ULONGLONG last_camera_tick;
static int valid_pose(const float *p){
 for(int i=0;i<16;++i)if(!isfinite(p[i]))return 0;
 for(int axis=0;axis<3;++axis){float a=0;for(int j=0;j<3;++j)a+=p[axis*4+j]*p[axis*4+j];if(a<.8f||a>1.2f)return 0;}
 return fabsf(p[12])<100000&&fabsf(p[13])<100000&&fabsf(p[14])<100000;
}
static int identity(uintptr_t o){
 if(!readable(o,0x120)||*(uintptr_t*)o!=0x9099aa5b8ULL)return 0;
 uintptr_t c=*(uintptr_t*)(o+0xc8),registry=*(uintptr_t*)(o+0xc0);
 if(!readable(c,0x1e0)||*(uintptr_t*)c!=0x9099ff808ULL||!readable(registry,0x40))return 0;
 uintptr_t n=*(uintptr_t*)(registry+0x38);
 return readable(n,0x30)&&*(unsigned*)(n+0x20)==0&&*(uintptr_t*)(n+0x28)==c;
}
static void restore(void){
 InterlockedExchange(&hud_requested,0);cage_apply(0);freeze_restore();
 if(!owner||!identity(owner)||*(uintptr_t*)(owner+0xc8)!=camera){InterlockedExchange(&state,-3);owner=0;return;}
 if(*(unsigned*)(owner+8)==1){
  set_mode(owner,previous_mode);
  set_pose(camera,saved_pose);
  *(float*)(camera+0xac)=saved_fov;
  *(unsigned char*)(owner+0xd8)=saved_flags[0];
  *(unsigned char*)(owner+0xd9)=saved_flags[1];
  *(unsigned char*)(owner+0xe0)=saved_flags[2];
  InterlockedExchange(&state,3);
 }else InterlockedExchange(&state,-4);
 owner=0;
}
static void camera_update(uintptr_t o,float dt){
 (void)dt;InterlockedIncrement(&ticks);
 if(owner){
  if(o!=owner)return;
  if(!focused||PHOTO_NOW()>=deadline||PHOTO_NOW()-input_tick>1000||(InterlockedCompareExchange(&commands,0,0)&PHOTO_STOP)){restore();return;}
  if(!identity(owner)||*(unsigned*)(owner+8)!=1){restore();return;}
  InterlockedExchange64(&hud_camera_tick,(LONG64)PHOTO_NOW());
  unsigned command=(unsigned)InterlockedExchange(&commands,0);
  if(command&PHOTO_RESET){memcpy(test_pose,initial_pose,64);current_fov=saved_fov;}
  if(command&PHOTO_FOV){int fov=(int)InterlockedCompareExchange(&target_fov,0,0);if(fov>=20&&fov<=120)current_fov=(float)fov;}
  if(command&PHOTO_FREEZE)freeze_toggle();
  if(command&PHOTO_HUD)InterlockedExchange(&hud_requested,!InterlockedCompareExchange(&hud_requested,0,0));
  if(command&PHOTO_CAGE)InterlockedExchange(&cage_requested,!InterlockedCompareExchange(&cage_requested,0,0));
  ULONGLONG now=PHOTO_NOW();float camera_dt=(float)(now-last_camera_tick)*.001f;last_camera_tick=now;
  PhotoInput in=published_input;
  photo_step(test_pose,&current_fov,camera_dt,in.right,in.up,in.forward,in.yaw,in.pitch,in.zoom,in.fast);
  set_pose(camera,test_pose);*(float*)(camera+0xac)=current_fov;return;
 }
 if(InterlockedCompareExchange(&requested,0,1)!=1)return;
 if(!focused||!identity(o)||*(unsigned*)(o+8)!=0){InterlockedExchange(&state,-1);return;}
 owner=o;camera=*(uintptr_t*)(o+0xc8);previous_mode=*(unsigned*)(o+8);
 memcpy(saved_pose,(void*)(camera+0x180),64);saved_fov=*(float*)(camera+0xac);current_fov=saved_fov;
 saved_flags[0]=*(unsigned char*)(o+0xd8);saved_flags[1]=*(unsigned char*)(o+0xd9);saved_flags[2]=*(unsigned char*)(o+0xe0);
 /* A fresh owner must seed from the current render camera using the native API. */
 if(saved_flags[1]||!valid_pose(saved_pose)||!isfinite(saved_fov)||saved_fov<20||saved_fov>120){owner=0;InterlockedExchange(&state,-2);return;}
 set_mode(o,1);
 memcpy(test_pose,(void*)(camera+0x180),64);
 if(*(unsigned*)(o+8)!=1||!valid_pose(test_pose)){restore();return;}
 memcpy(initial_pose,test_pose,64);InterlockedExchange(&commands,0);last_camera_tick=PHOTO_NOW();
 freeze_capture();InterlockedExchange64(&hud_camera_tick,(LONG64)PHOTO_NOW());InterlockedExchange(&hud_requested,1);
 set_pose(camera,test_pose);deadline=PHOTO_NOW()+1800000;InterlockedExchange(&state,2);
}

static int fpv_owner_valid(uintptr_t o){
 if(!readable(o,0x120)||*(uintptr_t*)o!=0x9099aa5b8ULL)return 0;
 uintptr_t c=*(uintptr_t*)(o+0xc8);
 return readable(c,0x1e0)&&*(uintptr_t*)c==0x9099ff808ULL;
}
static int valid_fighter_pair(const float *m1,const float *m2){
 if(!valid_pose(m1)||!valid_pose(m2))return 0;
 if(fabsf(m1[13])>0.25f||fabsf(m2[13])>0.25f)return 0;
 float r1=m1[12]*m1[12]+m1[14]*m1[14],r2=m2[12]*m2[12]+m2[14]*m2[14];
 if((r1+r2)<0.005f||r1>22.f||r2>22.f)return 0;
 float dx=m2[12]-m1[12],dy=m2[13]-m1[13],dz=m2[14]-m1[14];
 float d2=dx*dx+dy*dy+dz*dz;
 return d2<25.f;
}
static volatile LONG64 fpv_pair_addr;
static volatile LONG fpv_scanning;
static DWORD WINAPI fpv_scan_worker(void *unused){
 (void)unused;
 uintptr_t best_addr=0;
 float best_score=1e9f;
 MEMORY_BASIC_INFORMATION mbi;
 uintptr_t addr=0x1000000000ULL;
 while(addr<0x1200000000ULL&&VirtualQuery((void*)addr,&mbi,sizeof(mbi))){
  uintptr_t base=(uintptr_t)mbi.BaseAddress;
  size_t size=mbi.RegionSize;
  if(mbi.State==MEM_COMMIT&&mbi.Protect==PAGE_READWRITE&&size>=1024*1024){
   for(size_t off=0;off+0xa0<=size;off+=16){
    const float *m1=(const float*)(base+off);
    if(m1[3]!=0.f||m1[7]!=0.f||m1[11]!=0.f||m1[15]!=1.f)continue;
    const float *m2=(const float*)(base+off+0x60);
    if(m2[3]!=0.f||m2[7]!=0.f||m2[11]!=0.f||m2[15]!=1.f)continue;
    if(valid_fighter_pair(m1,m2)){
     float r1=m1[12]*m1[12]+m1[14]*m1[14],r2=m2[12]*m2[12]+m2[14]*m2[14];
     float dx=m2[12]-m1[12],dz=m2[14]-m1[14];
     float score=r1+r2+(dx*dx+dz*dz);
     if(off+0x100<=size){
      const float *m3=(const float*)(base+off+0xc0);
      if(m3[12]>100.f&&m3[13]>100.f&&m3[14]>100.f)score-=10000.f;
     }
     if(score<best_score){best_score=score;best_addr=base+off;}
    }
   }
  }
  addr=base+size;
 }
 if(best_addr)InterlockedExchange64(&fpv_pair_addr,(LONG64)best_addr);
 InterlockedExchange(&fpv_scanning,0);
 return 0;
}
static void fpv_request_scan(void){
 if(InterlockedCompareExchange(&fpv_scanning,1,0)==0){
  HANDLE t=CreateThread(NULL,0,fpv_scan_worker,NULL,0,NULL);
  if(t)CloseHandle(t);else InterlockedExchange(&fpv_scanning,0);
 }
}
static int find_fight_head(int target,float *self_m,float *opp_m){
 uintptr_t arr=(uintptr_t)InterlockedCompareExchange64(&fpv_pair_addr,0,0);
 if(!readable(arr,0xc0))return 0;
 const float *m1=(const float*)(arr+0x00);
 const float *m2=(const float*)(arr+0x60);
 if(!valid_fighter_pair(m1,m2))return 0;
 memcpy(self_m,target==2?m2:m1,64);
 memcpy(opp_m,target==2?m1:m2,64);
 return 1;
}
static void build_fpv_pose(const float *self_m,const float *opp_m,float *pose){
 float sx=self_m[12],sy=self_m[13]+1.58f+fpv_height_m,sz=self_m[14];
 float ox=opp_m[12],oy=opp_m[13]+1.42f,oz=opp_m[14];
 float fx=ox-sx,fy=oy-sy,fz=oz-sz;
 float fl=sqrtf(fx*fx+fy*fy+fz*fz);
 if(fl<0.15f){
  fx=self_m[0];fy=-0.08f;fz=self_m[2];
  fl=sqrtf(fx*fx+fy*fy+fz*fz);
 }
 if(fl<1e-4f){fx=0;fy=0;fz=1;}else{fx/=fl;fy/=fl;fz/=fl;}
 float rx=-fz,ry=0.f,rz=fx;
 float rl=sqrtf(rx*rx+rz*rz);
 if(rl<1e-4f){rx=1;ry=0;rz=0;}else{rx/=rl;rz/=rl;}
 float ux=ry*fz-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy-ry*fx;
 pose[0]=rx;pose[1]=ry;pose[2]=rz;pose[3]=0.f;
 pose[4]=ux;pose[5]=uy;pose[6]=uz;pose[7]=0.f;
 pose[8]=-fx;pose[9]=-fy;pose[10]=-fz;pose[11]=0.f;
 float fwd_dist=0.24f+fpv_fwd_m;
 pose[12]=sx+fx*fwd_dist;
 pose[13]=sy+fy*fwd_dist;
 pose[14]=sz+fz*fwd_dist;
 pose[15]=1.f;
 if(fabsf(fpv_pitch_deg)>0.01f){
  float local_right[3]={pose[0],pose[1],pose[2]};
  float rad=fpv_pitch_deg*0.0174532925f;
  for(int axis=0;axis<3;++axis)photo_rotate(pose+axis*4,local_right,rad);
 }
}
static void fpv_restore(void){
 if(!InterlockedCompareExchange(&fpv_active,0,0))return;
 cage_apply(0);
 if(fpv_owner&&fpv_owner_valid(fpv_owner)&&*(uintptr_t*)(fpv_owner+0xc8)==fpv_camera&&*(unsigned*)(fpv_owner+8)==1){
  set_mode(fpv_owner,fpv_prev_mode);
  set_pose(fpv_camera,fpv_saved_pose);
  *(float*)(fpv_camera+0xac)=fpv_saved_fov;
  *(unsigned char*)(fpv_owner+0xd8)=fpv_saved_flags[0];
  *(unsigned char*)(fpv_owner+0xd9)=fpv_saved_flags[1];
  *(unsigned char*)(fpv_owner+0xe0)=fpv_saved_flags[2];
 }
 fpv_owner=0;fpv_camera=0;InterlockedExchange(&fpv_active,0);
}
static int fpv_grapple_paused,fpv_has_last,fpv_last_target,fpv_pose_valid;
static float fpv_last_pos[3],fpv_smooth_pose[16],fpv_prev_vel[3],fpv_bob_phase,fpv_jolt_pitch,fpv_jolt_roll;
static ULONGLONG fpv_grapple_tick;
static void smooth_fpv_pose(const float *target,float dt,float *out_pose){
 if(!fpv_pose_valid){memcpy(fpv_smooth_pose,target,64);fpv_pose_valid=1;fpv_bob_phase=0.f;fpv_jolt_pitch=0.f;fpv_jolt_roll=0.f;memset(fpv_prev_vel,0,sizeof(fpv_prev_vel));memcpy(out_pose,fpv_smooth_pose,64);return;}
 if(!isfinite(dt)||dt<0.001f)dt=0.001f;
 if(dt>0.05f)dt=0.05f;
 static const float rates[]={18.f,14.f,22.f,45.f};
 float rate=rates[(fpv_wobble>=0&&fpv_wobble<=3)?fpv_wobble:0];
 float a=1.f-expf(-rate*dt);
 float prev_x=fpv_smooth_pose[12],prev_y=fpv_smooth_pose[13],prev_z=fpv_smooth_pose[14];
 for(int i=12;i<15;++i)fpv_smooth_pose[i]+=(target[i]-fpv_smooth_pose[i])*a;
 float fx=-fpv_smooth_pose[8]+(-target[8]-(-fpv_smooth_pose[8]))*a;
 float fy=-fpv_smooth_pose[9]+(-target[9]-(-fpv_smooth_pose[9]))*a;
 float fz=-fpv_smooth_pose[10]+(-target[10]-(-fpv_smooth_pose[10]))*a;
 float fl=sqrtf(fx*fx+fy*fy+fz*fz);
 if(fl<1e-4f){memcpy(fpv_smooth_pose,target,64);memcpy(out_pose,fpv_smooth_pose,64);return;}
 fx/=fl;fy/=fl;fz/=fl;
 float rx=-fz,ry=0.f,rz=fx;
 float rl=sqrtf(rx*rx+rz*rz);
 if(rl<1e-4f){rx=1.f;ry=0.f;rz=0.f;}else{rx/=rl;rz/=rl;}
 float ux=ry*fz-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy-ry*fx;
 fpv_smooth_pose[0]=rx;fpv_smooth_pose[1]=ry;fpv_smooth_pose[2]=rz;fpv_smooth_pose[3]=0.f;
 fpv_smooth_pose[4]=ux;fpv_smooth_pose[5]=uy;fpv_smooth_pose[6]=uz;fpv_smooth_pose[7]=0.f;
 fpv_smooth_pose[8]=-fx;fpv_smooth_pose[9]=-fy;fpv_smooth_pose[10]=-fz;fpv_smooth_pose[11]=0.f;
 fpv_smooth_pose[15]=1.f;
 memcpy(out_pose,fpv_smooth_pose,64);
 if(fpv_wobble>0){
  float vx=(fpv_smooth_pose[12]-prev_x)/dt,vy=(fpv_smooth_pose[13]-prev_y)/dt,vz=(fpv_smooth_pose[14]-prev_z)/dt;
  float dvx=vx-fpv_prev_vel[0],dvz=vz-fpv_prev_vel[2];
  fpv_prev_vel[0]=vx;fpv_prev_vel[1]=vy;fpv_prev_vel[2]=vz;
  float speed=sqrtf(vx*vx+vz*vz);if(speed>4.f)speed=4.f;
  fpv_bob_phase+=dt*(5.5f+speed*3.2f);
  if(fpv_bob_phase>628.3185f)fpv_bob_phase-=628.3185f;
  float bob_amp=fpv_wobble==1?0.018f:fpv_wobble==2?0.035f:0.045f;
  float roll_max=fpv_wobble==1?0.035f:fpv_wobble==2?0.075f:0.095f;
  float lat_vel=vx*rx+vz*rz;
  float roll=-lat_vel*(roll_max*0.45f);
  if(roll<-roll_max)roll=-roll_max;if(roll>roll_max)roll=roll_max;
  if(fpv_wobble>=2){
   float jolt_lat=(dvx*rx+dvz*rz)*0.008f,jolt_fwd=(dvx*fx+dvz*fz)*0.008f;
   fpv_jolt_roll+=jolt_lat;fpv_jolt_pitch+=jolt_fwd;
   if(fpv_jolt_roll<-0.12f)fpv_jolt_roll=-0.12f;if(fpv_jolt_roll>0.12f)fpv_jolt_roll=0.12f;
   if(fpv_jolt_pitch<-0.10f)fpv_jolt_pitch=-0.10f;if(fpv_jolt_pitch>0.10f)fpv_jolt_pitch=0.10f;
   float decay=expf(-10.f*dt);fpv_jolt_roll*=decay;fpv_jolt_pitch*=decay;
  }else{fpv_jolt_roll=0.f;fpv_jolt_pitch=0.f;}
  float move_gain=speed>0.15f?(speed/2.5f>1.f?1.f:speed/2.5f):0.08f;
  out_pose[13]+=sinf(fpv_bob_phase)*bob_amp*move_gain;
  out_pose[12]+=rx*cosf(fpv_bob_phase*0.5f)*(bob_amp*0.35f)*move_gain;
  out_pose[14]+=rz*cosf(fpv_bob_phase*0.5f)*(bob_amp*0.35f)*move_gain;
  float total_roll=roll+fpv_jolt_roll+sinf(fpv_bob_phase*0.5f)*(roll_max*0.25f)*move_gain;
  if(fabsf(total_roll)>1e-4f){float fwd_axis[3]={fx,fy,fz};for(int axis=0;axis<3;++axis)photo_rotate(out_pose+axis*4,fwd_axis,total_roll);}
  if(fabsf(fpv_jolt_pitch)>1e-4f){float right_axis[3]={out_pose[0],out_pose[1],out_pose[2]};for(int axis=0;axis<3;++axis)photo_rotate(out_pose+axis*4,right_axis,fpv_jolt_pitch);}
 }
}
static void fpv_step(uintptr_t o,float dt){
 int target=(int)InterlockedCompareExchange(&fpv_target,0,0);
 float head_m[16],opp_m[16];
 if(target!=fpv_last_target){fpv_has_last=0;fpv_pose_valid=0;fpv_last_target=target;}
 if(!target){fpv_grapple_paused=0;fpv_has_last=0;fpv_pose_valid=0;fpv_restore();InterlockedExchange(&fpv_diag_code,0);return;}
 if(!InterlockedCompareExchange(&focused,0,0)){fpv_pose_valid=0;fpv_restore();InterlockedExchange(&fpv_diag_code,2);return;}
 if(InterlockedCompareExchange(&fpv_active,0,0)&&o!=fpv_owner)return;
 if(!fpv_owner_valid(o)){if(o==fpv_owner){fpv_pose_valid=0;fpv_restore();}if(!fpv_active)InterlockedExchange(&fpv_diag_code,3);return;}
 if(!find_fight_head(target,head_m,opp_m)){
  fpv_request_scan();
  if(o==fpv_owner){fpv_pose_valid=0;fpv_restore();}
  if(!fpv_active)InterlockedExchange(&fpv_diag_code,4);
  return;
 }
 if(fpv_has_last){
  float d_self=(head_m[12]-fpv_last_pos[0])*(head_m[12]-fpv_last_pos[0])+(head_m[14]-fpv_last_pos[2])*(head_m[14]-fpv_last_pos[2]);
  float d_opp=(opp_m[12]-fpv_last_pos[0])*(opp_m[12]-fpv_last_pos[0])+(opp_m[14]-fpv_last_pos[2])*(opp_m[14]-fpv_last_pos[2]);
  if(d_opp+0.04f<d_self){
   float tmp[16];memcpy(tmp,head_m,64);memcpy(head_m,opp_m,64);memcpy(opp_m,tmp,64);
  }
 }
 fpv_last_pos[0]=head_m[12];fpv_last_pos[1]=head_m[13];fpv_last_pos[2]=head_m[14];fpv_has_last=1;
 fpv_cfg_load();
 if(fpv_grapple_dist>0.05f){
  float dx=opp_m[12]-head_m[12],dz=opp_m[14]-head_m[14];
  float d2=dx*dx+dz*dz;
  float exit_d=fpv_grapple_dist,resume_d=fpv_grapple_dist+0.20f;
  ULONGLONG now=PHOTO_NOW();
  if(!fpv_grapple_paused){
   if(d2<exit_d*exit_d){
    if(now-fpv_grapple_tick>150){
     fpv_grapple_paused=1;fpv_pose_valid=0;fpv_grapple_tick=now;
     if(o==fpv_owner)fpv_restore();
     return;
    }
   }else{
    fpv_grapple_tick=now;
   }
  }else{
   if(d2>resume_d*resume_d){
    if(now-fpv_grapple_tick>200){fpv_grapple_paused=0;fpv_has_last=0;fpv_pose_valid=0;fpv_grapple_tick=now;}
    else return;
   }else{
    fpv_grapple_tick=now;return;
   }
  }
 }else{
  fpv_grapple_paused=0;
 }
 if(!InterlockedCompareExchange(&fpv_active,0,0)){
  fpv_owner=o;fpv_camera=*(uintptr_t*)(o+0xc8);fpv_prev_mode=*(unsigned*)(o+8);
  memcpy(fpv_saved_pose,(void*)(fpv_camera+0x180),64);fpv_saved_fov=*(float*)(fpv_camera+0xac);
  fpv_saved_flags[0]=*(unsigned char*)(o+0xd8);fpv_saved_flags[1]=*(unsigned char*)(o+0xd9);fpv_saved_flags[2]=*(unsigned char*)(o+0xe0);
  if(fpv_saved_flags[1]||!valid_pose(fpv_saved_pose)||!isfinite(fpv_saved_fov)||fpv_saved_fov<20||fpv_saved_fov>120){
   fpv_owner=0;fpv_camera=0;InterlockedExchange(&fpv_diag_code,6);return;
  }
  *(unsigned char*)(o+0xd8)=1;*(unsigned char*)(o+0xe0)=1;set_mode(o,1);
  if(*(unsigned*)(o+8)!=1){fpv_owner=0;fpv_camera=0;InterlockedExchange(&fpv_diag_code,7);return;}
  InterlockedExchange(&fpv_active,1);
 }
 if(fpv_owner!=o||*(uintptr_t*)(o+0xc8)!=fpv_camera||*(unsigned*)(o+8)!=1){fpv_pose_valid=0;fpv_restore();return;}
 InterlockedExchange(&fpv_diag_code,8);
 fpv_poll_keys();
 float pose[16],out_pose[16];build_fpv_pose(head_m,opp_m,pose);
 smooth_fpv_pose(pose,dt,out_pose);
 set_pose(fpv_camera,out_pose);*(float*)(fpv_camera+0xac)=(float)fpv_fov_deg;
 cage_apply(1);
}
static void __attribute__((sysv_abi)) update(uintptr_t o,float dt){
 original(o,dt);
 if(TryAcquireSRWLockExclusive(&photo_guard)){
  if(InterlockedCompareExchange(&state,0,0)==2||InterlockedCompareExchange(&requested,0,0)!=0){
   fpv_pose_valid=0;fpv_restore();
   camera_update(o,dt);cage_apply(cage_active());
  }else{
   fpv_step(o,dt);
  }
  ReleaseSRWLockExclusive(&photo_guard);
 }
}
int photo_mode_stop_pending(void){return InterlockedCompareExchange(&state,0,0)==2&&(InterlockedCompareExchange(&commands,0,0)&PHOTO_STOP)!=0;}
int photo_mode_blocks_input(void){return (InterlockedCompareExchange(&state,0,0)==2&&!photo_mode_stop_pending())||InterlockedCompareExchange(&requested,0,0)!=0;}
void photo_mode_command(unsigned command){
 InterlockedOr(&commands,(LONG)command);
 if(command&PHOTO_STOP){
  InterlockedExchange(&hud_requested,0);
  AcquireSRWLockExclusive(&photo_guard);
  freeze_restore();if(requested){InterlockedExchange(&requested,0);InterlockedExchange(&state,3);}
  ReleaseSRWLockExclusive(&photo_guard);
 }
}
void photo_mode_set_fov(int degrees){if(degrees<20||degrees>120)return;InterlockedExchange(&target_fov,degrees);InterlockedOr(&commands,PHOTO_FOV);}
void photo_mode_snapshot(PhotoSnapshot *out){
 out->cage_count=0;out->cage_writes=InterlockedCompareExchange(&cage_writes,0,0);out->cage_restores=InterlockedCompareExchange(&cage_restores,0,0);
 if(TryAcquireSRWLockShared(&cage_guard)){for(int i=0;i<CAGE_PANEL_CAPACITY;++i)if(cage_panels[i].object)out->cage_count++;ReleaseSRWLockShared(&cage_guard);}
 out->cage_visible=(int)InterlockedCompareExchange(&cage_requested,0,0);out->cage_available=InterlockedCompareExchange(&cage_installed,0,0)==1;
 out->state=(int)InterlockedCompareExchange(&state,0,0);out->frozen=(int)InterlockedCompareExchange(&frozen,0,0);out->fov=0;out->hud_hidden=(int)InterlockedCompareExchange(&hud_requested,0,0);
 if(TryAcquireSRWLockShared(&photo_guard)){out->fov=(int)(current_fov+.5f);ReleaseSRWLockShared(&photo_guard);}
}
void photo_mode_poll(int focus,int menu_open,const PhotoInput *input){
 InterlockedExchange(&focused,focus);InterlockedExchange(&hud_menu_open,menu_open);InterlockedExchange64(&hud_poll_tick,(LONG64)PHOTO_NOW());
 if(!focus)InterlockedExchange(&hud_requested,0);
 if(!focus)InterlockedOr(&commands,PHOTO_STOP);
 if(TryAcquireSRWLockExclusive(&photo_guard)){
  memset(&published_input,0,sizeof(published_input));if(focus&&!menu_open&&input)published_input=*input;
  input_tick=PHOTO_NOW();
  if(!focus){freeze_restore();if(requested){InterlockedExchange(&requested,0);InterlockedExchange(&state,3);}}
  if(requested&&PHOTO_NOW()>=deadline){InterlockedExchange(&requested,0);InterlockedExchange(&state,-5);}
  ReleaseSRWLockExclusive(&photo_guard);
 }
}
static void ensure_fpv_hooks(void);
void photo_mode_fpv_set(int target){
 if(target<0||target>2)target=0;
 ensure_fpv_hooks();
 InterlockedExchange(&fpv_target,target);
 if(target)fpv_request_scan();
}
int photo_mode_fpv_get(void){return (int)InterlockedCompareExchange(&fpv_target,0,0);}
int photo_mode_fpv_diag(void){return (int)InterlockedCompareExchange(&fpv_diag_code,0,0);}
void photo_mode_fpv_adjust(float pitch_deg,float height_offset_m,float fwd_offset_m,int fov_deg,float grapple_dist_m){
 fpv_pitch_deg=pitch_deg;
 fpv_height_m=height_offset_m;
 fpv_fwd_m=fwd_offset_m;
 if(fov_deg>=60&&fov_deg<=110)fpv_fov_deg=fov_deg;
 if(grapple_dist_m>=0.f&&grapple_dist_m<=1.2f)fpv_grapple_dist=grapple_dist_m;
 fpv_cfg_loaded=1;
 fpv_cfg_save();
}
void photo_mode_fpv_get_tune(float *pitch_deg,float *height_offset_m,float *fwd_offset_m,int *fov_deg,float *grapple_dist_m){
 fpv_cfg_load();
 if(pitch_deg)*pitch_deg=fpv_pitch_deg;
 if(height_offset_m)*height_offset_m=fpv_height_m;
 if(fwd_offset_m)*fwd_offset_m=fpv_fwd_m;
 if(fov_deg)*fov_deg=fpv_fov_deg;
 if(grapple_dist_m)*grapple_dist_m=fpv_grapple_dist;
}
static void ensure_fpv_hooks(void){
#if !defined(NATIVE_CAMERA_TEST) && !defined(QUIT_MENU_TEST)
 if(InterlockedCompareExchange(&skel_installed,0,0))return;
 int profile=engine_fingerprint();if(profile!=1&&profile!=2&&profile!=3)return;
 set_mode=(Mode)0x902a84520ULL;set_pose=(Pose)0x902b85580ULL;
 const unsigned char expected[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x18,0x03,0,0};
 ExactHook hooks[]={
  {(unsigned char*)0x902a83c20ULL,(void*)update,(void**)&original,expected,sizeof(expected)}
 };
 const char *error=NULL;
 if(install_exact_hooks(hooks,1,&error))InterlockedExchange(&skel_installed,1);
#else
 (void)skel_installed;
#endif
}
int photo_mode_start(int fight_ready){
 if(photo_mode_stop_pending())return 0;
 if(photo_mode_blocks_input()){photo_mode_command(PHOTO_STOP);return 1;}
 if(!fight_ready)return 0;
#ifndef NATIVE_CAMERA_TEST
 ensure_fpv_hooks();
 if(!installed){
#ifdef QUIT_MENU_TEST
  (void)update;(void)hud_draw_a;(void)hud_draw_b;return 0;
#else
  int profile=engine_fingerprint();if(profile!=1&&profile!=2&&profile!=3)return PHOTO_START_BUILD;
  set_mode=(Mode)0x902a84520ULL;set_pose=(Pose)0x902b85580ULL;
  const unsigned char draw_a[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x68};
  const unsigned char draw_b[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x78};
  ExactHook hud_hooks[]={
   {(unsigned char*)0x905223f20ULL,(void*)hud_draw_a,(void**)&hud_originals[0],draw_a,sizeof(draw_a)},
   {(unsigned char*)0x905224c70ULL,(void*)hud_draw_b,(void**)&hud_originals[1],draw_b,sizeof(draw_b)}};const char *error=NULL;
  if(!install_exact_hooks(hud_hooks,2,&error))return PHOTO_START_HOOKS;
  InterlockedExchange(&installed,1);
#endif
 }
#endif
 AcquireSRWLockExclusive(&photo_guard);
 deadline=PHOTO_NOW()+1800000;input_tick=PHOTO_NOW();InterlockedExchange(&commands,0);InterlockedExchange(&requested,1);
 ReleaseSRWLockExclusive(&photo_guard);return 1;
}
