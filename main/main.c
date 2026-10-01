// main/main.c —— PasPet（拓麻歌子风电子宠物）应用入口。
//
// 三键语义（pet_app/pet_input.h 的交互宪法）：
//   上/下 短按   图标坞移动 / 列表选择 / 游戏选大小
//   确定  短按   确认 / 进入
//   确定  长按   返回上级；主屏关灯快捷；游戏内二次长按退出
//   确定  双击   主屏进状态页
//
// 本文件只做 BSP 初始化与任务装配，全部玩法逻辑在 main/pet_core 与 main/pet_app。
// UI 为宠物应用原创，未复用官方 demo 菜单 / demo_* 页面 / ui_pixel 外壳。
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "pet_app.h"
#include "pet_input.h"

static const char *TAG = "main";

#define INPUT_QUEUE_DEPTH 8

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;

// 按键回调跑在共享 esp_timer 任务上：只入队、绝不阻塞（AGENTS.md 硬约束）。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void) user;
    if (s_input_queue == NULL) {
        return;
    }
    const input_event_t input = { .btn = btn, .event = ev };
    (void) xQueueSend(s_input_queue, &input, 0);
}

static void input_task(void *arg)
{
    (void) arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            // BSP 与 pet_input 枚举数值一一对应（UP/DOWN/OK × PRESS..LONG），
            // 见 bsp_button.h 与 pet_input.h。
            pet_app_button((pet_btn_t) input.btn, (pet_ev_t) input.event);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "PasPet boot");

    // I2C 总线先就位；各外设（codec / 电量计）在各自 init 里按需探测。
    // 注意：开机不调用 bsp_i2c_scan() —— 那是调试工具：它顺序 probe 112 个
    // 地址，设备缺失时每个地址都要等超时，会把 app_main 拉长到任务看门狗
    // 阈值以上（模拟器无 I2C 设备时必然触发），真机冷启动也白白浪费上百毫秒。
    bsp_i2c_init();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG,
                 "display/LVGL init failed: MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC,
                 BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (s_input_queue == NULL
        || xTaskCreate(input_task, "pet_input", 4096, NULL, 5, NULL)
               != pdPASS) {
        ESP_LOGE(TAG, "input task/queue init failed");
        return;
    }

    esp_err_t button_err = bsp_button_init(on_key, NULL);
    if (button_err != ESP_OK) {
        ESP_LOGW(TAG, "button init failed: %s", esp_err_to_name(button_err));
    }

    // 音频 / 电量计失败不阻塞启动：音频静默降级，电量显示 "--"。
    bool audio_ok = (bsp_audio_init() == ESP_OK);
    bool battery_ok = (bsp_battery_init() == ESP_OK);

    // pet_app_start 内部：NVS 读档/新蛋 → 时钟锚定 → 引擎/音频任务 → 创建房间 UI。
    pet_app_start(audio_ok);

    ESP_LOGI(TAG, "ready: audio=%d battery=%d", audio_ok, battery_ok);
}
