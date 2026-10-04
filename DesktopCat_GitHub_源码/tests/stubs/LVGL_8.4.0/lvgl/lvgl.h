#pragma once
#include <string>
#include <vector>
#define LV_OPA_COVER 255
#define LV_OBJ_FLAG_SCROLLABLE 1
#define LV_OBJ_FLAG_HIDDEN 2
#define LV_ALIGN_CENTER 0
#define LV_ALIGN_BOTTOM_MID 1
struct lv_obj_t{int w=0,h=0,x=0,y=0;unsigned flags=0;std::string text;};
inline std::vector<lv_obj_t*> allocated;
inline lv_obj_t screen;
inline lv_obj_t* lv_scr_act(){return &screen;}
inline lv_obj_t* lv_obj_create(lv_obj_t*){auto *o=new lv_obj_t;allocated.push_back(o);return o;}
inline lv_obj_t* lv_label_create(lv_obj_t*p){return lv_obj_create(p);}
inline void lv_obj_remove_style_all(lv_obj_t*){}
inline void lv_obj_set_size(lv_obj_t*o,int w,int h){o->w=w;o->h=h;}
inline void lv_obj_set_height(lv_obj_t*o,int h){o->h=h;}
inline void lv_obj_set_pos(lv_obj_t*o,int x,int y){o->x=x;o->y=y;}
inline uint32_t lv_color_hex(uint32_t x){return x;}
inline uint32_t lv_color_black(){return 0;}
inline void lv_obj_set_style_bg_color(lv_obj_t*,uint32_t,int){}
inline void lv_obj_set_style_bg_opa(lv_obj_t*,int,int){}
inline void lv_obj_set_style_radius(lv_obj_t*,int,int){}
inline void lv_obj_set_style_text_color(lv_obj_t*,uint32_t,int){}
inline void lv_obj_clear_flag(lv_obj_t*o,unsigned f){o->flags&=~f;}
inline void lv_obj_add_flag(lv_obj_t*o,unsigned f){o->flags|=f;}
inline void lv_obj_center(lv_obj_t*){}
inline void lv_obj_align(lv_obj_t*,int,int,int){}
inline void lv_label_set_text(lv_obj_t*o,const char*t){o->text=t;}
