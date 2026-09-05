#include "../../ui.h"
#include "../../ui_helpers.h"
#include "../Inc/ui_HomePage.h"
#include "../../../Func/Inc/HWDataAccess.h"
#include <math.h>

#define PI 3.14159265358979323846  // 宏定义π

lv_obj_t * ui_HomePage;
static lv_timer_t* arc_timer;
static uint16_t arc_current_angle = 0;
int flag = 0;
static lv_obj_t* line; // 直线对象指针

uint16_t start_x = 0;
uint16_t start_y = 0;
uint16_t end_x = 0;
uint16_t end_y = 0;

float jiaodu = 0;
uint16_t changdu = 0;
uint16_t short_changdu = 30;
// 定时器回调函数
void arc_rotation_timer_cb(lv_timer_t * timer)
{
    lv_obj_t* arc = timer->user_data;
    if(arc_current_angle == 0) {
        flag = 0;
    }
    else if(arc_current_angle >= 250) { 
        flag = 1;
    }
    
    if(flag == 0) {
        arc_current_angle += 2;
        jiaodu += PI/90;
    }
    else if(flag == 1) {
        arc_current_angle -= 2;
        jiaodu -= PI/90;
    }

    // 设置背景圆弧的角度范围
    lv_arc_set_bg_angles(arc, 0, arc_current_angle);
}

void ui_HomePage_screen_init(void)
{
    // 重置圆弧角度
    arc_current_angle = 0;
    flag = 0;
    
    ui_HomePage = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_HomePage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(ui_HomePage);
    lv_obj_set_size(ui_HomePage, 240, 280);

    // 创建背景图片
    lv_obj_t* ui_desktop_wallpaper_Page = lv_img_create(ui_HomePage);
    lv_img_set_zoom(ui_desktop_wallpaper_Page, LV_IMG_ZOOM_NONE);
    lv_obj_set_size(ui_desktop_wallpaper_Page, 240, 280);
    lv_img_set_src(ui_desktop_wallpaper_Page, &desktop_wallpaper);
    lv_obj_align(ui_desktop_wallpaper_Page, LV_ALIGN_TOP_LEFT, 0, 0);

    // 创建圆弧组件
    lv_obj_t* arc = lv_arc_create(ui_HomePage);
    lv_obj_set_size(arc, 210, 260);
    lv_obj_align_to(arc, ui_desktop_wallpaper_Page, LV_ALIGN_TOP_LEFT, 18, 30);
    
    // 设置圆弧初始状态
    lv_arc_set_rotation(arc, 90);  // 设置0度位置在顶部
    lv_arc_set_bg_angles(arc, 0, 0);
    
    // 设置圆弧样式
    lv_obj_set_style_arc_width(arc, 4, 0);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_arc_rounded(arc, 0, 0);
    
    // 禁用交互功能
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    
    // 移除不需要的部件
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_INDICATOR);

    // 创建定时器
    arc_timer = lv_timer_create(arc_rotation_timer_cb, 20, arc); // 
}

void ui_HomePage_screen_deinit(void)
{

}

Page_t Page_Home = {ui_HomePage_screen_init, ui_HomePage_screen_deinit, &ui_HomePage};
