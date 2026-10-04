#pragma once
struct bsp_display_lvgl_partial_cfg_t{bool use_psram;bool double_buffer;int buffer_height;};
inline void bsp_display_start_partial(const bsp_display_lvgl_partial_cfg_t*){}
inline void bsp_display_brightness_set(int){}
inline bool bsp_display_lock(int){return true;}
inline void bsp_display_unlock(){}
