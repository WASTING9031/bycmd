#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace camera_override_core {
// A single live field: engine changes become the latest baseline, never restored over.
struct scalar {uint64_t object=0;float original=0,last=0;bool written=false;};
template<class R,class W> bool release(R& r,W& w,scalar& e){
 if(!e.written){e={};return true;}float v=0;if(!r(e.object+0x48,&v,4))return false;
 if(v==e.last&&!w(e.object+0x48,&e.original,4))return false;e={};return true;
}
template<class R,class W> bool apply(R& r,W& w,scalar& e,uint64_t object,float fov){
 if(!std::isfinite(fov)||fov<30||fov>140)return false;
 if(e.object&&e.object!=object)return false;float cur=0;
 if(!r(object+0x48,&cur,4)||!std::isfinite(cur)||cur<1||cur>179)return false;
 if(!e.object)e.object=object;
 if(!e.written||cur!=e.last)e.original=cur;
 if(cur!=fov&&!w(object+0x48,&fov,4))return false;e.last=fov;e.written=true;return true;
}
// Native Camera::set_projectionMatrix (0x5ED6E4C). Only horizontal focal scalars
// are changed, not dynamic view matrices. Support symmetric implicit perspective only.

struct aspect_edit {
 uint64_t native=0;int32_t id=0;float ratio[2]{},original_y[2]{},original_fov=0,last_x[2]{},last_y[2]{};
 bool wrote[2]{},mode_written=false,flag_written=false,dirty_written=false;uint8_t flag=0;
};
inline float focal(float degrees){return 1.f/std::tan(degrees*.00872664625997165f);}
template<class R> bool same(R& r,const aspect_edit& e){int32_t id=0;return e.native&&r(e.native+8,&id,4)&&id==e.id;}
template<class R,class W> bool restore_aspect(R& r,W& w,aspect_edit& e){
 if(!e.native)return true;if(!same(r,e)){e={};return false;}
 uint32_t mode=99;if(!r(e.native+0x504,&mode,4))return false;
 if(e.mode_written&&mode!=0){e={};return true;}
 // An engine-authored/custom projection replaces our ownership. Never force its mode.
 for(int i=0;i<2;++i)if(e.wrote[i]){float x=0,y=0;const auto a=e.native+(i?0x130:0xB0);
  if(!r(a,&x,4)||!r(a+20,&y,4))return false;
  if(x!=e.last_x[i]||y!=e.last_y[i]){e={};return true;}}
 float fov=0;if(!r(e.native+0x170,&fov,4)||!std::isfinite(fov)||fov<1||fov>179)return false;
 for(int i=0;i<2;++i)if(e.wrote[i]){const auto a=e.native+(i?0x130:0xB0);
  const float y=fov==e.original_fov?e.original_y[i]:focal(fov),x=y/e.ratio[i];
  // Record each partial restore so a transient write failure can be retried.
  if(!w(a,&x,4))return false;e.last_x[i]=x;
  if(!w(a+20,&y,4))return false;e.last_y[i]=y;e.wrote[i]=false;
 }
 if(e.flag_written){uint8_t flag=255;if(!r(e.native+0x508,&flag,1))return false;if(flag==0&&!w(e.native+0x508,&e.flag,1))return false;e.flag_written=false;}
 if(e.mode_written){const uint32_t implicit=1;if(!w(e.native+0x504,&implicit,4))return false;e.mode_written=false;
  // Camera::set_fieldOfView marks the projection/inverse caches dirty with 0x0101.
  const uint16_t dirty=0x0101;if(!w(e.native+0x500,&dirty,2)){e.dirty_written=true;return false;}}
 if(e.dirty_written){const uint16_t dirty=0x0101;if(!w(e.native+0x500,&dirty,2))return false;}
 e={};return true;
}
template<class R,class W> bool apply_aspect(R& r,W& w,aspect_edit& e,uint64_t native,float aspect){
 if(!std::isfinite(aspect)||aspect<.75f||aspect>3)return false;
 if(e.native&&e.native!=native)return false;
 uint32_t mode=99;float m[2][16]{},fov=0;int32_t id=0;uint8_t flag=255;
 if(!r(native+8,&id,4)||!id||!r(native+0x504,&mode,4)||!r(native+0x508,&flag,1)||flag>1||!r(native+0xB0,m[0],64)||!r(native+0x130,m[1],64)||
 !r(native+0x170,&fov,4)||!std::isfinite(fov)||fov<1||fov>179)return false;
 if(mode!=(e.mode_written?0u:1u))return false;
 for(auto& a:m){for(float v:a)if(!std::isfinite(v))return false;
  if(a[0]<.05f||a[0]>20||a[5]<.05f||a[5]>20||std::fabs(a[11])<.5f||std::fabs(a[15])>.01f||a[2]!=0||a[6]!=0||a[8]!=0||a[9]!=0)return false;}
 // Implicit mode must be the standard native FOV perspective, not another custom camera.
 const float fy=focal(fov);if(!std::isfinite(fy)||fy<.05f||fy>20)return false;
 if(!e.native){for(int i=0;i<2;++i)if(std::fabs(m[i][5]-fy)>fy*.01f)return false;
  e.native=native;e.id=id;e.flag=flag;e.original_fov=fov;for(int i=0;i<2;++i){e.ratio[i]=m[i][5]/m[i][0];e.original_y[i]=m[i][5];}}
 if(!same(r,e))return false;
 for(int i=0;i<2;++i)if(e.wrote[i]&&(m[i][0]!=e.last_x[i]||m[i][5]!=e.last_y[i]))return false;
 // Rebuild from the LIVE native FOV every frame. Explicit projection must not freeze scope zoom.
 for(int i=0;i<2;++i){const float y=fov==e.original_fov?e.original_y[i]:fy,x=y/aspect;const auto a=native+(i?0x130:0xB0);
  if(!e.wrote[i]){e.last_x[i]=m[i][0];e.last_y[i]=m[i][5];e.wrote[i]=true;}
  if(m[i][0]!=x&&!w(a,&x,4))return false;e.last_x[i]=x;
  if(m[i][5]!=y&&!w(a+20,&y,4))return false;e.last_y[i]=y;
 }
 const uint8_t no=0;if(!w(native+0x508,&no,1))return false;e.flag_written=true;
 const uint32_t explicit_mode=0;if(!w(native+0x504,&explicit_mode,4))return false;e.mode_written=true;return true;
}
}
