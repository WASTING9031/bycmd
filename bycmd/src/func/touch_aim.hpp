#pragma once
// Experimental external input via Linux uinput; no writes into the game process.
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"
#include "../other/diag.hpp"
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include <cerrno>
namespace touch_aim {
inline const char* status="off";
inline char last_active_status[128]="not yet tested with menu closed";
inline std::atomic<long long> foreground_at{0};
inline std::atomic<bool> monitor_started{false};
// Fail closed on errors or stale results. This is app-focus checking, NOT wall visibility.
inline void start_focus_monitor(){
 bool expected=false;if(!monitor_started.compare_exchange_strong(expected,true))return;
 std::thread([]{
  for(;;){
   bool current=false,focus_line=false,focus_game=false,app_line=false,app_game=false;
   FILE* f=popen("dumpsys window windows 2>/dev/null", "r");
   if(f){char line[2048];while(fgets(line,sizeof line,f)){
     if(std::strstr(line,"mCurrentFocus=")){focus_line=true;if(std::strstr(line,"com.axlebolt.standoff2"))focus_game=true;if(std::strstr(line,"com.axlebolt.standoff2/"))current=true;}
     if(std::strstr(line,"mFocusedApp=")){app_line=true;if(std::strstr(line,"com.axlebolt.standoff2"))app_game=true;}
    }
     int rc=pclose(f);if(rc!=0){current=false;static long long last=0;if(diag::every(last,5000))diag::log("TOUCH_AIM","focus query failed: rc=%d",rc);}
     if(rc==0&&!current){static long long last=0;if(diag::every(last,5000))diag::log("FOCUS_PROBE","focus_line=%d focus_game=%d app_line=%d app_game=%d",int(focus_line),int(focus_game),int(app_line),int(app_game));}
   }
   else {static long long last=0;if(diag::every(last,5000))diag::log("TOUCH_AIM","focus query could not start: errno=%d",errno);}
   foreground_at.store(current?diag::now_ms():0,std::memory_order_release);
   std::this_thread::sleep_for(std::chrono::milliseconds(600));
  }
 }).detach();
}
inline bool focused(long long now){auto t=foreground_at.load(std::memory_order_acquire);return t>0&&now>=t&&now-t<1800;}
struct Candidate {bool valid=false;float dx=0,dy=0,dist2=0;uint64_t id=0;};
inline Candidate candidate;
inline void begin_frame(){candidate={};}
inline void offer(uint64_t id,float x,float y,float width,float height,float fov){
 if(!id||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(fov)||width<=0||height<=0||x<0||y<0||x>width||y>height)return;
 const float dx=x-width*.5f,dy=y-height*.5f,d2=dx*dx+dy*dy,r=std::clamp(fov,10.f,360.f);
 if(d2>r*r)return;if(!candidate.valid||d2<candidate.dist2)candidate={true,dx,dy,d2,id};
}
struct Point{int x,y;};
inline Point native_point(float x,float y,int w,int h,int orientation){
 x=std::clamp(x,0.f,float(w-1));y=std::clamp(y,0.f,float(h-1));
 switch(orientation){
  case 1:return {int(std::round(h-1-y)),int(std::round(x))};
  case 2:return {int(std::round(w-1-x)),int(std::round(h-1-y))};
  case 3:return {int(std::round(y)),int(std::round(w-1-x))};
  default:return {int(std::round(x)),int(std::round(y))};
 }
}
class Device {
 int fd=-1,w=0,h=0,orient=0,tracking=1;bool down=false;float x=0,y=0;long long ready=0,last=0;
 bool emit(unsigned short type,unsigned short code,int value){input_event e{};e.type=type;e.code=code;e.value=value;return ::write(fd,&e,sizeof e)==sizeof e;}
 bool event_pos(){auto p=native_point(x,y,w,h,orient);return emit(EV_ABS,ABS_MT_SLOT,0)&&emit(EV_ABS,ABS_MT_POSITION_X,p.x)&&emit(EV_ABS,ABS_MT_POSITION_Y,p.y)&&emit(EV_ABS,ABS_MT_PRESSURE,128)&&emit(EV_ABS,ABS_X,p.x)&&emit(EV_ABS,ABS_Y,p.y)&&emit(EV_ABS,ABS_PRESSURE,128);}
public:
 ~Device(){close_device();}
 void close_device(){if(fd>=0){release();if(fd>=0){ioctl(fd,UI_DEV_DESTROY);::close(fd);fd=-1;}}down=false;}
 void release(){if(fd<0||!down)return;
  bool ok=emit(EV_ABS,ABS_MT_SLOT,0)&&emit(EV_ABS,ABS_MT_TRACKING_ID,-1)&&emit(EV_ABS,ABS_PRESSURE,0)&&emit(EV_KEY,BTN_TOUCH,0)&&emit(EV_KEY,BTN_TOOL_FINGER,0)&&emit(EV_SYN,SYN_REPORT,0);
  down=false;if(!ok){ioctl(fd,UI_DEV_DESTROY);::close(fd);fd=-1;}
 }
 bool init(int width,int height,int orientation,long long now){
  if(width<100||height<100||width>10000||height>10000||orientation<0||orientation>3)return false;
  if(fd>=0&&(w!=width||h!=height||orient!=orientation))close_device();
  if(fd>=0)return true;
  int f=::open("/dev/uinput",O_WRONLY|O_NONBLOCK|O_CLOEXEC);if(f<0)f=::open("/dev/input/uinput",O_WRONLY|O_NONBLOCK|O_CLOEXEC);if(f<0){static long long last=0;if(diag::every(last,5000))diag::log("TOUCH_AIM", "cannot open uinput: errno=%d",errno);return false;}fd=f;w=width;h=height;orient=orientation;
  auto set=[&](unsigned long req,int val){return ioctl(fd,req,val)>=0;};
  bool ok=set(UI_SET_EVBIT,EV_SYN)&&set(UI_SET_EVBIT,EV_KEY)&&set(UI_SET_KEYBIT,BTN_TOUCH)&&set(UI_SET_KEYBIT,BTN_TOOL_FINGER)&&set(UI_SET_EVBIT,EV_ABS);
  for(int a:{ABS_X,ABS_Y,ABS_PRESSURE,ABS_MT_SLOT,ABS_MT_TRACKING_ID,ABS_MT_POSITION_X,ABS_MT_POSITION_Y,ABS_MT_PRESSURE})ok=ok&&set(UI_SET_ABSBIT,a);
#ifdef UI_SET_PROPBIT
  ok=ok&&set(UI_SET_PROPBIT,INPUT_PROP_DIRECT);
#endif
  uinput_user_dev dev{};std::snprintf(dev.name,sizeof dev.name,"bycmd touch input");dev.id.bustype=BUS_VIRTUAL;dev.id.vendor=1;dev.id.product=2;dev.id.version=1;
  const int maxx=(orient==1||orient==3?h:w)-1,maxy=(orient==1||orient==3?w:h)-1;
  dev.absmax[ABS_X]=dev.absmax[ABS_MT_POSITION_X]=maxx;dev.absmax[ABS_Y]=dev.absmax[ABS_MT_POSITION_Y]=maxy;
  dev.absmax[ABS_MT_SLOT]=0;dev.absmax[ABS_MT_TRACKING_ID]=65535;dev.absmax[ABS_PRESSURE]=dev.absmax[ABS_MT_PRESSURE]=255;
  ok=ok&&(::write(fd,&dev,sizeof dev)==sizeof dev)&&ioctl(fd,UI_DEV_CREATE)>=0;
  if(!ok){static long long last_error=0;if(diag::every(last_error,5000))diag::log("TOUCH_AIM","uinput setup/create failed: errno=%d",errno);::close(fd);fd=-1;return false;}ready=now+1000;last=0;return true;
 }
 bool step(float anchor_x,float anchor_y,float dx,float dy,float smooth,float gain,long long now){
  if(now<ready)return false;if(last&&now-last<16)return true;float dt=last?std::clamp(float(now-last)/1000.f,.016f,.05f):.016f;last=now;
  const float ax=std::clamp(anchor_x,0.05f,.95f)*(w-1),ay=std::clamp(anchor_y,.05f,.95f)*(h-1);
  if(!down){x=ax;y=ay;tracking=tracking%65534+1;
   if(!(emit(EV_ABS,ABS_MT_SLOT,0)&&emit(EV_ABS,ABS_MT_TRACKING_ID,tracking)&&event_pos()&&emit(EV_KEY,BTN_TOUCH,1)&&emit(EV_KEY,BTN_TOOL_FINGER,1)&&emit(EV_SYN,SYN_REPORT,0))){close_device();return false;}down=true;return true;}
  float a=std::clamp(smooth,.025f,1.f);a=1.f-std::pow(1.f-a,dt*60.f);float vx=std::clamp(dx*a*gain,-12.f,12.f),vy=std::clamp(dy*a*gain,-12.f,12.f);
  float nx=x+vx,ny=y+vy;float range=std::min(100.f,std::min(float(w),float(h))*.16f);
  if(std::fabs(nx-ax)>range||std::fabs(ny-ay)>range||nx<2||nx>w-3||ny<2||ny>h-3){release();return true;}
  x=nx;y=ny;if(!(event_pos()&&emit(EV_SYN,SYN_REPORT,0))){close_device();return false;}return true;
 }
};
inline Device device;
inline void finish(bool game_ready,bool menu_open,int orientation){
 struct Report {bool menu;int orientation;~Report(){
  static char previous[160]=""; static long long last=0; auto now=diag::now_ms();
  if(cfg::legit::enabled){
   if(!menu)std::snprintf(last_active_status,sizeof last_active_status,"%s",status);
   if(std::strcmp(previous,status)!=0||now-last>=5000){
    std::snprintf(previous,sizeof previous,"%s",status);last=now;
    diag::log("TOUCH_AIM","status=%s target=%d dx=%.1f dy=%.1f menu=%d move_point=%d visible_check=%d orientation=%d",status,candidate.valid?1:0,candidate.dx,candidate.dy,menu?1:0,cfg::legit::edit_position?1:0,cfg::legit::visible_check?1:0,orientation);
   }
  }
 }} report{menu_open,orientation};
 if(!cfg::legit::enabled){device.release();status="off";return;}
 start_focus_monitor();
 if(!cfg::style::overlay_enabled||!game_ready||menu_open||cfg::legit::edit_position){device.release();status="paused: game/menu";return;}
 if(cfg::legit::visible_check){device.release();status="blocked: visible check unavailable";return;}
 const long long now=diag::now_ms();if(!focused(now)){device.release();status="waiting for verified game focus";return;}
 if(!candidate.valid){device.release();status="no target in FOV";return;}
 if(!std::isfinite(cfg::legit::point_x)||!std::isfinite(cfg::legit::point_y)||!std::isfinite(cfg::legit::smooth)||!std::isfinite(cfg::legit::gain)){device.release();status="invalid settings";return;}
 if(!device.init(int(g_sw),int(g_sh),orientation,now)){status="uinput unavailable (root/device policy)";return;}
 status=device.step(cfg::legit::point_x,cfg::legit::point_y,candidate.dx,candidate.dy,cfg::legit::smooth,cfg::legit::gain,now)?"touch sent (device unverified)":"waiting for touch device";
}
}
