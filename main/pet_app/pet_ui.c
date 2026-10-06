#include "pet_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "lvgl.h"

#include "pet_app.h"
#include "pet_audio.h"
#include "pet_clock.h"
#include "pet_decor.h"
#include "pet_dock.h"
#include "pet_game.h"
#include "pet_games2.h"
#include "pet_jobs.h"
#include "pt_config.h"
#include "pt_evolve.h"
#include "pt_compose.h"
#include "pet_parts_data.h"
#include "pet_ui_icons.h"



// ---------------------------------------------------------------------------
// 配色（马卡龙底色 + 深描边，designs 10 §2）
// ---------------------------------------------------------------------------

#define COL_BG       0xFFF6E6
#define COL_ROOM     0xDDF3E4
#define COL_ROOM_TOP 0xEAF9EE
#define COL_FLOOR    0xC6E6CC
#define COL_FLOOR_EDGE 0xA8D8B2
#define COL_RUG      0xB9DCC0
#define COL_SHADOW   0x9FCFA9
#define COL_CREAM    0xFFF6E6
#define COL_DOCK_BAND 0xF3E4CE
#define COL_DOCK_EDGE 0xD1C3B2
#define COL_PILL_PINK 0xFFE3EC
#define COL_PILL_BLUE 0xE7F0FF
#define COL_INK      0x4A3F45
#define COL_PANEL    0xFFFBF2
#define COL_DIM      0x3A2F2A
#define COL_GREEN    0x55B96B
#define COL_YELLOW   0xF2B134
#define COL_RED      0xE05B5B
#define COL_BLUE     0x5B8DE0
#define COL_BROWN    0x9C7A4E
#define COL_PIP_OFF  0xE3D8C2
#define COL_SEL      0xE17FA2
#define COL_DOCK_BG  0xF2E2C6
// 夜间房间（定稿 mockup 不透明色，避免 alpha 整屏合成）
#define COL_NIGHT_WALL   0x34386B
#define COL_NIGHT_FLOOR  0x222547
#define COL_NIGHT_EDGE   0x3A3E73
#define COL_NIGHT_RUG    0x191B38
#define COL_NIGHT_SHADOW 0x14152E
#define COL_NIGHT_STAR   0xFFF6C8

// PV2 统一卡片弹层色板（定稿 pv2-cards.html：奶白卡 + 暖墨描边 + 马卡龙）
#define COL_CARD_WHITE   0xFFFFFF
#define COL_CARD_PINK    0xFFF3F8   // 选中卡粉底
#define COL_CARD_GIFT    0xFFE9A8   // 签到礼品卡黄底
#define COL_CARD_GREENBG 0xE7F8E9   // Style / OWNED 浅绿底
#define COL_HAIR         0xE5E0DA   // 页眉发丝线（墨色 14% 落奶白）
#define COL_SUB          0x9A908A   // 卡片副标签灰
#define COL_HINT         0xB4A99E   // hold OK 提示灰
#define COL_COIN_BG      0xFFE9A8
#define COL_BADGE_BLUE   0xE7F0FF   // 背包数量徽章
#define COL_GREEN_DEEP   0x3E9B52
#define COL_DIS_BG       0xEFE9DC   // 失效翻页键
#define COL_DIS_LINE     0xD1C7B8
#define COL_DIS_TX       0xC3B8AA
#define COL_TAB_ON       0xF3A0BC   // 页脚激活段粉底
#define COL_PLAY_SWEET   0xDFF5E2   // G2 判定条中心甜区

#define LCD_W BSP_LCD_W
#define LCD_H BSP_LCD_H

// ---------------------------------------------------------------------------
// 界面模式与瞬态
// ---------------------------------------------------------------------------

typedef enum {
    MODE_ROOM = 0,
    MODE_LIST,      // 喂食 / 设置 通用列表弹层
    MODE_STATUS,    // 状态分页
    MODE_GAME,      // G1 猜大小
    MODE_GAMES,     // 游戏选择器（G1–G6）
    MODE_GX,        // G2–G6 运行时
    MODE_JOB,       // 成年职业办公室
    MODE_AGENCY,    // 职介所
    MODE_SHOP,      // 商店 / 背包（P1 经济）
    MODE_DECOR,     // 换装 / 家具 / 主题（P1-S4）
    MODE_CHECKIN,   // 每日签到弹层
    MODE_MATE,      // 婚介/家庭（P2-S3a）
    MODE_DEX,       // 图鉴（P2-S4）
    MODE_OVERLAY,   // 孵化 / 进化全屏演出（锁输入）
    MODE_MEMORIAL,  // 死亡纪念
} ui_mode_t;

enum { LIST_FOOD = 0, LIST_SETTINGS };
enum { MOOD_NORMAL = 0, MOOD_HAPPY, MOOD_EAT };
enum { PHASE_CHOOSE = 0, PHASE_RESULT };
enum { EFF_NONE = 0, EFF_HATCH, EFF_EVOLVE };

typedef struct {
    lv_obj_t *box;
    lv_obj_t *img;
} dock_widget_t;

static lv_obj_t *s_scr;
static lv_timer_t *s_timer;

// 顶栏
static lv_obj_t *s_top_stage;
static lv_obj_t *s_clock;
static lv_obj_t *s_celestial;          // 日月 12px 图片
static lv_obj_t *s_battery;

// 微状态行（PV1 象形版：碗 + 4 心 / 笑 + 4 心 + 体重胶囊 + 羁绊心）
static lv_obj_t *s_heart_full[4];
static lv_obj_t *s_heart_fun[4];
static lv_obj_t *s_weight_lbl;
static lv_obj_t *s_bond_lbl;

// 房间
static lv_obj_t *s_room;
static lv_obj_t *s_wall_top;
static lv_obj_t *s_floor_obj;
static lv_obj_t *s_floor_edge;
static lv_obj_t *s_creature;
static lv_obj_t *s_furn_layer;   // S4 已摆放功能家具的几何标记层
static lv_obj_t *s_star[6];      // 星空主题点缀
static lv_obj_t *s_eye[4][2];     // normal/happy/sleep/sick（几何蛋期不再使用，保留 NULL 安全切换）
static lv_obj_t *s_mouth[2];      // normal/eat
static lv_obj_t *s_zz;

// P2-S2：基因合成画布缓冲（05 §5.2：无 PSRAM，严格单实例、可复用，32 KiB）。
static uint16_t s_pet_canvas_px[PT_COMPOSE_PIXELS];
static const uint8_t *pet_art_frame(int slot, uint8_t index, uint8_t frame,
                                    uint16_t *bytes)
{
    const pet_part_art_t *a = pet_parts_art(slot, index);
    if (a == NULL || frame >= a->frame_count) {
        return NULL;
    }
    *bytes = a->bytes[frame];
    return a->rle[frame];
}
static const pet_art_provider_t s_pet_art = { pet_art_frame };
static lv_obj_t *s_pet_canvas_obj;                 // 非蛋期合成画布（蛋/悼念页为 NULL）
static uint8_t s_pose[PT_GENE_SLOT_COUNT];        // 当前各槽动画帧
static bool s_pose_dirty = true;                  // 重建后强制重算一次
static bool s_night_canvas;                      // 画布底色当前是否夜墙色
static lv_obj_t *s_bubble;
static lv_obj_t *s_bubble_txt;
static lv_obj_t *s_poops;
static lv_obj_t *s_night;

// 微状态行（PV1 象形版：碗/笑 + 四心计量 + 体重胶囊 + 羁绊值，对象在 init 里创建）

// 图标坞与说明
static pet_dock_t s_dock;
static dock_widget_t s_dock_w[PET_ICON_COUNT];
static lv_obj_t *s_caption;       // 情境提示胶囊（房间底部，平时隐藏）
static lv_obj_t *s_caption_txt;

// 弹层
static lv_obj_t *s_modal;
static lv_obj_t *s_modal_title;
static lv_obj_t *s_modal_body;
static lv_obj_t *s_game_box;      // G1 中央卡（赢时变绿）
static lv_obj_t *s_game_card;
static lv_obj_t *s_game_sub;      // 结果副行 "7 -> 9"
static lv_obj_t *s_game_hi;
static lv_obj_t *s_game_lo;
static lv_obj_t *s_game_result;
static lv_obj_t *s_game_pip[5];   // 5 档胜场灯
static lv_obj_t *s_game_hint;

// 全屏演出与纪念
static lv_obj_t *s_eff_cont;
static lv_obj_t *s_eff_ring_out;   // 悬念期呼吸双环（金/粉）
static lv_obj_t *s_eff_ring_in;
static lv_obj_t *s_eff_glyph;      // Montserrat20：孵化 ! / 进化 ?
static lv_obj_t *s_eff_word;       // HATCHING / EVOLVING
static lv_obj_t *s_eff_burst;      // 揭晓扩散粉环
static lv_obj_t *s_eff_spark[4];   // 揭晓飞散星点（金/粉）
static lv_obj_t *s_memorial;
static lv_obj_t *s_memorial_name;
static lv_obj_t *s_memorial_line;
static lv_obj_t *s_memorial_tier;

static ui_mode_t s_mode = MODE_ROOM;
static int s_list_kind;
static int s_list_sel;
static int s_status_page;
static int s_status_focus;   // PV2 STATUS 页脚焦点：0=‹ 1=INFO 2=VITALS 3=CARE 4=›
static uint8_t s_sound_page; // PV2 SOUND 卡片页：0=音量四档 1=免打扰两档

static pet_game_t s_game;
static int s_g_phase;
static int s_g_choice;
static int64_t s_g_exit_armed_until;

// 游戏选择器（PV2 两页卡：页 0 = G1–G4，页 1 = G5–G6；焦点复用 s_list_sel）
static uint8_t s_games_page;

// G2–G6 统一运行时
static uint8_t s_gx_id;
static uint8_t s_gx_phase;        // 0=READY 1=进行中 2=结算
static int64_t s_gx_t0;
static int64_t s_gx_next_ms;
static int64_t s_gx_exit_armed_until;
static uint8_t s_gx_sel;          // G4/G6 当前选项
static uint8_t s_g4_lit;          // G4 演示阶段点亮的键位
static int16_t s_g2_from;         // G2 本拍起始端（-100/100）
static pet_g2_t s_g2;
static pet_g3_t s_g3;
static pet_g4_t s_g4;
static pet_g5_t s_g5;
static pet_g6_t s_g6;
static bool s_gx_job;         // true=打工短关（结算走工资，非游戏奖金）
static lv_obj_t *s_gx_pill;       // 顶部信息胶囊底
static lv_obj_t *s_gx_info;
static lv_obj_t *s_gx_hint;
#define GX_MK_N 12
static lv_obj_t *s_gx_mk[GX_MK_N];
static lv_obj_t *s_gx_ml[4];
#define GX_IMG_N 3
static lv_obj_t *s_gx_img[GX_IMG_N];
// READY / RESULT 中央卡
static lv_obj_t *s_gx_card;
static lv_obj_t *s_gx_card_icon;
static lv_obj_t *s_gx_card_l1;
static lv_obj_t *s_gx_card_l2;
static lv_obj_t *s_gx_card_l3;

// 成年职业（S3）
static pt_jobs_t s_jobs;
static int8_t s_job_sel;
static int8_t s_agency_sel;
static pt_job_id_t s_agency_ids[PT_JOB_COUNT];
static uint8_t s_agency_n;
static lv_obj_t *s_job_row[2];
static bool s_g1_job;

static int s_eff_kind = EFF_NONE;
static int64_t s_eff_t0;
static bool s_eff_rebuilt;

static int s_mood = MOOD_NORMAL;
static int64_t s_mood_until;
static int64_t s_next_blink;
static int64_t s_blink_until;

static char s_msg[40];
static int64_t s_msg_until;

static pt_state_t s_snap;
static pt_econ_t s_esnap;
static pt_state_t s_prev;
static bool s_have_prev;
static bool s_checkin_prompted;   // 本次开机是否已弹过签到

// 商店/背包视图
static bool s_shop_bag;           // false=货架, true=背包
static uint8_t s_shop_page;
static uint8_t s_shop_sel;        // PV2：跨卡片+页脚的扁平焦点序号
static bool s_shop_dirty;

// PV2 卡片弹层：当前页卡片根矩形（最多 4 张）与常态底色（选中态切粉底后复原）。
static lv_obj_t *s_pv2_card[4];
static uint32_t s_pv2_card_bg[4];

// PV2 扁平焦点槽：卡片在前，末尾按  ‹ / SHOP / BAG / › 顺序挂页脚控件。
enum {
    PV2_F_CARD = 0,
    PV2_F_PREV,
    PV2_F_SHOP,
    PV2_F_BAG,
    PV2_F_NEXT,
};
#define PV2_F_MAX 9
static uint8_t s_focus_n;
static uint8_t s_focus_kind[PV2_F_MAX];
static uint8_t s_focus_param[PV2_F_MAX];   // CARD 时为当前页内卡序号

// SHOP/BAG 标签页下的完整条目序列（签到/Style 作为虚拟行），每页切 4 张。
enum {
    SEQ_ITEM = 0,
    SEQ_BONUS,
    SEQ_STYLE,
};
#define SEQ_MAX (PT_ECON_INV_SLOTS + 2)
static uint8_t s_seq_n;
static uint8_t s_seq_kind[SEQ_MAX];
static uint16_t s_seq_id[SEQ_MAX];
static uint8_t s_shop_pages;

// PV2 弹层框架 chrome：发丝线 / 金币胶囊 / 页码 / 页脚 / 返回提示 / toast。
static lv_obj_t *s_mhair;
static lv_obj_t *s_mcoin;
static lv_obj_t *s_mcoin_lbl;
static lv_obj_t *s_mp_prev;
static lv_obj_t *s_mp_cur;
static lv_obj_t *s_mp_next;
static lv_obj_t *s_mfooter;
static lv_obj_t *s_ft_prev, *s_ft_shop, *s_ft_bag, *s_ft_next;
// PV2 batch2 STATUS 专用页脚：‹ INFO VITALS CARE ›（与 SHOP 页脚互斥）。
static lv_obj_t *s_sfooter;
static lv_obj_t *s_sf_prev;
static lv_obj_t *s_sf_tab[3];
static lv_obj_t *s_sf_next;
// PV2 batch4 DEX 专用页脚：‹ ALBUM PARTS BADGES ›（与其它页脚互斥）。
static lv_obj_t *s_dfooter;
static lv_obj_t *s_df_prev;
static lv_obj_t *s_df_tab[3];
static lv_obj_t *s_df_next;
// PV2 batch5 FAMILY 专用页脚：BACK/TREE、仅 BACK、‹ BACK › 三态。
static lv_obj_t *s_mffooter;
static lv_obj_t *s_mf_back;
static lv_obj_t *s_mf_tree;
static lv_obj_t *s_mf_prev;
static lv_obj_t *s_mf_mid;
static lv_obj_t *s_mf_next;
// PV2 batch6：签到 CLAIM 单丸；STYLE 三档页脚 WEAR/ROOM/THEME + 贝壳 chip。
static lv_obj_t *s_cclaim;
static lv_obj_t *s_tfooter;
static lv_obj_t *s_tf_tab[3];
static lv_obj_t *s_tshell;
static lv_obj_t *s_tshell_lbl;
static lv_obj_t *s_mback;
static lv_obj_t *s_toast_box;
static lv_obj_t *s_toast_msg;
static lv_timer_t *s_toast_timer;

// 换装/家具/主题（S4，PV2 batch6）
static pt_decor_t s_dsnap;
static pt_decor_t s_dprev;
static bool s_have_dprev;
static uint8_t s_dec_tab;        // 0=Wear 1=Room 2=Theme
static uint8_t s_dec_sel;        // 0..n-1=行；n..n+2=页脚三档
static lv_obj_t *s_t_row[6];     // 当前档列表行（最多 6）
static bool s_dec_dirty;
static int s_battery_cache = -2;
static int s_battery_div;
static bool s_bubble_anim_on;

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------

static lv_obj_t *rect(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                      int32_t radius, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static void border(lv_obj_t *o, uint32_t color, int32_t width)
{
    lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(o, width, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
}

static lv_obj_t *label(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                       const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text);
    return l;
}

static void show(lv_obj_t *o) { lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); }
static void hide(lv_obj_t *o) { lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); }

static void set_msg(const char *text, int ms)
{
    snprintf(s_msg, sizeof(s_msg), "%s", text);
    s_msg_until = (int64_t) lv_tick_get() + ms;
}

static int64_t now_ms(void) { return (int64_t) lv_tick_get(); }

// 容器基准位置（CREATURE_X/Y）：所有位移动画都必须以基准为起点写绝对坐标——
// lv_anim 的 exec_cb 直接改 style x/y，若写 0/-3 这类"相对值"，动画一启动
// 就会把对象从基准位置永久拽到屏幕左上角（历史 bug：宠物长期出现在左上）。
#define CREATURE_X 60
#define CREATURE_Y 56

static void bounce(lv_obj_t *o, int32_t base, int32_t dist, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, base, base + dist);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_playback_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) lv_obj_set_y);
    lv_anim_start(&a);
}

// 名称表（原创占位英文名，designs 04 §8 命名待定）
static const char *species_name(pt_species_t sp)
{
    switch (sp) {
    case PT_SP_EGG: return "Egg";
    case PT_SP_BABY: return "Bao";
    case PT_SP_CHILD: return "Bean";
    case PT_SP_TEEN_A: return "Mochi";
    case PT_SP_TEEN_B: return "Goo";
    case PT_SP_TEEN_C: return "Fuzz";
    case PT_SP_ADULT_PERFECT: return "Snowball";
    case PT_SP_ADULT_GREAT: return "Marsh";
    case PT_SP_ADULT_NORMAL: return "Riceball";
    case PT_SP_ADULT_NEGLECT: return "Soot";
    case PT_SP_ADULT_MOON: return "Moonshade";
    default: return "Pet";
    }
}

static const char *stage_name(pt_stage_t st)
{
    switch (st) {
    case PT_STAGE_EGG: return "EGG";
    case PT_STAGE_BABY: return "BABY";
    case PT_STAGE_CHILD: return "CHILD";
    case PT_STAGE_TEEN: return "TEEN";
    case PT_STAGE_ADULT: return "ADULT";
    case PT_STAGE_SENIOR: return "SENIOR";
    default: return "--";
    }
}

static const char *call_text(pt_call_kind_t call)
{
    switch (call) {
    case PT_CALL_HUNGRY: return "!";
    case PT_CALL_SAD: return "?";
    case PT_CALL_LIGHTS: return "z";
    default: return "";
    }
}

// ---------------------------------------------------------------------------
// 角色程序化绘制
// ---------------------------------------------------------------------------

typedef struct {
    uint8_t w, h, radius;
    uint32_t body, edge, belly, cheek;
    uint8_t ears;      // 0 无 / 1 圆耳 / 2 尖耳
    bool dark_eyes;    // 深色身体用浅色眼睛
    bool crescent;     // 月影额头标记
} profile_t;

static const profile_t *profile_for(pt_stage_t stage, pt_species_t sp)
{
    static const profile_t EGG = { 60, 74, 30, 0xFFFFFF, 0xC9BEB0, 0, 0, 0, false, false };
    static const profile_t BABY = { 70, 64, 32, 0xFFE27A, 0xD9A93B, 0xFFF3BF, 0xF7B7A8, 1, false, false };
    static const profile_t CHILD = { 68, 70, 30, 0xF3C68F, 0xC99456, 0xFBE7C8, 0xF2A0A0, 1, false, false };
    static const profile_t TEEN_A = { 76, 68, 34, 0xF6B8C9, 0xD88AA3, 0xFBD9E4, 0xF48FA0, 2, false, false };
    static const profile_t TEEN_B = { 72, 72, 36, 0xA8D8EA, 0x6FA8C4, 0xD8F0FA, 0xF2A0A0, 1, false, false };
    static const profile_t TEEN_C = { 80, 70, 34, 0xB9E09A, 0x86B366, 0xDCF0C8, 0xF2A0A0, 1, false, false };
    static const profile_t PERF = { 88, 76, 40, 0xFBFBF6, 0xBFC4C9, 0xF1F1EA, 0xF4A8B8, 1, false, false };
    static const profile_t GREAT = { 92, 72, 36, 0xFCD2E2, 0xE497B8, 0xFEE9F1, 0xF48FA0, 2, false, false };
    static const profile_t NORMAL = { 84, 74, 22, 0xE7DEC8, 0xB8A784, 0xF4EDDB, 0xEAA99E, 0, false, false };
    static const profile_t NEGLECT = { 76, 72, 36, 0x5B5566, 0x3E3A48, 0x77707F, 0, 0, true, false };
    static const profile_t MOON = { 86, 78, 40, 0x3E3E68, 0x27274A, 0x5A5A8E, 0, 2, true, true };

    if (stage == PT_STAGE_EGG) { return &EGG; }
    switch (sp) {
    case PT_SP_BABY: return &BABY;
    case PT_SP_CHILD: return &CHILD;
    case PT_SP_TEEN_A: return &TEEN_A;
    case PT_SP_TEEN_B: return &TEEN_B;
    case PT_SP_TEEN_C: return &TEEN_C;
    case PT_SP_ADULT_PERFECT: return &PERF;
    case PT_SP_ADULT_GREAT: return &GREAT;
    case PT_SP_ADULT_NORMAL: return &NORMAL;
    case PT_SP_ADULT_NEGLECT: return &NEGLECT;
    case PT_SP_ADULT_MOON: return &MOON;
    default: return &CHILD;
    }
}

static void egg_idle_anim(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_creature);
    // 绝对坐标 = 基准 X ± 3，不能写 -3..3（会把容器拽离 CREATURE_X）。
    lv_anim_set_values(&a, CREATURE_X - 3, CREATURE_X + 3);
    lv_anim_set_duration(&a, 320);
    lv_anim_set_playback_duration(&a, 320);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) lv_obj_set_x);
    lv_anim_start(&a);
}

// RGB565 合成画布无 alpha：背景必须与宠物身后的墙同色，128x128 方块才隐形。
// 夜间房间层是固定深蓝（不随主题变色），关灯时画布底填夜色，否则填主题墙色；
// 灯光切换由刷新逻辑置 s_pose_dirty 触发重合成。
static uint16_t canvas_wall565(void)
{
    if (s_snap.lights_off) {
        return pt_rgb565_u32(COL_NIGHT_WALL);
    }
    const pt_theme_def_t *td = pt_theme_def((pt_theme_t) s_dsnap.theme);
    if (td == NULL) {
        td = pt_theme_def(PT_THEME_COZY);
    }
    return pt_rgb565_u32(td->wall);
}

static void build_creature(void)
{
    const pt_state_t *s = &s_snap;
    lv_anim_delete(s_creature, NULL);
    // 复位基准位置：上个阶段的摆动/反应动画被删时样式值可能停在中途，
    // 不复位会把偏移带进新阶段。
    lv_obj_set_pos(s_creature, CREATURE_X, CREATURE_Y);
    lv_obj_clean(s_creature);
    for (int r = 0; r < 4; r += 1) {
        s_eye[r][0] = s_eye[r][1] = NULL;
    }
    s_mouth[0] = s_mouth[1] = NULL;
    s_zz = NULL;
    s_pet_canvas_obj = NULL;
    memset(s_pose, 0, sizeof(s_pose));
    s_pose_dirty = true;

    if (s->stage == PT_STAGE_DEAD) {
        return;   // 纪念页全屏接管
    }

    int32_t W = 120;
    int32_t cx = W / 2;

    if (s->stage == PT_STAGE_EGG) {
        const profile_t *p = profile_for(s->stage, s->species);
        int32_t bx = (W - p->w) / 2;
        int32_t by = 112 - 12 - p->h;

        if (p->ears == 1) {
            lv_obj_t *e1 = rect(s_creature, bx + 6, by - 7, 15, 15, 7, p->body);
            lv_obj_t *e2 = rect(s_creature, bx + p->w - 21, by - 7, 15, 15, 7, p->body);
            border(e1, p->edge, 2);
            border(e2, p->edge, 2);
        } else if (p->ears == 2) {
            rect(s_creature, bx + 8, by - 6, 9, 12, 3, p->body);
            rect(s_creature, bx + p->w - 17, by - 6, 9, 12, 3, p->body);
        }
        lv_obj_t *body = rect(s_creature, bx, by, p->w, p->h, p->radius, p->body);
        border(body, p->edge, 2);
        if (p->belly != 0) {
            int32_t bw = (int32_t) p->w * 56 / 100;
            int32_t bh = (int32_t) p->h * 30 / 100;
            rect(s_creature, cx - bw / 2, by + p->h - bh - 9, bw, bh, bh / 2, p->belly);
        }
        rect(s_creature, bx + (int32_t) p->w * 16 / 100, by + p->h - 5, 13, 7, 3, p->edge);
        rect(s_creature, bx + (int32_t) p->w * 68 / 100, by + p->h - 5, 13, 7, 3, p->edge);
        int32_t spots[4][2] = {
            { bx + 14, by + 20 }, { bx + p->w - 22, by + 28 },
            { bx + 22, by + p->h - 22 }, { bx + p->w - 16, by + p->h - 16 },
        };
        for (int i = 0; i < 4; i += 1) {
            rect(s_creature, spots[i][0], spots[i][1], 5, 5, 2, p->edge);
        }
        // 蛋期待孵化：左右轻晃（全屏弹层打开时暂停，关闭后由 modal_close 恢复，
        // 避免动画失效区与全屏重绘在模拟器软件渲染下抢 CPU）。
        egg_idle_anim();
        return;
    }

    // P2-S2：L2 基因合成宠（designs 05 §5）。RGB565 无 alpha，背景填当前
    // 房间墙色——房间为单色墙，方块视觉融合；主题/灯光切换时随刷新重建。
    uint16_t wall565 = canvas_wall565();
    pt_compose_pet(&s->genome, s_pose, wall565, s_pet_canvas_px, &s_pet_art);
    lv_obj_t *canvas = lv_canvas_create(s_creature);
    lv_canvas_set_buffer(canvas, s_pet_canvas_px, PT_COMPOSE_W, PT_COMPOSE_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas, -4, -8);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(canvas, 0, 0);
    lv_obj_set_style_pad_all(canvas, 0, 0);
    s_pet_canvas_obj = canvas;
    s_night_canvas = s->lights_off;

    // S4 穿戴层（05 §7.1：穿戴层在基因层之上）。坐标按 128x128 合成画布
    // （容器内偏移 -4,-8）标定；§5.3 表情帧（眨眼/吃/睡/病/呼吸）已在
    // update_creature_mood 按槽切帧重合成。
    for (uint8_t slot = 0; slot < PT_SLOT_COUNT; slot += 1) {
        pt_outfit_t oid = pt_decor_worn(&s_dsnap, (pt_slot_t) slot);
        if (oid == PT_OUTFIT_NONE) {
            continue;
        }
        switch (oid) {
        case PT_OUTFIT_CAP:
            rect(s_creature, cx - 15, 0, 30, 4, 2, 0x3E6FD0);
            rect(s_creature, cx - 11, -6, 22, 9, 5, 0x4E86E6);
            break;
        case PT_OUTFIT_TOPHAT:
            rect(s_creature, cx - 15, 0, 30, 3, 1, 0x33292A);
            rect(s_creature, cx - 9, -11, 18, 13, 2, 0x33292A);
            break;
        case PT_OUTFIT_STARHAT:
            rect(s_creature, cx - 10, -2, 20, 12, 4, 0x7B5EA8);
            rect(s_creature, cx - 3, -7, 7, 7, 3, 0xFFD84D);
            rect(s_creature, cx - 13, 3, 26, 3, 1, 0xFFD84D);
            break;
        case PT_OUTFIT_GLASSES: {
            lv_obj_t *g1 = rect(s_creature, 38, 60, 12, 9, 3, 0xFFFFFF);
            lv_obj_t *g2 = rect(s_creature, 70, 60, 12, 9, 3, 0xFFFFFF);
            border(g1, 0x3A2F2A, 2);
            border(g2, 0x3A2F2A, 2);
            break;
        }
        case PT_OUTFIT_SCARF:
            rect(s_creature, cx - 24, 92, 48, 6, 3, 0xE05B5B);
            break;
        case PT_OUTFIT_BALLOON: {
            lv_obj_t *ball = rect(s_creature, 100, 0, 13, 13, 7, 0xF07A96);
            border(ball, 0xB84564, 1);
            rect(s_creature, 106, 14, 1, 9, 0, 0xB84564);
            break;
        }
        default:
            break;
        }
    }

    s_zz = label(s_creature, 82, 2, 30, &lv_font_montserrat_20, COL_BLUE, "Z");
    hide(s_zz);
}

static void build_poops(void)
{
    lv_obj_clean(s_poops);
    uint8_t n = s_snap.poops;
    for (uint8_t i = 0; i < n; i += 1) {
        int32_t x = 120 - (int32_t) n * 9 + (int32_t) i * 18;
        lv_obj_t *o = rect(s_poops, x, 8, 14, 11, 5, COL_BROWN);
        border(o, 0x6E5436, 2);
    }
}

// S4：房间主题换壁纸色；星空主题额外显示固定星点。
static void apply_room_theme(void)
{
    const pt_theme_def_t *td = pt_theme_def((pt_theme_t) s_dsnap.theme);
    if (td == NULL) {
        td = pt_theme_def(PT_THEME_COZY);
    }
    // Cozy 固定为定稿的马卡龙绿；其他主题用其墙色压暗推地板色。
    uint32_t wall = COL_ROOM_TOP;
    if (s_dsnap.theme != PT_THEME_COZY) {
        wall = td->wall;
    }
    lv_obj_set_style_bg_color(s_room, lv_color_hex(wall), 0);
    lv_obj_set_style_bg_color(s_wall_top, lv_color_hex(wall), 0);
    uint32_t floor;
    if (s_dsnap.theme == PT_THEME_COZY) {
        floor = COL_FLOOR;
    } else {
        // 墙色压暗 18%，边缘再压一档。
        uint32_t r = (wall >> 16) & 0xFF;
        uint32_t g = (wall >> 8) & 0xFF;
        uint32_t b = wall & 0xFF;
        r = (r * 82) / 100;
        g = (g * 82) / 100;
        b = (b * 82) / 100;
        floor = (r << 16) | (g << 8) | b;
    }
    lv_obj_set_style_bg_color(s_floor_obj, lv_color_hex(floor), 0);
    uint32_t edge = (((((floor >> 16) & 0xFF) * 82) / 100) << 16)
                  | ((((floor >> 8) & 0xFF) * 82) / 100 << 8)
                  | (((floor & 0xFF) * 82) / 100);
    lv_obj_set_style_bg_color(s_floor_edge, lv_color_hex(edge), 0);
    bool stars = s_dsnap.theme == PT_THEME_STARRY;
    for (uint8_t i = 0; i < 6; i += 1) {
        if (s_star[i] != NULL) {
            if (stars) {
                show(s_star[i]);
            } else {
                hide(s_star[i]);
            }
        }
    }
}

// S4：已摆放功能家具的极简几何标记（每件固定位置，不放 3 件以上）。
static void build_furniture(void)
{
    if (s_furn_layer == NULL) {
        return;
    }
    lv_obj_clean(s_furn_layer);
    // 盆栽：左下；健身角：右上；厨房：左上。
    if (pt_decor_is_placed(&s_dsnap, PT_FURN_PLANT)) {
        rect(s_furn_layer, 4, 128, 16, 10, 2, 0xB06A3E);   // 盆
        lv_obj_t *leaf = rect(s_furn_layer, 2, 110, 20, 20, 10, 0x4F9E54);
        border(leaf, 0x35723A, 1);
    }
    if (pt_decor_is_placed(&s_dsnap, PT_FURN_GYM)) {
        rect(s_furn_layer, 206, 30, 20, 4, 2, 0x5A6270);   // 杠
        rect(s_furn_layer, 202, 26, 6, 12, 2, 0x3A4250);
        rect(s_furn_layer, 224, 26, 6, 12, 2, 0x3A4250);
    }
    if (pt_decor_is_placed(&s_dsnap, PT_FURN_KITCHEN)) {
        rect(s_furn_layer, 6, 28, 22, 20, 3, 0xD9C9B4);    // 柜
        lv_obj_t *pot = rect(s_furn_layer, 9, 18, 16, 12, 3, 0x8A94A6);
        border(pot, 0x5F6776, 1);
    }
}

// ---------------------------------------------------------------------------
// 图标坞
// ---------------------------------------------------------------------------

static lv_obj_t *s_dockbox;

// 坞带几何（与定稿 mockup 一致：276..320，槽中线 298，图标 20/选中 25）
#define DOCK_GAP       2
#define DOCK_START_X   6
#define DOCK_BAND_W    228
#define DOCK_TILE_Y    4    // 相对 dockbox(276)：胶囊 36 高 → 280..316
#define DOCK_TILE_H    36
#define DOCK_IMG_Y     12   // 20px：288..308，中线 298
#define DOCK_IMG_Y_SEL 9    // 25px：285..310，中线 297.5

static int32_t s_slot_w;
static pet_icon_t s_last_sel = PET_ICON_COUNT;   // 选中变化弹跳用；重建后为哨兵
static int8_t s_dock_style[PET_ICON_COUNT];      // 每槽当前样式：-1 未知 / 0 普通 / 1 选中

static int32_t slot_center(uint8_t i)
{
    return DOCK_START_X + (int32_t) i * (s_slot_w + DOCK_GAP)
           + s_slot_w / 2 + (s_slot_w % 2);
}

static lv_obj_t *icon_img(lv_obj_t *parent, int32_t x, int32_t y,
                          const lv_image_dsc_t *dsc)
{
    lv_obj_t *o = lv_image_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_image_set_src(o, dsc);
    return o;
}

static void dock_build_widgets(void)
{
    // 依据当前阶段 + 药解锁状态重建坞内容（图标按阶段长出来）。
    pet_dock_build(&s_dock, s_snap.stage, pet_app_med_unlocked());
    for (uint8_t i = 0; i < PET_ICON_COUNT; i += 1) {
        s_dock_w[i].box = NULL;
        s_dock_w[i].img = NULL;
        s_dock_style[i] = -1;
    }
    s_last_sel = PET_ICON_COUNT;   // 重建不触发弹跳
    lv_obj_clean(s_dockbox);
    s_slot_w = (DOCK_BAND_W - (int32_t) (s_dock.count - 1) * DOCK_GAP)
               / (int32_t) s_dock.count;
    for (uint8_t i = 0; i < s_dock.count; i += 1) {
        pet_icon_t icon = s_dock.icons[i];
        int32_t x = DOCK_START_X + (int32_t) i * (s_slot_w + DOCK_GAP);
        // 槽胶囊：常态全透明；选中态刷奶油底 + 粉描边。
        lv_obj_t *box = rect(s_dockbox, x, DOCK_TILE_Y, s_slot_w, DOCK_TILE_H,
                             10, COL_CREAM);
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
        int32_t cx = slot_center(i);
        lv_obj_t *img = icon_img(s_dockbox, cx - 10, DOCK_IMG_Y,
                                 pet_ui_dock_dsc(icon, false));
        s_dock_w[icon].box = box;
        s_dock_w[icon].img = img;
    }
    pet_dock_focus_call(&s_dock, s_snap.active_call);
}

static void dock_refresh_selected(void)
{
    pet_icon_t sel = pet_dock_selected(&s_dock);
    for (uint8_t i = 0; i < s_dock.count; i += 1) {
        pet_icon_t icon = s_dock.icons[i];
        lv_obj_t *box = s_dock_w[icon].box;
        lv_obj_t *img = s_dock_w[icon].img;
        if (box == NULL || img == NULL) {
            continue;
        }
        bool selected = (icon == sel);
        int8_t want = selected ? 1 : 0;
        // 仅在档位变化时改样式/坐标：避免每帧 set_pos 与弹跳 y 动画互相打架。
        if (s_dock_style[icon] == want) {
            continue;
        }
        s_dock_style[icon] = want;
        if (selected) {
            lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
            border(box, COL_SEL, 2);
            lv_image_set_src(img, pet_ui_dock_dsc(icon, true));
            lv_obj_set_pos(img, slot_center(i) - 12, DOCK_IMG_Y_SEL);
        } else {
            lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(box, 0, 0);
            lv_image_set_src(img, pet_ui_dock_dsc(icon, false));
            lv_obj_set_pos(img, slot_center(i) - 10, DOCK_IMG_Y);
        }
    }
    // 选中切槽：胶囊 + 25px 图标一起上跳 4px 再回落（定稿"放大 + 弹跳"）。
    if (s_last_sel != PET_ICON_COUNT && s_last_sel != sel) {
        lv_obj_t *box = s_dock_w[sel].box;
        lv_obj_t *img = s_dock_w[sel].img;
        if (box != NULL && img != NULL) {
            bounce(box, DOCK_TILE_Y, -4, 170);
            bounce(img, DOCK_IMG_Y_SEL, -4, 170);
        }
    }
    s_last_sel = sel;
}

// ---------------------------------------------------------------------------
// 弹层：通用列表（喂食 / 设置）、状态分页、G1 游戏
// ---------------------------------------------------------------------------

// ===========================================================================
// PV2 统一卡片弹层（定稿 pet-redesign/pv2-cards.html）
// 面板局部坐标：页眉 0..36，卡片区 42..198（4 张 192x36），页脚 200..220，
// 返回提示基线约 234。所有卡片弹层统一走 frame_pv2 几何。
// ===========================================================================

static void toast_hide_cb(lv_timer_t *t)
{
    hide(s_toast_box);
    s_toast_timer = NULL;
    lv_timer_delete(t);
}

// 面板内顶部下滑的拒绝/反馈胶囊；1.2s 自动消失，不打断焦点。
static void toast_show(const char *msg)
{
    lv_label_set_text(s_toast_msg, msg);
    show(s_toast_box);
    lv_obj_move_foreground(s_toast_box);
    if (s_toast_timer != NULL) {
        lv_timer_delete(s_toast_timer);
    }
    s_toast_timer = lv_timer_create(toast_hide_cb, 1200, NULL);
}

static void frame_pv2(bool with_footer)
{
    lv_obj_set_pos(s_modal_body, 8, 42);
    lv_obj_set_size(s_modal_body, 192, 156);
    show(s_mhair);
    show(s_mback);
    lv_label_set_text(s_mback, "hold OK - back");
    if (with_footer) {
        show(s_mfooter);
    } else {
        hide(s_mfooter);
    }
    hide(s_sfooter);
    hide(s_dfooter);
    hide(s_mffooter);
    hide(s_tfooter);
    hide(s_cclaim);
    hide(s_tshell);
    hide(s_mcoin);
    // SOUND 会把页码挪到页眉居中；回到其它弹层时复位。
    lv_obj_set_pos(s_mp_cur, 66, 15);
    hide(s_mp_prev);
    hide(s_mp_cur);
    hide(s_mp_next);
    hide(s_toast_box);
}

// Montserrat ASCII 文本像素宽（不走 lv_text 可变参数 API）。
static int32_t text_wf(const lv_font_t *font, const char *s)
{
    int32_t w = 0;
    for (size_t i = 0; s[i] != '\0'; i += 1) {
        w += lv_font_get_glyph_width(font, (uint32_t) s[i],
                                     (uint32_t) s[i + 1]);
    }
    return w;
}

static int32_t text_w12(const char *s)
{
    return text_wf(&lv_font_montserrat_12, s);
}

static lv_obj_t *llabel(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                        const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *l = label(parent, x, y, w, font, color, text);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
    return l;
}

// 右缘对齐 x_end 的 12px 文本（PV2 卡片右侧数值）。
static lv_obj_t *rlabel12(lv_obj_t *parent, int32_t x_end, int32_t y,
                          uint32_t color, const char *text)
{
    int32_t w = text_w12(text);
    return llabel(parent, x_end - w, y, w + 1, &lv_font_montserrat_12,
                  color, text);
}

// 仪表卡细槽条：轨道 100x7，填充 0..100（最小值 3px 圆点，同定稿稿）。
static void meter100(lv_obj_t *parent, int32_t x, int32_t y, uint8_t v,
                     uint32_t color)
{
    rect(parent, x, y, 100, 7, 3, COL_PIP_OFF);
    int32_t fw = (int32_t) v;
    if (fw < 3) {
        fw = 3;
    }
    rect(parent, x, y, fw, 7, 3, color);
}

// 7px 效果心：绿=饱腹，黄=开心；空槽不画（稿面只亮实际强度）。
static void card_hearts(lv_obj_t *p, int32_t x, int32_t y, uint8_t g,
                        uint8_t yh)
{
    for (uint8_t i = 0; i < g; i += 1) {
        icon_img(p, x + (int32_t) i * 12, y,
                 pet_ui_small_dsc(PET_UI_SMALL_HEART_GREEN));
    }
    for (uint8_t i = 0; i < yh; i += 1) {
        icon_img(p, x + (int32_t) (g + i) * 12, y,
                 pet_ui_small_dsc(PET_UI_SMALL_HEART_YELLOW));
    }
}

enum {
    RIGHT_NONE = 0,
    RIGHT_PRICE,    // 金币胶囊（price）
    RIGHT_SALE,     // 红 -20% 标签 + 现价 / 删除线原价
    RIGHT_OWNED,    // 绿对勾圆
    RIGHT_QTY,      // 背包食物数量
    RIGHT_PLAY,     // 玩具剩余次数（qty=剩余 0..3）
    RIGHT_GET,      // 签到 GET
    RIGHT_GO,       // Style GO
};

static void card_right(lv_obj_t *root, uint8_t mode, uint32_t price,
                       uint32_t old_price, uint8_t qty)
{
    char buf[16];
    switch (mode) {
    case RIGHT_PRICE: {
        snprintf(buf, sizeof(buf), "%luG", (unsigned long) price);
        int32_t tw = text_w12(buf);
        int32_t pw = tw + 18;
        if (pw < 40) {
            pw = 40;
        }
        int32_t px = 188 - pw;
        lv_obj_t *pill = rect(root, px, 9, pw, 18, 9, COL_COIN_BG);
        border(pill, COL_INK, 1);
        rect(pill, 5, 6, 6, 6, 3, COL_YELLOW);   // 小金币点（盒内描边简化）
        label(pill, 11, 2, pw - 13, &lv_font_montserrat_12, COL_INK, buf);
        break;
    }
    case RIGHT_SALE: {
        lv_obj_t *tag = rect(root, 140, 4, 32, 13, 4, COL_RED);
        label(tag, 0, 0, 32, &lv_font_montserrat_12, COL_CARD_WHITE, "-20%");
        snprintf(buf, sizeof(buf), "%lu", (unsigned long) price);
        int32_t wn = text_w12(buf);
        char old[12];
        snprintf(old, sizeof(old), "%lu", (unsigned long) old_price);
        int32_t wo = text_w12(old);
        int32_t gx = 188 - (8 + 2 + wn + 5 + wo);
        rect(root, gx, 23, 6, 6, 3, COL_YELLOW);
        llabel(root, gx + 9, 19, wn + 2, &lv_font_montserrat_12, COL_INK, buf);
        int32_t ox = gx + 9 + wn + 5;
        llabel(root, ox, 19, wo + 2, &lv_font_montserrat_12, COL_HINT, old);
        // 1px 删除线（不用 text_decor，跨 LVGL 小版本最稳）。
        rect(root, ox, 26, wo, 1, 0, COL_HINT);
        break;
    }
    case RIGHT_OWNED: {
        lv_obj_t *c = rect(root, 168, 8, 20, 20, 10, COL_CARD_GREENBG);
        border(c, COL_GREEN, 1);
        label(c, 0, 3, 20, &lv_font_montserrat_12, COL_GREEN_DEEP,
              LV_SYMBOL_OK);
        break;
    }
    case RIGHT_QTY: {
        lv_obj_t *c = rect(root, 167, 7, 22, 22, 11, COL_BADGE_BLUE);
        border(c, COL_INK, 1);
        snprintf(buf, sizeof(buf), "x%u", (unsigned) qty);
        label(c, 0, 4, 22, &lv_font_montserrat_12, COL_INK, buf);
        break;
    }
    case RIGHT_PLAY: {
        int32_t tw = text_w12("play");
        llabel(root, 159 - tw, 19, tw + 1, &lv_font_montserrat_12, COL_INK,
               "play");
        for (uint8_t i = 0; i < 3; i += 1) {
            rect(root, 163 + (int32_t) i * 9, 20, 7, 9, 2,
                 i < qty ? COL_BLUE : COL_PIP_OFF);
        }
        break;
    }
    case RIGHT_GET:
    case RIGHT_GO: {
        lv_obj_t *g = rect(root, 148, 9, 36, 17, 9,
                           mode == RIGHT_GET ? COL_YELLOW : COL_GREEN);
        border(g, COL_INK, 1);
        label(g, 0, 2, 36, &lv_font_montserrat_12,
              mode == RIGHT_GET ? COL_INK : COL_PANEL,
              mode == RIGHT_GET ? "GET" : "GO");
        break;
    }
    default:
        break;
    }
}

// 建一张 192x36 卡片（父级 s_modal_body，y 为 body 局部坐标）。
static lv_obj_t *pv2_card(uint8_t idx, int32_t y,
                          const lv_image_dsc_t *art, const char *name,
                          uint8_t gh, uint8_t yh, const char *sub,
                          uint32_t bg, uint8_t right, uint32_t price,
                          uint32_t old_price, uint8_t qty)
{
    lv_obj_t *r = rect(s_modal_body, 0, y, 192, 36, 10, bg);
    border(r, COL_INK, 1);
    if (art != NULL) {
        icon_img(r, 4, 4, art);
    }
    llabel(r, 40, 3, 104, &lv_font_montserrat_14, COL_INK, name);
    card_hearts(r, 40, 19, gh, yh);
    if (sub != NULL && sub[0] != '\0') {
        int32_t sx = 40 + (int32_t) (gh + yh) * 12 + 3;
        llabel(r, sx, 19, 188 - sx, &lv_font_montserrat_12, COL_SUB, sub);
    }
    card_right(r, right, price, old_price, qty);
    s_pv2_card[idx] = r;
    s_pv2_card_bg[idx] = bg;
    return r;
}

static void box_focus_style(lv_obj_t *o, bool on, bool disabled)
{
    if (on) {
        border(o, COL_SEL, 2);
    } else {
        border(o, disabled ? COL_DIS_LINE : COL_INK, 1);
    }
}

// 重画全部焦点态：卡片选中粉底粉框；页脚控件选中粉框。
static void pv2_mark(uint8_t sel, bool footer)
{
    for (uint8_t i = 0; i < s_focus_n; i += 1) {
        if (s_focus_kind[i] != PV2_F_CARD) {
            continue;
        }
        uint8_t p = s_focus_param[i];
        bool on = (i == sel);
        lv_obj_set_style_bg_color(s_pv2_card[p],
                                  lv_color_hex(on ? COL_CARD_PINK
                                                  : s_pv2_card_bg[p]), 0);
        box_focus_style(s_pv2_card[p], on, false);
    }
    if (!footer) {
        return;
    }
    for (uint8_t i = 0; i < s_focus_n; i += 1) {
        uint8_t k = s_focus_kind[i];
        bool on = (i == sel);
        if (k == PV2_F_PREV) {
            box_focus_style(s_ft_prev, on, s_shop_page == 0);
        } else if (k == PV2_F_NEXT) {
            box_focus_style(s_ft_next, on, s_shop_page + 1 >= s_shop_pages);
        } else if (k == PV2_F_SHOP) {
            box_focus_style(s_ft_shop, on, false);
        } else if (k == PV2_F_BAG) {
            box_focus_style(s_ft_bag, on, false);
        }
    }
}

static void focus_add(uint8_t kind, uint8_t param)
{
    if (s_focus_n >= PV2_F_MAX) {
        return;
    }
    s_focus_kind[s_focus_n] = kind;
    s_focus_param[s_focus_n] = param;
    s_focus_n += 1;
}

// 页脚布局：多页时 ‹ / SHOP / BAG / ›；单页只留居中双段。
static void footer_layout(bool pager)
{
    show(s_mfooter);
    lv_obj_t *sl = (lv_obj_t *) lv_obj_get_child(s_ft_shop, 0);
    lv_obj_t *bl = (lv_obj_t *) lv_obj_get_child(s_ft_bag, 0);
    if (pager) {
        show(s_ft_prev);
        show(s_ft_next);
        lv_obj_set_pos(s_ft_prev, 14, 0);
        lv_obj_set_pos(s_ft_next, 174, 0);
        lv_obj_set_pos(s_ft_shop, 38, 0);
        lv_obj_set_pos(s_ft_bag, 104, 0);
        lv_obj_set_size(s_ft_shop, 66, 20);
        lv_obj_set_size(s_ft_bag, 66, 20);
        lv_obj_set_width(sl, 66);
        lv_obj_set_width(bl, 66);
    } else {
        hide(s_ft_prev);
        hide(s_ft_next);
        lv_obj_set_pos(s_ft_shop, 28, 0);
        lv_obj_set_pos(s_ft_bag, 104, 0);
        lv_obj_set_size(s_ft_shop, 76, 20);
        lv_obj_set_size(s_ft_bag, 76, 20);
        lv_obj_set_width(sl, 76);
        lv_obj_set_width(bl, 76);
    }
}

static void chrome_coins(uint32_t coins)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%luG", (unsigned long) coins);
    int32_t pw = text_w12(buf) + 18;
    if (pw < 40) {
        pw = 40;
    }
    lv_obj_set_pos(s_mcoin, 196 - pw, 12);
    lv_obj_set_size(s_mcoin, pw, 17);
    lv_obj_set_width(s_mcoin_lbl, pw - 13);
    lv_obj_set_pos(s_mcoin_lbl, 11, 2);
    lv_label_set_text(s_mcoin_lbl, buf);
    show(s_mcoin);
}

static void chrome_pager(uint8_t page, uint8_t pages)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned) (page + 1),
             (unsigned) pages);
    lv_label_set_text(s_mp_cur, buf);
    lv_obj_set_style_text_color(s_mp_prev,
                                lv_color_hex(page == 0 ? COL_DIS_TX : COL_INK),
                                0);
    lv_obj_set_style_text_color(s_mp_next,
                                lv_color_hex(page + 1 >= pages ? COL_DIS_TX
                                                               : COL_INK), 0);
    show(s_mp_prev);
    show(s_mp_cur);
    show(s_mp_next);
}

// FOOD 卡片弹层：婴儿仅 Bottle；幼儿+ 为 Meal / Snack，免费直喂。
static void food_build(void)
{
    lv_obj_clean(s_modal_body);
    memset(s_pv2_card, 0, sizeof(s_pv2_card));
    frame_pv2(false);
    lv_label_set_text(s_modal_title, "FOOD");
    s_focus_n = 0;

    if (s_snap.stage == PT_STAGE_BABY) {
        pv2_card(0, 0, pet_ui_card_dsc(PET_UI_CARD_BOTTLE), "Bottle",
                 4, 1, "baby", COL_CARD_WHITE, RIGHT_NONE, 0, 0, 0);
        focus_add(PV2_F_CARD, 0);
    } else {
        pv2_card(0, 0, pet_ui_card_dsc(PET_UI_CARD_MEAL), "Meal",
                 3, 0, "meal", COL_CARD_WHITE, RIGHT_NONE, 0, 0, 0);
        pv2_card(1, 40, pet_ui_card_dsc(PET_UI_CARD_SNACK), "Snack",
                 1, 3, "snack", COL_CARD_WHITE, RIGHT_NONE, 0, 0, 0);
        focus_add(PV2_F_CARD, 0);
        focus_add(PV2_F_CARD, 1);
    }
    if ((uint8_t) s_list_sel >= s_focus_n) {
        s_list_sel = 0;
    }
    pv2_mark((uint8_t) s_list_sel, false);
}

static void modal_open(void)
{
    // 全屏弹层盖住房间：停掉宠物身上的持续动画，弹层期间不产生无谓失效。
    lv_anim_delete(s_creature, NULL);
    // 框架 chrome 由各 builder 的 frame_pv2 / mate_frame / dex_frame 自建。
    show(s_modal);
    lv_obj_move_foreground(s_modal);
}

static void modal_close(void)
{
    hide(s_modal);
    lv_obj_clean(s_modal_body);
    s_mode = MODE_ROOM;
    if (s_snap.stage == PT_STAGE_EGG) {
        egg_idle_anim();
    }
}

// SOUND 页脚：只留居中 ‹ ›（复用 SHOP 页脚容器，隐藏 SHOP/BAG 段）。
static void footer_pager_only(void)
{
    show(s_mfooter);
    hide(s_ft_shop);
    hide(s_ft_bag);
    show(s_ft_prev);
    show(s_ft_next);
    lv_obj_set_pos(s_ft_prev, 88, 0);
    lv_obj_set_pos(s_ft_next, 132, 0);
}

// PV2 batch2：SOUND 卡片弹层。页 0 = Mute/Low/Mid/High 四档音量；
// 页 1 = Quiet ON/OFF 免打扰。当前值挂 OWNED 绿勾，选中卡粉框。
static void sound_build(void)
{
    lv_obj_clean(s_modal_body);
    memset(s_pv2_card, 0, sizeof(s_pv2_card));
    frame_pv2(true);
    footer_pager_only();
    lv_label_set_text(s_modal_title, "SOUND");

    char buf[8];
    snprintf(buf, sizeof(buf), "%u/2", (unsigned) (s_sound_page + 1));
    lv_label_set_text(s_mp_cur, buf);
    lv_obj_set_pos(s_mp_cur, 91, 15);
    show(s_mp_cur);

    s_focus_n = 0;
    if (s_sound_page == 0) {
        static const char *const NAME[4] = { "Mute", "Low", "Mid", "High" };
        static const char *const SUB[4] = {
            "off", "soft", "normal", "loud",
        };
        static const pet_ui_card_t ART[4] = {
            PET_UI_CARD_SPEAKER_OFF, PET_UI_CARD_SPEAKER,
            PET_UI_CARD_SPEAKER, PET_UI_CARD_SPEAKER,
        };
        uint8_t cur = pet_app_volume();
        for (uint8_t i = 0; i < 4; i += 1) {
            pv2_card(i, (int32_t) i * 40, pet_ui_card_dsc(ART[i]), NAME[i],
                     0, 0, SUB[i], COL_CARD_WHITE,
                     i == cur ? RIGHT_OWNED : RIGHT_NONE, 0, 0, 0);
            focus_add(PV2_F_CARD, i);
        }
    } else {
        bool quiet = pet_app_quiet();
        pv2_card(0, 0, pet_ui_card_dsc(PET_UI_CARD_CRESCENT), "Quiet ON",
                 0, 0, "quiet hrs", COL_CARD_WHITE,
                 quiet ? RIGHT_OWNED : RIGHT_NONE, 0, 0, 0);
        pv2_card(1, 40, pet_ui_card_dsc(PET_UI_CARD_SNUB), "Quiet OFF",
                 0, 0, "all alerts", COL_CARD_WHITE,
                 !quiet ? RIGHT_OWNED : RIGHT_NONE, 0, 0, 0);
        focus_add(PV2_F_CARD, 0);
        focus_add(PV2_F_CARD, 1);
    }
    focus_add(PV2_F_PREV, 0);
    focus_add(PV2_F_NEXT, 0);

    if ((uint8_t) s_list_sel >= s_focus_n) {
        s_list_sel = 0;
    }
    // pv2_mark() 的端点失效判断读商店页码；SOUND 复用同一套样式逻辑。
    s_shop_page = s_sound_page;
    s_shop_pages = 2;
    pv2_mark((uint8_t) s_list_sel, true);
}

// ---------------------------------------------------------------------------
// PV2 batch2：STATUS 卡片弹层（INFO / VITALS / CARE，卡片全部只读）
// ---------------------------------------------------------------------------

static const char *stage_lower(pt_stage_t st)
{
    switch (st) {
    case PT_STAGE_EGG: return "egg";
    case PT_STAGE_BABY: return "baby";
    case PT_STAGE_CHILD: return "child";
    case PT_STAGE_TEEN: return "teen";
    case PT_STAGE_ADULT: return "adult";
    case PT_STAGE_SENIOR: return "senior";
    default: return "--";
    }
}

static lv_obj_t *status_card(int32_t y, pet_ui_card_t art)
{
    lv_obj_t *r = rect(s_modal_body, 0, y, 192, 36, 10, COL_CARD_WHITE);
    border(r, COL_INK, 1);
    icon_img(r, 4, 4, pet_ui_card_dsc(art));
    return r;
}

static void status_page_info(void)
{
    char buf[48];

    // 身份卡：物种名 + 阶段/年龄副行；成年右置性格，生病时副行让位给红字。
    lv_obj_t *c = status_card(0, PET_UI_CARD_PETFACE);
    llabel(c, 40, 3, 110, &lv_font_montserrat_14, COL_INK,
           species_name(s_snap.species));
    if (s_snap.sick) {
        llabel(c, 40, 19, 110, &lv_font_montserrat_12, COL_RED, "sick now");
    } else {
        snprintf(buf, sizeof(buf), "%s - age %ud",
                 stage_lower(s_snap.stage), (unsigned) s_snap.age_days);
        llabel(c, 40, 19, 110, &lv_font_montserrat_12, COL_SUB, buf);
        if (s_snap.stage >= PT_STAGE_ADULT) {
            rlabel12(c, 188, 4, COL_SUB,
                     pt_personality_name(
                         (pt_personality_t) s_snap.genome.personality));
        }
    }

    c = status_card(40, PET_UI_CARD_SCALE);
    llabel(c, 40, 3, 80, &lv_font_montserrat_14, COL_INK, "Weight");
    snprintf(buf, sizeof(buf), "%u g", (unsigned) s_snap.weight);
    rlabel12(c, 188, 6, COL_INK, buf);

    c = status_card(80, PET_UI_CARD_BOND);
    llabel(c, 40, 3, 80, &lv_font_montserrat_14, COL_INK, "Bond");
    snprintf(buf, sizeof(buf), "%u", (unsigned) s_snap.bond);
    rlabel12(c, 188, 6, COL_SEL, buf);

    c = status_card(120, PET_UI_CARD_WALLET);
    snprintf(buf, sizeof(buf), "%lu G", (unsigned long) s_esnap.coins);
    llabel(c, 40, 3, 120, &lv_font_montserrat_14, COL_INK, buf);
    snprintf(buf, sizeof(buf), "%u shells", (unsigned) s_esnap.shells);
    llabel(c, 40, 19, 120, &lv_font_montserrat_12, COL_SUB, buf);
}

static void status_page_vitals(void)
{
    static const char *const NAME[4] = { "Food", "Fun", "Health", "Energy" };
    static const pet_ui_card_t ART[4] = {
        PET_UI_CARD_FOODBOWL, PET_UI_CARD_FUNFACE,
        PET_UI_CARD_HEALTH, PET_UI_CARD_BOLT,
    };
    static const uint32_t BAR_COL[4] = {
        COL_GREEN, COL_YELLOW, COL_RED, COL_BLUE,
    };
    const uint8_t val[4] = {
        s_snap.fullness, s_snap.happiness, s_snap.health, s_snap.energy,
    };
    char buf[8];
    for (uint8_t i = 0; i < 4; i += 1) {
        int32_t y = (int32_t) i * 40;
        lv_obj_t *c = status_card(y, ART[i]);
        llabel(c, 40, 3, 100, &lv_font_montserrat_14, COL_INK, NAME[i]);
        meter100(c, 40, 22, val[i], BAR_COL[i]);
        snprintf(buf, sizeof(buf), "%u", (unsigned) val[i]);
        rlabel12(c, 188, 19, COL_INK, buf);
    }
}

static void status_page_care(void)
{
    char buf[48];

    lv_obj_t *c = status_card(0, PET_UI_CARD_BANDAID);
    llabel(c, 40, 3, 120, &lv_font_montserrat_14, COL_INK, "Misses");
    snprintf(buf, sizeof(buf), "small %d - big %d",
             (int) s_snap.ledger.small, (int) s_snap.ledger.big);
    llabel(c, 40, 19, 140, &lv_font_montserrat_12, COL_SUB, buf);

    c = status_card(40, PET_UI_CARD_MEDAL);
    llabel(c, 40, 3, 120, &lv_font_montserrat_14, COL_INK, "Top-ups");
    snprintf(buf, sizeof(buf), "full %u - fun %u",
             (unsigned) s_snap.ledger.full_topups,
             (unsigned) s_snap.ledger.happy_topups);
    llabel(c, 40, 19, 140, &lv_font_montserrat_12, COL_SUB, buf);

    c = status_card(80, PET_UI_CARD_SNUB);
    llabel(c, 40, 10, 80, &lv_font_montserrat_14, COL_INK, "Snubs");
    snprintf(buf, sizeof(buf), "%u",
             (unsigned) s_snap.ledger.perfunctory);
    rlabel12(c, 188, 12, COL_INK, buf);

    // Skills：M/B/A 三条 30x6 迷你蓝槽（槽位比例即数值，不写数字）。
    c = status_card(120, PET_UI_CARD_STAR);
    llabel(c, 40, 3, 100, &lv_font_montserrat_14, COL_INK, "Skills");
    static const char *const TAG[3] = { "M", "B", "A" };
    const uint8_t sk[3] = {
        s_snap.skill[PT_SKILL_MIND],
        s_snap.skill[PT_SKILL_BODY],
        s_snap.skill[PT_SKILL_ART],
    };
    for (uint8_t i = 0; i < 3; i += 1) {
        int32_t lx = 38 + (int32_t) i * 44;
        int32_t bx = lx + 9;
        llabel(c, lx, 16, 8, &lv_font_montserrat_12, COL_INK, TAG[i]);
        rect(c, bx, 20, 30, 6, 3, COL_PIP_OFF);
        int32_t fw = (int32_t) sk[i] * 30 / 99;
        if (fw < 3) {
            fw = 3;
        }
        rect(c, bx, 20, fw, 6, 3, COL_BLUE);
    }
}

// 页脚五控件重绘：‹ INFO VITALS CARE ›。激活段粉底，焦点段 2px 粉框，
// 端点箭头在首页/末页变灰。
static void status_footer_refresh(void)
{
    lv_obj_t *w[5];
    bool disabled[5];
    w[0] = s_sf_prev;
    w[1] = s_sf_tab[0];
    w[2] = s_sf_tab[1];
    w[3] = s_sf_tab[2];
    w[4] = s_sf_next;
    disabled[0] = (s_status_page == 0);
    disabled[1] = false;
    disabled[2] = false;
    disabled[3] = false;
    disabled[4] = (s_status_page == 2);

    for (uint8_t i = 0; i < 5; i += 1) {
        bool on = ((int) i == s_status_focus);
        bool is_tab = (i >= 1 && i <= 3);
        bool active = is_tab && ((int) (i - 1) == s_status_page);
        uint32_t fill = is_tab ? (active ? COL_TAB_ON : COL_DOCK_BG)
                               : (disabled[i] ? COL_DIS_BG : COL_DOCK_BG);
        lv_obj_set_style_bg_color(w[i], lv_color_hex(fill), 0);
        if (on) {
            border(w[i], COL_SEL, 2);
        } else if (active) {
            border(w[i], COL_SEL, 1);
        } else {
            border(w[i], disabled[i] ? COL_DIS_LINE : COL_INK, 1);
        }
        lv_obj_t *txt = (lv_obj_t *) lv_obj_get_child(w[i], 0);
        lv_obj_set_style_text_color(
            txt, lv_color_hex(disabled[i] ? COL_DIS_TX : COL_INK), 0);
    }
}

static void status_build(void)
{
    lv_obj_clean(s_modal_body);
    frame_pv2(false);
    show(s_sfooter);

    lv_label_set_text(s_modal_title, "STATUS");
    if (s_status_page == 0) {
        status_page_info();
    } else if (s_status_page == 1) {
        status_page_vitals();
    } else {
        status_page_care();
    }
    status_footer_refresh();
}

// ---------------------------------------------------------------------------
// 商店 / 背包 / 每日签到（P1 经济）
// ---------------------------------------------------------------------------

static void style_open(void);

// 物品 → PV2 卡片缩略图。
static const lv_image_dsc_t *item_card_art(pt_item_t id)
{
    switch (id) {
    case PT_ITEM_RICEBALL:  return pet_ui_card_dsc(PET_UI_CARD_RICEBALL);
    case PT_ITEM_BISCUIT:   return pet_ui_card_dsc(PET_UI_CARD_BISCUIT);
    case PT_ITEM_BENTO:     return pet_ui_card_dsc(PET_UI_CARD_BENTO);
    case PT_ITEM_PUDDING:   return pet_ui_card_dsc(PET_UI_CARD_PUDDING);
    case PT_ITEM_CAKE:      return pet_ui_card_dsc(PET_UI_CARD_CAKE);
    case PT_ITEM_CREPE:     return pet_ui_card_dsc(PET_UI_CARD_CREPE);
    case PT_ITEM_WATERMELON: return pet_ui_card_dsc(PET_UI_CARD_WATERMELON);
    case PT_ITEM_BALL:      return pet_ui_card_dsc(PET_UI_CARD_BALL);
    case PT_ITEM_MUSICBOX:  return pet_ui_card_dsc(PET_UI_CARD_MUSICBOX);
    case PT_ITEM_BUBBLES:   return pet_ui_card_dsc(PET_UI_CARD_BUBBLES);
    default:                return NULL;
    }
}

// 效果心强度（绿=饱腹 / 黄=开心）：与定稿稿一致的风味强度，非精确数值。
static void item_pips(pt_item_t id, uint8_t *g, uint8_t *yh)
{
    *g = 0;
    *yh = 0;
    switch (id) {
    case PT_ITEM_RICEBALL:  *g = 1; break;
    case PT_ITEM_BENTO:     *g = 2; break;
    case PT_ITEM_WATERMELON: *g = 2; break;
    case PT_ITEM_BISCUIT:   *yh = 2; break;
    case PT_ITEM_PUDDING:  *yh = 2; break;
    case PT_ITEM_CAKE:     *yh = 3; break;
    case PT_ITEM_CREPE:     *yh = 3; break;
    default: break;
    }
}

// 把当前标签页下的条目展平成 s_seq 序列（签到 / Style 为虚拟行），重算页数。
static void shop_seq_build(const uint16_t *items, uint8_t total, bool bonus)
{
    s_seq_n = 0;
    if (!s_shop_bag) {
        if (bonus) {
            s_seq_kind[s_seq_n] = SEQ_BONUS;
            s_seq_id[s_seq_n] = PT_ITEM_NONE;
            s_seq_n += 1;
        }
        // PV2 batch6：幼儿期即可进入看锁定行（穿戴/家具均 TEEN 解锁）。
        if (s_snap.stage >= PT_STAGE_CHILD) {
            s_seq_kind[s_seq_n] = SEQ_STYLE;
            s_seq_id[s_seq_n] = PT_ITEM_NONE;
            s_seq_n += 1;
        }
    }
    for (uint8_t i = 0; i < total && s_seq_n < SEQ_MAX; i += 1) {
        s_seq_kind[s_seq_n] = SEQ_ITEM;
        s_seq_id[s_seq_n] = items[i];
        s_seq_n += 1;
    }
    s_shop_pages = (uint8_t) ((s_seq_n + 3) / 4);
    if (s_shop_pages == 0) {
        s_shop_pages = 1;
    }
}

static uint8_t focus_index_of(uint8_t kind)
{
    for (uint8_t i = 0; i < s_focus_n; i += 1) {
        if (s_focus_kind[i] == kind) {
            return i;
        }
    }
    return 0;
}

static void shop_empty_bag(void)
{
    // 48px 空包图（28 原图 2x，绕中心缩放）。
    lv_obj_t *img = icon_img(s_modal_body, 82, 42,
                             pet_ui_card_dsc(PET_UI_CARD_EMPTY));
    lv_obj_set_style_transform_pivot_x(img, 14, 0);
    lv_obj_set_style_transform_pivot_y(img, 14, 0);
    lv_obj_set_style_transform_scale(img, 512, 0);
    label(s_modal_body, 0, 88, 192, &lv_font_montserrat_14, COL_INK,
          "The bag is empty");
    label(s_modal_body, 0, 110, 192, &lv_font_montserrat_12, COL_SUB,
          "Pick treats in Shop");
}

// 单件物品卡（货架 / 背包两种右挂件）。
static void shop_item_card(uint8_t slot, uint16_t id16)
{
    pt_item_t id = (pt_item_t) id16;
    const pt_item_def_t *d = pt_item_def(id);
    if (d == NULL) {
        return;
    }
    uint8_t gh = 0, yh = 0;
    item_pips(id, &gh, &yh);
    const char *sub = "";
    uint8_t right = RIGHT_NONE;
    uint32_t price = 0, old_price = 0;
    uint8_t qty = 0;

    if (s_shop_bag) {
        if (d->kind == PT_ITEM_KIND_TOY) {
            sub = "today";
            right = RIGHT_PLAY;
            qty = (uint8_t) (PT_ECON_TOY_USES_DAY - s_esnap.toy_uses[id]);
        } else {
            sub = "OK to eat";
            right = RIGHT_QTY;
            qty = pt_econ_count(&s_esnap, id);
        }
    } else {
        sub = (d->kind == PT_ITEM_KIND_TOY) ? "3 plays / day"
              : (d->food_class == PT_FOOD_MEAL ? "meal" : "snack");
        bool owned = d->kind == PT_ITEM_KIND_TOY
                     && pt_econ_count(&s_esnap, id) > 0;
        if (owned) {
            right = RIGHT_OWNED;
        } else {
            price = pt_econ_item_price_now(d->price, s_snap.day_id);
            old_price = d->price;
            if (pt_econ_sale_pct(s_snap.day_id, false) >= 20 && price < old_price) {
                right = RIGHT_SALE;
            } else {
                right = RIGHT_PRICE;
            }
        }
    }
    pv2_card(slot, (int32_t) slot * 40, item_card_art(id), d->name,
             gh, yh, sub, COL_CARD_WHITE, right, price, old_price, qty);
}

// 把当前页（s_shop_page）铺成卡片 + 页脚，并重建扁平焦点槽。
static void shop_build(void)
{
    lv_obj_clean(s_modal_body);
    memset(s_pv2_card, 0, sizeof(s_pv2_card));
    s_focus_n = 0;

    frame_pv2(true);
    lv_label_set_text(s_modal_title, s_shop_bag ? "BAG" : "SHOP");
    chrome_coins(s_esnap.coins);

    uint16_t items[PT_ECON_INV_SLOTS];
    uint8_t total = 0;
    bool bonus = false;
    if (!s_shop_bag) {
        total = pt_econ_shop_build(&s_esnap, s_snap.stage, s_snap.minute,
                                  items, (uint8_t) (sizeof(items)
                                                    / sizeof(items[0])));
        bonus = s_shop_page == 0
                && pt_econ_can_checkin(&s_esnap, s_snap.day_id);
    } else {
        for (uint8_t i = 0; i < PT_ECON_INV_SLOTS; i += 1) {
            if (s_esnap.inv[i].qty > 0 && s_esnap.inv[i].item != PT_ITEM_NONE) {
                items[total++] = s_esnap.inv[i].item;
            }
        }
    }
    shop_seq_build(items, total, bonus);
    if (s_shop_page >= s_shop_pages) {
        s_shop_page = (uint8_t) (s_shop_pages - 1);
    }

    uint8_t start = (uint8_t) (s_shop_page * 4);
    if (s_shop_bag && s_seq_n == 0) {
        shop_empty_bag();
    } else {
        for (uint8_t i = 0; i < 4 && start + i < s_seq_n; i += 1) {
            uint8_t seq = (uint8_t) (start + i);
            uint8_t kind = s_seq_kind[seq];
            if (kind == SEQ_BONUS) {
                pv2_card(i, (int32_t) i * 40,
                         pet_ui_card_dsc(PET_UI_CARD_GIFT), "Daily bonus",
                         0, 0, "+20 G every day", COL_CARD_GIFT,
                         RIGHT_GET, 0, 0, 0);
            } else if (kind == SEQ_STYLE) {
                lv_obj_t *r = pv2_card(i, (int32_t) i * 40, NULL, "Style",
                                        0, 0, "room & wear",
                                        COL_CARD_GREENBG, RIGHT_GO, 0, 0, 0);
                icon_img(r, 6, 5, pet_ui_deco_dsc(PET_UI_DECO_PLANT));
            } else {
                shop_item_card(i, s_seq_id[seq]);
            }
            focus_add(PV2_F_CARD, i);
        }
    }

    // 页脚：翻页键 + SHOP/BAG 段；激活段粉底。
    footer_layout(s_shop_pages > 1);
    lv_obj_set_style_bg_color(s_ft_shop,
                              lv_color_hex(s_shop_bag ? COL_DOCK_BG : COL_TAB_ON),
                              0);
    lv_obj_set_style_bg_color(s_ft_bag,
                              lv_color_hex(s_shop_bag ? COL_TAB_ON : COL_DOCK_BG),
                              0);
    if (s_shop_pages > 1) {
        chrome_pager(s_shop_page, s_shop_pages);
        focus_add(PV2_F_PREV, 0);
    }
    focus_add(PV2_F_SHOP, 0);
    focus_add(PV2_F_BAG, 0);
    if (s_shop_pages > 1) {
        focus_add(PV2_F_NEXT, 0);
    }

    if (s_shop_sel >= s_focus_n) {
        s_shop_sel = 0;
    }
    pv2_mark(s_shop_sel, true);
}

static void shop_open(void)
{
    s_shop_bag = false;
    s_shop_page = 0;
    s_shop_sel = 0;
    s_shop_dirty = false;
    s_mode = MODE_SHOP;
    modal_open();
    shop_build();
    pet_audio_play(SND_CONFIRM);
}

static void shop_buy_msg(pt_econ_rv_t rv)
{
    switch (rv) {
    case PT_ECON_NO_MONEY: toast_show("Not enough G"); break;
    case PT_ECON_OWNED: toast_show("Already owned"); break;
    case PT_ECON_LOCKED: toast_show("Locked"); break;
    case PT_ECON_NOT_ON_SHELF: toast_show("Not on shelf"); break;
    default: toast_show("Can't buy"); break;
    }
}

static void shop_activate_seq(uint8_t seq)
{
    uint8_t kind = s_seq_kind[seq];
    pt_item_t item = (pt_item_t) s_seq_id[seq];

    if (kind == SEQ_STYLE) {
        style_open();
        return;
    }
    if (kind == SEQ_BONUS) {
        pet_app_econ_checkin();
        toast_show("20 G  thank you!");
        s_shop_dirty = true;
        return;
    }

    if (!s_shop_bag) {
        // 对快照副本做一次"试买"：失败给准确提示且绝不二次扣款。
        pt_econ_t trial = s_esnap;
        pt_econ_rv_t rv = pt_econ_buy(&trial, s_snap.stage, s_snap.minute,
                                      item);
        if (rv != PT_ECON_OK) {
            shop_buy_msg(rv);
            pet_audio_play(SND_CANCEL);
            return;
        }
        pet_app_econ_buy(item);
        s_shop_dirty = true;
        toast_show("Bought!");
        return;
    }

    const pt_item_def_t *d = pt_item_def(item);
    if (d != NULL && d->kind == PT_ITEM_KIND_TOY) {
        pt_econ_t trial = s_esnap;
        pt_econ_rv_t rv = pt_econ_use_toy(&trial, item);
        if (rv != PT_ECON_OK) {
            toast_show(rv == PT_ECON_USES_EXHAUSTED ? "Played out today"
                                                    : "Can't play");
            pet_audio_play(SND_CANCEL);
            return;
        }
        pet_app_econ_play(item);
        s_shop_dirty = true;
        toast_show("Fun!");
        return;
    }

    pt_econ_t trial = s_esnap;
    if (pt_econ_take_food(&trial, item) != PT_ECON_OK) {
        toast_show("No stock");
        pet_audio_play(SND_CANCEL);
        return;
    }
    pet_app_econ_eat(item);
    pet_audio_play(SND_EAT);
    s_mood = MOOD_EAT;
    s_mood_until = now_ms() + 800;
    modal_close();
}

static void shop_activate_focus(void)
{
    uint8_t kind = s_focus_kind[s_shop_sel];
    uint8_t param = s_focus_param[s_shop_sel];

    if (kind == PV2_F_CARD) {
        uint8_t seq = (uint8_t) (s_shop_page * 4 + param);
        if (seq < s_seq_n) {
            shop_activate_seq(seq);
        }
        return;
    }
    if (kind == PV2_F_PREV) {
        if (s_shop_page > 0) {
            s_shop_page -= 1;
            s_shop_sel = 0;
            shop_build();
            pet_audio_play(SND_CONFIRM);
        }
        return;
    }
    if (kind == PV2_F_NEXT) {
        if (s_shop_page + 1 < s_shop_pages) {
            s_shop_page += 1;
            s_shop_sel = 0;
            shop_build();
            pet_audio_play(SND_CONFIRM);
        }
        return;
    }
    if (kind == PV2_F_SHOP || kind == PV2_F_BAG) {
        bool want_bag = (kind == PV2_F_BAG);
        if (s_shop_bag != want_bag) {
            s_shop_bag = want_bag;
            s_shop_page = 0;
            shop_build();
            s_shop_sel = focus_index_of(want_bag ? PV2_F_BAG : PV2_F_SHOP);
            pv2_mark(s_shop_sel, true);
            pet_audio_play(SND_CONFIRM);
        }
    }
}

static void handle_shop_key(pet_ui_action_t act)
{
    if (act == PET_UI_ACT_PREV) {
        s_shop_sel = (uint8_t) ((s_shop_sel + s_focus_n - 1) % s_focus_n);
        pv2_mark(s_shop_sel, true);
    } else if (act == PET_UI_ACT_NEXT) {
        s_shop_sel = (uint8_t) ((s_shop_sel + 1) % s_focus_n);
        pv2_mark(s_shop_sel, true);
    } else if (act == PET_UI_ACT_CONFIRM) {
        shop_activate_focus();
    } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        pet_audio_play(SND_CANCEL);
        modal_close();
    }
}

// ---------------------------------------------------------------------------
// Style 弹层：服装 / 功能家具 / 房间主题（PV2 batch6，定稿 pv2-style.html）
// 单环：当前档全部列表行 -> WEAR -> ROOM -> THEME；长按 OK 回 SHOP，双击回房间。
// ---------------------------------------------------------------------------

static const char *outfit_slot_tag(pt_slot_t slot)
{
    switch (slot) {
    case PT_SLOT_HAT: return "Hat";
    case PT_SLOT_FACE: return "Face";
    case PT_SLOT_COLLAR: return "Neck";
    case PT_SLOT_HELD: return "Hand";
    default: return "?";
    }
}

static uint8_t style_tab_count(void)
{
    if (s_dec_tab == 0) {
        return (uint8_t) (PT_OUTFIT_COUNT - 1);
    }
    if (s_dec_tab == 1) {
        return (uint8_t) PT_FURN_COUNT;
    }
    return (uint8_t) PT_THEME_COUNT;
}

// 页眉双钱包：贝壳 chip（蓝）+ 金币 chip（复用 chrome_coins）。
static void style_chrome(void)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%uS", (unsigned) s_esnap.shells);
    lv_label_set_text(s_tshell_lbl, buf);
    chrome_coins(s_esnap.coins);
}

// 右缘价格小丸（行内坐标，右缘对齐 x=184）：G=黄底棕字，S=蓝底蓝字。
static void style_price(lv_obj_t *row, const char *txt, bool shells)
{
    int32_t tw = text_w12(txt);
    int32_t pw = tw + 12;
    if (pw < 30) {
        pw = 30;
    }
    lv_obj_t *p = rect(row, 184 - pw, 3, pw, 16, 8,
                       shells ? COL_PILL_BLUE : COL_COIN_BG);
    border(p, shells ? COL_BLUE : COL_YELLOW, 1);
    label(p, 0, 2, pw, &lv_font_montserrat_12,
          shells ? COL_BLUE : COL_BROWN, txt);
}

static void style_row_wear(lv_obj_t *r, uint8_t id)
{
    pt_outfit_t oid = (pt_outfit_t) (id + 1);
    const pt_outfit_def_t *od = pt_outfit_def(oid);
    bool locked = s_snap.stage < od->min_stage;
    llabel(r, 10, 4, 28, &lv_font_montserrat_12,
           locked ? COL_HINT : COL_SUB, outfit_slot_tag(od->slot));
    llabel(r, 40, 4, 96, &lv_font_montserrat_12,
           locked ? COL_DIS_TX : COL_INK, od->name);
    if (locked) {
        rlabel12(r, 184, 4, COL_DIS_TX, "Locked");
        return;
    }
    char buf[20];
    if (pt_decor_worn(&s_dsnap, od->slot) == oid) {
        rlabel12(r, 184, 4, COL_GREEN, "ON");
    } else if (pt_decor_owns_outfit(&s_dsnap, oid)) {
        rlabel12(r, 184, 4, COL_BLUE, "Wear");
    } else if (od->shells > 0) {
        snprintf(buf, sizeof(buf), "%uS", (unsigned) od->shells);
        style_price(r, buf, true);
    } else {
        snprintf(buf, sizeof(buf), "%luG",
                 (unsigned long) pt_decor_outfit_price_g(od,
                                                          s_snap.day_id));
        style_price(r, buf, false);
    }
}

static void style_row_furn(lv_obj_t *r, uint8_t id)
{
    const pt_furn_def_t *fd = pt_furn_def((pt_furn_t) id);
    bool locked = s_snap.stage < fd->min_stage;
    llabel(r, 10, 4, 120, &lv_font_montserrat_12,
           locked ? COL_DIS_TX : COL_INK, fd->name);
    if (locked) {
        rlabel12(r, 184, 4, COL_DIS_TX, "Locked");
        return;
    }
    char buf[20];
    if (pt_decor_is_placed(&s_dsnap, (pt_furn_t) id)) {
        rlabel12(r, 184, 4, COL_GREEN, "PLACED");
    } else if (pt_decor_owns_furn(&s_dsnap, (pt_furn_t) id)) {
        rlabel12(r, 184, 4, COL_BLUE, "Place");
    } else {
        snprintf(buf, sizeof(buf), "%luG",
                 (unsigned long) pt_decor_furn_price_g(fd, s_snap.day_id));
        style_price(r, buf, false);
    }
}

static void style_row_theme(lv_obj_t *r, uint8_t id)
{
    const pt_theme_def_t *td = pt_theme_def((pt_theme_t) id);
    // 14x14 r4 壁纸色板（深色主题照常 1px 墨边）。
    rect(r, 10, 4, 14, 14, 4, td->wall);
    border(r, COL_INK, 1);
    llabel(r, 30, 4, 90, &lv_font_montserrat_12, COL_INK, td->name);
    char buf[20];
    if ((pt_theme_t) s_dsnap.theme == (pt_theme_t) id) {
        rlabel12(r, 184, 4, COL_GREEN, "ON");
    } else if (pt_decor_owns_theme(&s_dsnap, (pt_theme_t) id)) {
        rlabel12(r, 184, 4, COL_BLUE, "Use");
    } else if (td->shells > 0) {
        snprintf(buf, sizeof(buf), "%uS", (unsigned) td->shells);
        style_price(r, buf, true);
    } else {
        snprintf(buf, sizeof(buf), "%uG", (unsigned) td->price);
        style_price(r, buf, false);
    }
}

// 行焦点 + 页脚三档着色（当前档粉填充；焦点 2px 粉框）。
static void style_mark(void)
{
    uint8_t n = style_tab_count();
    for (uint8_t i = 0; i < n; i += 1) {
        bool on = (s_dec_sel == i);
        border(s_t_row[i], on ? COL_SEL : COL_HAIR, on ? 2 : 1);
    }
    for (uint8_t k = 0; k < 3; k += 1) {
        bool cur = (k == s_dec_tab);
        bool on = (s_dec_sel == (uint8_t) (n + k));
        lv_obj_set_style_bg_color(s_tf_tab[k],
            lv_color_hex(cur ? COL_TAB_ON : COL_DOCK_BG), 0);
        border(s_tf_tab[k], on ? COL_SEL : (cur ? COL_TAB_ON : COL_INK),
               on ? 2 : 1);
    }
}

static void style_build(void)
{
    lv_obj_clean(s_modal_body);
    memset(s_t_row, 0, sizeof(s_t_row));
    frame_pv2(false);
    show(s_tfooter);
    show(s_tshell);
    lv_label_set_text(s_modal_title, "STYLE");
    style_chrome();

    uint8_t n = style_tab_count();
    for (uint8_t i = 0; i < n; i += 1) {
        lv_obj_t *r = rect(s_modal_body, 0, (int32_t) i * 24, 192, 22,
                           8, COL_CARD_WHITE);
        s_t_row[i] = r;
        if (s_dec_tab == 0) {
            style_row_wear(r, i);
        } else if (s_dec_tab == 1) {
            style_row_furn(r, i);
        } else {
            style_row_theme(r, i);
        }
    }

    // 单环 0..n-1 行，n..n+2 页脚三档；越界夹回首行。
    if (s_dec_sel > (uint8_t) (n + 2)) {
        s_dec_sel = 0;
    }
    style_mark();
}

static void style_open(void)
{
    s_dec_tab = 0;
    s_dec_sel = 0;
    s_dec_dirty = false;
    s_mode = MODE_DECOR;
    modal_open();
    style_build();
    pet_audio_play(SND_CONFIRM);
}

static void decor_buy_msg(pt_decor_rv_t rv, bool shells)
{
    switch (rv) {
    case PT_DECOR_NO_MONEY:
        toast_show(shells ? "Not enough shells" : "Not enough G");
        break;
    case PT_DECOR_LOCKED: toast_show("Locked"); break;
    case PT_DECOR_OWNED: toast_show("Already owned"); break;
    case PT_DECOR_PLACE_CAP: toast_show("3 furniture max"); break;
    default: toast_show("Can't do that"); break;
    }
}

// OK 激活当前行（s_dec_sel < n 时调用）：买下即用/穿戴、再点脱下/收起。
static void style_activate_row(void)
{
    uint8_t id = s_dec_sel;

    if (s_dec_tab == 0) {
        pt_outfit_t oid = (pt_outfit_t) (id + 1);
        const pt_outfit_def_t *od = pt_outfit_def(oid);
        if (od == NULL) {
            return;
        }
        if (!pt_decor_owns_outfit(&s_dsnap, oid)) {
            pt_decor_t dt = s_dsnap;
            pt_econ_t et = s_esnap;
            pt_decor_rv_t rv = pt_decor_buy_outfit(&dt, &et, s_snap.stage,
                                                   s_snap.day_id, oid);
            if (rv != PT_DECOR_OK) {
                decor_buy_msg(rv, od->shells > 0);
                pet_audio_play(SND_CANCEL);
                return;
            }
            // 买下即穿上（再点一次可脱下）。
            pet_app_decor_buy(0, (uint8_t) oid);
            pet_app_decor_equip((uint8_t) oid);
            set_msg("New look!", 900);
        } else if (pt_decor_worn(&s_dsnap, od->slot) == oid) {
            pet_app_decor_unequip(od->slot);
            set_msg("Taken off", 800);
        } else {
            pet_app_decor_equip((uint8_t) oid);
            set_msg("Looking good!", 800);
        }
        s_dec_dirty = true;
        return;
    }

    if (s_dec_tab == 1) {
        pt_furn_t fid = (pt_furn_t) id;
        const pt_furn_def_t *fd = pt_furn_def(fid);
        if (fd == NULL) {
            return;
        }
        if (pt_decor_is_placed(&s_dsnap, fid)) {
            pet_app_decor_unplace((uint8_t) fid);
            set_msg("Put away", 800);
        } else if (pt_decor_owns_furn(&s_dsnap, fid)) {
            pt_decor_t dt = s_dsnap;
            if (pt_decor_place(&dt, fid) != PT_DECOR_OK) {
                toast_show("3 furniture max");
                pet_audio_play(SND_CANCEL);
                return;
            }
            pet_app_decor_place((uint8_t) fid);
            set_msg("Placed!", 800);
        } else {
            pt_decor_t dt = s_dsnap;
            pt_econ_t et = s_esnap;
            pt_decor_rv_t rv = pt_decor_buy_furn(&dt, &et, s_snap.stage,
                                                 s_snap.day_id, fid);
            if (rv != PT_DECOR_OK) {
                decor_buy_msg(rv, false);
                pet_audio_play(SND_CANCEL);
                return;
            }
            pet_app_decor_buy(1, (uint8_t) fid);
            set_msg("Bought!", 900);
        }
        s_dec_dirty = true;
        return;
    }

    // 主题页
    pt_theme_t tid = (pt_theme_t) id;
    const pt_theme_def_t *td = pt_theme_def(tid);
    if (td == NULL || (pt_theme_t) s_dsnap.theme == tid) {
        return;
    }
    if (!pt_decor_owns_theme(&s_dsnap, tid)) {
        pt_decor_t dt = s_dsnap;
        pt_econ_t et = s_esnap;
        pt_decor_rv_t rv = pt_decor_buy_theme(&dt, &et, tid);
        if (rv != PT_DECOR_OK) {
            decor_buy_msg(rv, td->shells > 0);
            pet_audio_play(SND_CANCEL);
            return;
        }
        // 买下即启用成套主题。
        pet_app_decor_buy(2, (uint8_t) tid);
        pet_app_decor_set_theme((uint8_t) tid);
        set_msg("Room restyled!", 1000);
    } else {
        pet_app_decor_set_theme((uint8_t) tid);
        set_msg("Restyle!", 800);
    }
    s_dec_dirty = true;
}

static void handle_style_key(pet_ui_action_t act)
{
    uint8_t n = style_tab_count();
    uint8_t ring = (uint8_t) (n + 3);
    if (act == PET_UI_ACT_PREV) {
        s_dec_sel = (uint8_t) ((s_dec_sel + ring - 1) % ring);
        style_mark();
    } else if (act == PET_UI_ACT_NEXT) {
        s_dec_sel = (uint8_t) ((s_dec_sel + 1) % ring);
        style_mark();
    } else if (act == PET_UI_ACT_CONFIRM) {
        if (s_dec_sel >= n) {
            // 页脚档丸：直接切到该档，行焦点归 0。
            s_dec_tab = (uint8_t) (s_dec_sel - n);
            s_dec_sel = 0;
            style_build();
            pet_audio_play(SND_CONFIRM);
        } else {
            style_activate_row();
        }
    } else if (act == PET_UI_ACT_BACK) {
        pet_audio_play(SND_CANCEL);
        shop_open();   // 长按：返回商店/背包弹层
    } else if (act == PET_UI_ACT_MENU) {
        pet_audio_play(SND_CANCEL);
        modal_close(); // 双击：直接回房间
    }
}

// PV2 batch6：每日签到（定稿 pv2-checkin.html）。主体仪式布局 + 单颗 CLAIM
// 丸（环长 1）；OK 领取，长按/双击跳过。几何为主体 (8,42) 192x156 坐标。
static void checkin_build(void)
{
    lv_obj_clean(s_modal_body);
    frame_pv2(false);
    show(s_cclaim);
    lv_label_set_text(s_modal_title, "DAILY BONUS");
    lv_label_set_text(s_mback, "hold OK - skip");

    char buf[24];
    uint16_t day = (uint16_t) (s_esnap.streak + 1);
    bool week = day >= 7;

    // 56x56 礼品图（28px 位图整倍放大，pivot 在 28 盒中心），body (68,10)。
    lv_obj_t *gift = icon_img(s_modal_body, 68, 10,
                              pet_ui_card_dsc(PET_UI_CARD_GIFT));
    lv_obj_set_style_transform_pivot_x(gift, 14, 0);
    lv_obj_set_style_transform_pivot_y(gift, 14, 0);
    lv_obj_set_style_transform_scale(gift, 512, 0);

    snprintf(buf, sizeof(buf), "Day %u of 7", (unsigned) day);
    label(s_modal_body, 0, 64, 192, &lv_font_montserrat_14, COL_INK,
          week ? "7-day streak!" : buf);

    // 7 连签小灯（总宽 85 居中，body 起 x54，y86）：已领黄、今天粉、未到灰。
    for (uint8_t i = 0; i < 7; i += 1) {
        int32_t x = 54 + (int32_t) i * 13;
        if ((uint16_t) i < day - 1U) {
            lv_obj_t *d = rect(s_modal_body, x, 86, 7, 7, 2, COL_COIN_BG);
            border(d, COL_INK, 1);
        } else if ((uint16_t) i == day - 1U) {
            lv_obj_t *d = rect(s_modal_body, x, 86, 7, 7, 2, COL_TAB_ON);
            border(d, COL_INK, 1);
        } else {
            rect(s_modal_body, x, 86, 7, 7, 2, COL_PIP_OFF);
        }
    }

    // 奖励白卡丸（h22 r11 发丝边）：5px 金币点 + 12px 棕字。
    const char *rtxt = week ? "100 G" : "20 G";
    int32_t tw = text_w12(rtxt);
    int32_t pw = week ? 84 : 72;
    int32_t px = (192 - pw) / 2;
    lv_obj_t *pill = rect(s_modal_body, px, 102, pw, 22, 11, COL_CARD_WHITE);
    border(pill, COL_HAIR, 1);
    int32_t gx = (pw - (tw + 8)) / 2;
    rect(pill, gx, 8, 5, 5, 2, COL_COIN_BG);
    llabel(pill, gx + 8, 4, tw + 1, &lv_font_montserrat_12, COL_BROWN, rtxt);

    if (week) {
        label(s_modal_body, 0, 128, 192, &lv_font_montserrat_12, COL_BLUE,
              "+1 shell ticket");
    }
}

static void checkin_open(void)
{
    s_mode = MODE_CHECKIN;
    modal_open();
    checkin_build();
}

static void handle_checkin_key(pet_ui_action_t act)
{
    if (act == PET_UI_ACT_CONFIRM) {
        bool week = (uint16_t) (s_esnap.streak + 1) >= 7;
        pet_app_econ_checkin();
        set_msg(week ? "100G +1 shell!" : "20G  thank you!", 1200);
        pet_audio_play(SND_HAPPY);
        modal_close();
    } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        pet_audio_play(SND_CANCEL);
        modal_close();
    }
}

// ---------------------------------------------------------------------------
// P2-S3a 婚介/家庭面板（designs 08 §1–§4）
// 两级：候选名单 → 某候选的动作（打招呼/送礼/两种戒指示婚）。
// 已婚/蛋就绪为只读状态页（同住倒计时；蛋在 S3b 接上世代交替）。
// ---------------------------------------------------------------------------

static pt_social_t s_ssnap;
static bool s_have_ssnap;
static int s_mate_level;     // 0=主视图（VISIT/已婚/蛋就绪，相位驱动），1=动作页，2=家谱
static int s_mate_sel;       // 当前焦点环索引
static int s_mate_ci;        // 动作页候选槽位
static int8_t s_mate_page;   // 家谱页码

static const char *mate_soc_msg(uint8_t rv)
{
    switch ((pt_soc_rv_t) rv) {
    case PT_SOC_CAPPED:   return "No visits today";
    case PT_SOC_COOLDOWN: return "Visit again later";
    case PT_SOC_POOR:     return "Not enough G";
    case PT_SOC_NOT_LOVE: return "Not in love yet";
    case PT_SOC_LOCKED:   return "Wait 2 days";
    case PT_SOC_REJECTED: return "Maybe later...";
    default:              return "Can't now";
    }
}

// 关系阶段着色（冻结稿 pv2-mate.html）：Met 灰 / Friend 蓝 / Crush 粉 / Love 绿。
static uint32_t mate_stage_color(uint8_t bond)
{
    switch (pt_social_rel_stage(bond)) {
    case PT_REL_FRIEND: return COL_BLUE;
    case PT_REL_CRUSH:  return COL_SEL;
    case PT_REL_LOVE:   return COL_GREEN;
    default:            return COL_HINT;
    }
}

// 名片基因 -> 小色牌（零位图）：调色板主色 + 2px 轮廓 + 两只小眼（深底白眼）。
static void mate_token(lv_obj_t *parent, int32_t x, int32_t y, int32_t sz,
                       const pt_genome_t *g)
{
    const pt_palette_t *pal = pt_genome_palette(pt_part_index(g->palette));
    uint32_t body = (pal != NULL) ? pal->main : (uint32_t) COL_PIP_OFF;
    uint32_t edge = (pal != NULL && pal->edge != 0)
                    ? pal->edge : (uint32_t) COL_INK;
    lv_obj_t *tok = rect(parent, x, y, sz, sz, sz >= 30 ? 10 : 8, body);
    border(tok, edge, 2);
    uint32_t r = (body >> 16) & 0xFFu;
    uint32_t gg = (body >> 8) & 0xFFu;
    uint32_t bb = body & 0xFFu;
    uint32_t ec = (r * 299 + gg * 587 + bb * 114 < 128000u)
                  ? 0xFFFFFFu : (uint32_t) COL_INK;
    int32_t ew = sz >= 30 ? 3 : 2;
    int32_t ex = sz >= 30 ? 9 : 8;
    int32_t ey = sz >= 30 ? 13 : 11;
    int32_t gap = sz >= 30 ? 8 : 7;
    rect(tok, ex, ey, ew, ew, 1, ec);
    rect(tok, ex + gap, ey, ew, ew, 1, ec);
}

// 收集有效候选槽位，返回数量。
static uint8_t mate_slots(uint8_t *slots)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        if (s_ssnap.cand[i].valid) {
            slots[n++] = i;
        }
    }
    return n;
}

// 候选槽位 -> 名单中的次序（动作页返回时恢复焦点）。
static int mate_ordinal_of(uint8_t ci)
{
    uint8_t slots[PT_SOC_CANDIDATES];
    uint8_t n = mate_slots(slots);
    for (uint8_t k = 0; k < n; k += 1) {
        if (slots[k] == ci) {
            return (int) k;
        }
    }
    return 0;
}

static uint8_t mate_tree_pages(void)
{
    uint8_t hc = pt_social_hall_count(&s_ssnap);
    uint8_t pages = (uint8_t) ((hc + 2) / 3);
    return pages == 0 ? 1 : pages;
}

static void mate_frame(const char *page)
{
    lv_obj_clean(s_modal_body);
    frame_pv2(false);
    show(s_mffooter);
    lv_label_set_text(s_modal_title, "FAMILY");
    chrome_coins(s_esnap.coins);
    if (page != NULL) {
        lv_label_set_text(s_mp_cur, page);
        lv_obj_set_pos(s_mp_cur, 91, 15);
        show(s_mp_cur);
    }
}

// 页脚三态：MF_DUAL=BACK/TREE（TREE 名人堂空时灰禁但占环位）；
// MF_BACK=仅 BACK；MF_PAGER=‹ BACK ›（循环翻页，箭头永不灰禁）。
enum { MF_DUAL = 0, MF_BACK = 1, MF_PAGER = 2 };

static void mate_pill_style(lv_obj_t *w, bool on, bool off)
{
    lv_obj_set_style_bg_color(w,
        lv_color_hex(off ? COL_DIS_BG : COL_DOCK_BG), 0);
    border(w, on ? COL_SEL : (off ? COL_DIS_LINE : COL_INK), on ? 2 : 1);
    lv_obj_t *txt = (lv_obj_t *) lv_obj_get_child(w, 0);
    lv_obj_set_style_text_color(txt,
        lv_color_hex(off ? COL_DIS_TX : COL_INK), 0);
}

// DUAL 态 sel：-1 无焦点 / 0=BACK / 1=TREE；MF_BACK：0；PAGER：0/1/2。
static void mate_footer_refresh(int mode, int sel, bool tree_off)
{
    hide(s_mf_back);
    hide(s_mf_tree);
    hide(s_mf_prev);
    hide(s_mf_mid);
    hide(s_mf_next);
    if (mode == MF_DUAL) {
        show(s_mf_back);
        show(s_mf_tree);
        mate_pill_style(s_mf_back, sel == 0, false);
        mate_pill_style(s_mf_tree, sel == 1, tree_off);
    } else if (mode == MF_BACK) {
        show(s_mf_back);
        mate_pill_style(s_mf_back, sel == 0, false);
    } else {
        show(s_mf_prev);
        show(s_mf_mid);
        show(s_mf_next);
        mate_pill_style(s_mf_prev, sel == 0, false);
        mate_pill_style(s_mf_mid, sel == 1, false);
        mate_pill_style(s_mf_next, sel == 2, false);
    }
}

// ---------------------------------------------------------------------------
// VISIT：今日访客卡（n<=3）-> BACK -> TREE 单环。
// ---------------------------------------------------------------------------

static void mate_build_visit(void)
{
    mate_frame(NULL);
    uint8_t slots[PT_SOC_CANDIDATES];
    uint8_t n = mate_slots(slots);
    bool tree_off = pt_social_hall_count(&s_ssnap) == 0;

    llabel(s_modal_body, 0, 0, 120, &lv_font_montserrat_12, COL_HINT,
           "Today's visitors");

    if (n == 0) {
        label(s_modal_body, 0, 50, 192, &lv_font_montserrat_12, COL_SUB,
              "Nobody yet");
        label(s_modal_body, 0, 68, 192, &lv_font_montserrat_12, COL_HINT,
              "Matchmaker visits mornings");
    } else {
        char buf[24];
        for (uint8_t k = 0; k < n; k += 1) {
            const pt_soc_cand_t *c = &s_ssnap.cand[slots[k]];
            int y = 16 + (int) k * 38;
            bool on = (s_mate_sel == (int) k);
            lv_obj_t *card = rect(s_modal_body, 0, y, 192, 34, 10,
                                  COL_CARD_WHITE);
            border(card, on ? COL_SEL : COL_HAIR, on ? 2 : 1);
            mate_token(card, 8, 5, 24, &c->genome);

            pt_social_name(&c->genome, buf, sizeof(buf));
            llabel(card, 42, 3, 90, &lv_font_montserrat_14, COL_INK, buf);

            snprintf(buf, sizeof(buf), "%u/%u",
                     (unsigned) c->interacts_today,
                     (unsigned) PT_CFG_SOC_INTERACT_DAY_CAP);
            rlabel12(card, 184, 4, COL_SUB, buf);

            const char *rn = pt_social_rel_name(pt_social_rel_stage(c->bond));
            llabel(card, 42, 18, 44, &lv_font_montserrat_12,
                   mate_stage_color(c->bond), rn);
            snprintf(buf, sizeof(buf), "%u", (unsigned) c->bond);
            llabel(card, 42 + text_w12(rn) + 4, 18, 24,
                   &lv_font_montserrat_12, COL_INK, buf);
        }
    }

    int ring = (int) n + 2;
    if (s_mate_sel < 0 || s_mate_sel >= ring) {
        s_mate_sel = 0;
    }
    int fsel = (s_mate_sel == (int) n) ? 0
             : (s_mate_sel == (int) n + 1) ? 1 : -1;
    mate_footer_refresh(MF_DUAL, fsel, tree_off);
}

// ---------------------------------------------------------------------------
// 动作页：身份卡 + 好感条 + Greet/Gift/Propose x2 -> BACK 单环（5）。
// ---------------------------------------------------------------------------

static void mate_build_action(void)
{
    const pt_soc_cand_t *c =
        (s_mate_ci < PT_SOC_CANDIDATES && s_ssnap.cand[s_mate_ci].valid)
        ? &s_ssnap.cand[s_mate_ci] : NULL;
    if (c == NULL) {
        s_mate_level = 0;
        s_mate_sel = 0;
        mate_build_visit();
        return;
    }

    mate_frame(NULL);
    char buf[24];

    lv_obj_t *idc = rect(s_modal_body, 0, 0, 192, 34, 10, COL_CARD_WHITE);
    border(idc, COL_HAIR, 1);
    mate_token(idc, 8, 5, 24, &c->genome);
    pt_social_name(&c->genome, buf, sizeof(buf));
    llabel(idc, 42, 3, 90, &lv_font_montserrat_14, COL_INK, buf);
    snprintf(buf, sizeof(buf), "%u/%u visits",
             (unsigned) c->interacts_today,
             (unsigned) PT_CFG_SOC_INTERACT_DAY_CAP);
    rlabel12(idc, 184, 4, COL_SUB, buf);
    const char *rn = pt_social_rel_name(pt_social_rel_stage(c->bond));
    llabel(idc, 42, 18, 44, &lv_font_montserrat_12,
           mate_stage_color(c->bond), rn);
    snprintf(buf, sizeof(buf), "%u", (unsigned) c->bond);
    llabel(idc, 42 + text_w12(rn) + 4, 18, 24,
           &lv_font_montserrat_12, COL_INK, buf);

    // 好感条（灰轨 + 绿填充 bond%）。
    rect(s_modal_body, 4, 40, 184, 6, 3, COL_PIP_OFF);
    int32_t fw = (int32_t) c->bond * 184 / 100;
    if (fw > 0) {
        rect(s_modal_body, 4, 40, fw, 6, 3, COL_GREEN);
    }

    static const char *const ACT[4] = {
        "Greet", "Gift", "Propose 500G", "Propose 5000G"
    };
    static const uint32_t ACTC[4] = {
        COL_INK, COL_INK, COL_BLUE, COL_BROWN
    };
    for (int i = 0; i < 4; i += 1) {
        int y = 52 + i * 24;
        bool on = (s_mate_sel == i);
        lv_obj_t *row = rect(s_modal_body, 0, y, 192, 22, 8, COL_CARD_WHITE);
        border(row, on ? COL_SEL : COL_HAIR, on ? 2 : 1);
        llabel(row, 12, 4, 120, &lv_font_montserrat_12, ACTC[i], ACT[i]);
        if (i == 1) {
            rlabel12(row, 180, 4, COL_BROWN, "80G");
        }
    }

    if (s_mate_sel < 0 || s_mate_sel > 4) {
        s_mate_sel = 0;
    }
    mate_footer_refresh(MF_BACK, s_mate_sel == 4 ? 0 : -1, false);
}

// ---------------------------------------------------------------------------
// 已婚等蛋 / 蛋就绪：双宠 token + 状态卡；蛋就绪追加 Welcome 大按钮。
// ---------------------------------------------------------------------------

static void mate_build_married(void)
{
    bool egg = s_ssnap.phase == PT_SOC_EGG_READY;
    mate_frame(NULL);
    bool tree_off = pt_social_hall_count(&s_ssnap) == 0;

    // 双宠色牌水平交叠 4px：玩家 + 配偶。
    int32_t ty = egg ? 8 : 16;
    mate_token(s_modal_body, 62, ty, 36, &s_snap.genome);
    mate_token(s_modal_body, 94, ty, 36, &s_ssnap.spouse);

    label(s_modal_body, 0, egg ? 42 : 56, 192,
          &lv_font_montserrat_20, COL_INK, "Married");
    char nm[8];
    pt_social_name(&s_ssnap.spouse, nm, sizeof(nm));
    char buf[24];
    snprintf(buf, sizeof(buf), "to %s", nm);
    label(s_modal_body, 0, egg ? 70 : 86, 192,
          &lv_font_montserrat_12, COL_SUB, buf);

    int32_t cy = egg ? 92 : 108;
    lv_obj_t *sc = rect(s_modal_body, 0, cy, 192, egg ? 28 : 30, 10,
                        COL_CARD_WHITE);
    border(sc, COL_HAIR, 1);
    const char *stxt;
    uint32_t scol;
    if (egg) {
        stxt = "An egg is on the way!";
        scol = COL_RED;
    } else {
        int32_t left = pt_social_egg_minutes_left(&s_ssnap, s_snap.minute);
        if (left > 0) {
            snprintf(buf, sizeof(buf), "Egg in %ldh %ldm",
                     (long) (left / 60), (long) (left % 60));
            stxt = buf;
        } else {
            stxt = "Egg soon";
        }
        scol = COL_GREEN;
    }
    rect(sc, 14, egg ? 10 : 11, 8, 8, 2, scol);
    llabel(sc, 30, egg ? 7 : 8, 156, &lv_font_montserrat_12, scol, stxt);

    int ring;
    if (egg) {
        bool on = (s_mate_sel == 0);
        lv_obj_t *b = rect(s_modal_body, 16, 126, 160, 26, 12, COL_GREEN);
        border(b, on ? COL_SEL : COL_GREEN, on ? 2 : 0);
        label(b, 0, 5, 160, &lv_font_montserrat_12, COL_CARD_WHITE,
              "Welcome the egg!");
        ring = 3;
    } else {
        ring = 2;
    }
    if (s_mate_sel < 0 || s_mate_sel >= ring) {
        s_mate_sel = 0;
    }
    // DUAL 页脚焦点位：BACK / TREE 在环内的绝对序号。
    int back_idx = egg ? 1 : 0;
    int tree_idx = egg ? 2 : 1;
    int fsel = (s_mate_sel == back_idx) ? 0
             : (s_mate_sel == tree_idx) ? 1 : -1;
    mate_footer_refresh(MF_DUAL, fsel, tree_off);
}

// ---------------------------------------------------------------------------
// 家谱：每屏最多 3 张世代卡（最新一代在前），‹ BACK › 循环翻页。
// ---------------------------------------------------------------------------

static void mate_build_tree(void)
{
    uint8_t hc = pt_social_hall_count(&s_ssnap);
    uint8_t pages = mate_tree_pages();
    if (s_mate_page < 0) {
        s_mate_page = 0;
    }
    if (s_mate_page >= (int) pages) {
        s_mate_page = (int8_t) (pages - 1);
    }

    char pg[8];
    snprintf(pg, sizeof(pg), "%u/%u", (unsigned) s_mate_page + 1,
             (unsigned) pages);
    mate_frame(pg);

    static const char *const CAREW[4] = {
        "Perfect", "Great", "Normal", "Neglect"
    };

    for (uint8_t k = 0; k < 3; k += 1) {
        int idx = (int) hc - 1 - (s_mate_page * 3 + k);
        if (idx < 0) {
            break;
        }
        const pt_soc_hall_t *h = pt_social_hall_entry(&s_ssnap,
                                                      (uint8_t) idx);
        int y = (int) k * 46;
        lv_obj_t *card = rect(s_modal_body, 0, y, 192, 42, 10,
                              COL_CARD_WHITE);
        border(card, COL_HAIR, 1);

        char line[40];
        char pn[8];
        pt_social_name(&h->pet, pn, sizeof(pn));
        snprintf(line, sizeof(line), "G%lu %s",
                 (unsigned long) h->generation, pn);
        llabel(card, 8, 4, 100, &lv_font_montserrat_14, COL_INK, line);

        lv_obj_t *sp = llabel(card, 84, 4, 100, &lv_font_montserrat_14,
                              COL_BLUE, species_name((pt_species_t)
                                                     h->species));
        lv_obj_set_style_text_align(sp, LV_TEXT_ALIGN_RIGHT, 0);

        bool mated = false;
        const uint8_t *raw = (const uint8_t *) &h->mate;
        for (uint8_t b = 0; b < 8; b += 1) {
            if (raw[b] != 0) {
                mated = true;
                break;
            }
        }
        const char *cw = (h->care < 4) ? CAREW[h->care] : "Normal";
        if (mated) {
            char mn[8];
            pt_social_name(&h->mate, mn, sizeof(mn));
            snprintf(line, sizeof(line), "Care %s  +%s", cw, mn);
        } else {
            snprintf(line, sizeof(line), "Care %s", cw);
        }
        llabel(card, 8, 21, 176, &lv_font_montserrat_12, COL_SUB, line);
    }

    if (s_mate_sel < 0 || s_mate_sel > 2) {
        s_mate_sel = 0;
    }
    mate_footer_refresh(MF_PAGER, s_mate_sel, false);
}

static void mate_build(void)
{
    // 家谱只读页优先。
    if (s_mate_level == 2) {
        mate_build_tree();
        return;
    }
    // 已婚/蛋就绪为相位驱动视图；求婚成功后动作页重建自然落到这里。
    if (s_ssnap.phase == PT_SOC_MARRIED
        || s_ssnap.phase == PT_SOC_EGG_READY) {
        s_mate_level = 0;
        mate_build_married();
        return;
    }
    if (s_mate_level == 1) {
        mate_build_action();
        return;
    }
    mate_build_visit();
}

static void mate_open_tree(void)
{
    s_mate_level = 2;
    s_mate_page = 0;
    s_mate_sel = 0;
    mate_build();
}

static void mate_open(void)
{
    if (!pet_app_social_snapshot(&s_ssnap)) {
        set_msg("Busy", 800);
        return;
    }
    s_have_ssnap = true;
    s_mate_level = 0;
    s_mate_sel = 0;
    s_mate_ci = 0;
    s_mate_page = 0;
    s_mode = MODE_MATE;
    modal_open();
    mate_build();
    pet_audio_play(SND_CONFIRM);
}

static void handle_mate_key(pet_ui_action_t act)
{
    // ---- 家谱：‹ BACK › 环，OK 翻页/返回 ----
    if (s_mate_level == 2) {
        if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
            s_mate_level = 0;
            s_mate_sel = 0;
            mate_build();
            pet_audio_play(SND_CANCEL);
            return;
        }
        if (act == PET_UI_ACT_PREV || act == PET_UI_ACT_NEXT) {
            s_mate_sel += (act == PET_UI_ACT_NEXT) ? 1 : -1;
            if (s_mate_sel < 0) { s_mate_sel = 2; }
            if (s_mate_sel > 2) { s_mate_sel = 0; }
            mate_build();
            pet_audio_play(SND_CONFIRM);
            return;
        }
        if (act == PET_UI_ACT_CONFIRM) {
            uint8_t pages = mate_tree_pages();
            if (s_mate_sel == 0) {
                s_mate_page = (int8_t) ((s_mate_page + pages - 1) % pages);
                mate_build();
                pet_audio_play(SND_CONFIRM);
            } else if (s_mate_sel == 2) {
                s_mate_page = (int8_t) ((s_mate_page + 1) % pages);
                mate_build();
                pet_audio_play(SND_CONFIRM);
            } else {
                s_mate_level = 0;
                s_mate_sel = 0;
                mate_build();
                pet_audio_play(SND_CANCEL);
            }
        }
        return;
    }

    // ---- 已婚等蛋 / 蛋就绪 ----
    if (s_ssnap.phase == PT_SOC_MARRIED
        || s_ssnap.phase == PT_SOC_EGG_READY) {
        bool egg = s_ssnap.phase == PT_SOC_EGG_READY;
        int ring = egg ? 3 : 2;
        if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
            modal_close();
            pet_audio_play(SND_CANCEL);
        } else if (act == PET_UI_ACT_PREV || act == PET_UI_ACT_NEXT) {
            s_mate_sel += (act == PET_UI_ACT_NEXT) ? 1 : -1;
            if (s_mate_sel < 0) { s_mate_sel = ring - 1; }
            if (s_mate_sel >= ring) { s_mate_sel = 0; }
            mate_build();
            pet_audio_play(SND_CONFIRM);
        } else if (act == PET_UI_ACT_CONFIRM) {
            if (egg && s_mate_sel == 0) {
                // 世代交替：引擎任务执行，刷新循环见到相位变化即关面板。
                pet_app_soc_start_egg();
                pet_audio_play(SND_EVOLVE);
            } else if (s_mate_sel == ring - 1) {
                // 环末位恒为 TREE；名人堂空时灰禁，OK 吞掉不动。
                if (pt_social_hall_count(&s_ssnap) > 0) {
                    mate_open_tree();
                    pet_audio_play(SND_CONFIRM);
                }
            } else {
                // 其余焦点位 = BACK。
                modal_close();
                pet_audio_play(SND_CANCEL);
            }
        }
        return;
    }

    // ---- 返回语义（长按 OK / 双击 OK）----
    if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        if (s_mate_level == 1) {
            s_mate_level = 0;
            s_mate_sel = mate_ordinal_of((uint8_t) s_mate_ci);
            mate_build();
            pet_audio_play(SND_CANCEL);
        } else {
            modal_close();
            pet_audio_play(SND_CANCEL);
        }
        return;
    }

    // ---- 动作页：4 行 + BACK ----
    if (s_mate_level == 1) {
        if (act == PET_UI_ACT_PREV || act == PET_UI_ACT_NEXT) {
            s_mate_sel += (act == PET_UI_ACT_NEXT) ? 1 : -1;
            if (s_mate_sel < 0) { s_mate_sel = 4; }
            if (s_mate_sel > 4) { s_mate_sel = 0; }
            mate_build();
            pet_audio_play(SND_CONFIRM);
        } else if (act == PET_UI_ACT_CONFIRM) {
            switch (s_mate_sel) {
            case 0:
                pet_app_soc_greet((uint8_t) s_mate_ci);
                break;
            case 1:
                pet_app_soc_gift((uint8_t) s_mate_ci);
                break;
            case 2:
                pet_app_soc_propose((uint8_t) s_mate_ci, 0);
                break;
            case 3:
                pet_app_soc_propose((uint8_t) s_mate_ci, 1);
                break;
            default:
                s_mate_level = 0;
                s_mate_sel = mate_ordinal_of((uint8_t) s_mate_ci);
                mate_build();
                pet_audio_play(SND_CANCEL);
                return;
            }
        }
        return;
    }

    // ---- VISIT：n 张候选卡 -> BACK -> TREE ----
    uint8_t n = pt_social_candidate_count(&s_ssnap);
    bool tree_off = pt_social_hall_count(&s_ssnap) == 0;
    int ring = (int) n + 2;
    if (act == PET_UI_ACT_PREV || act == PET_UI_ACT_NEXT) {
        s_mate_sel += (act == PET_UI_ACT_NEXT) ? 1 : -1;
        if (s_mate_sel < 0) { s_mate_sel = ring - 1; }
        if (s_mate_sel >= ring) { s_mate_sel = 0; }
        mate_build();
        pet_audio_play(SND_CONFIRM);
    } else if (act == PET_UI_ACT_CONFIRM) {
        if (s_mate_sel < (int) n) {
            // 第 k 个有效候选 -> 槽位号。
            uint8_t slots[PT_SOC_CANDIDATES];
            (void) mate_slots(slots);
            s_mate_ci = (int) slots[s_mate_sel];
            s_mate_level = 1;
            s_mate_sel = 0;
            mate_build();
            pet_audio_play(SND_CONFIRM);
        } else if (s_mate_sel == (int) n) {
            modal_close();
            pet_audio_play(SND_CANCEL);
        } else if (!tree_off) {
            mate_open_tree();
            pet_audio_play(SND_CONFIRM);
        }
        // TREE 灰禁：OK 吞掉不动。
    }
}

// ---------------------------------------------------------------------------
// P2-S4 图鉴面板（PV2 batch4，定稿 pv2-dex.html）
// 统一页脚 ‹ ALBUM PARTS BADGES ›：ALBUM 物种卡 3 页 / PARTS 64 件浏览器
// （8 格与页脚连成单环，‹ › = 全局游标 ±8）/ BADGES 2 页。BACK 关闭。
// ---------------------------------------------------------------------------

static pt_dex_t s_dex_snap;
static int s_dex_tab;        // 0=ALBUM 1=PARTS 2=BADGES
static int s_dex_page;       // ALBUM 0..2 / BADGES 0..1
static int s_dex_cursor;     // PARTS 全局面位索引 0..63
static int s_dex_sel;        // 当前页焦点环索引（-1=由 builder 归位到当前档）

enum { DEX_TAB_SPECIES = 0, DEX_TAB_PARTS, DEX_TAB_BADGES };

// PARTS 当前页部件格（最多 8）。
static lv_obj_t *s_dex_cell[8];

static const char *dex_slot_name(pt_gene_slot_t s)
{
    switch (s) {
    case PT_GENE_SLOT_BODY:    return "Body";
    case PT_GENE_SLOT_EYES:    return "Eyes";
    case PT_GENE_SLOT_FACE:    return "Face";
    case PT_GENE_SLOT_HEAD:    return "Head";
    case PT_GENE_SLOT_PALETTE: return "Color";
    case PT_GENE_SLOT_BACK:    return "Back";
    default:                   return "?";
    }
}

static const char *dex_rarity_letter(pt_rarity_t r)
{
    switch (r) {
    case PT_RAR_U: return "U";
    case PT_RAR_R: return "R";
    case PT_RAR_L: return "L";
    default:       return "C";
    }
}

static uint32_t dex_rarity_color(pt_rarity_t r)
{
    switch (r) {
    case PT_RAR_U: return COL_BLUE;
    case PT_RAR_R: return 0xB06FC9;
    case PT_RAR_L: return COL_YELLOW;
    default:       return COL_INK;
    }
}

// 锁定提示（Montserrat 12，卡内可用宽约 140px：文案 <= 20 字符）。
static const char *dex_locked_hint(pt_species_t sp)
{
    switch (sp) {
    case PT_SP_TEEN_A:        return "Good-care teen";
    case PT_SP_TEEN_B:        return "Easygoing teen";
    case PT_SP_TEEN_C:        return "Weak-care teen";
    case PT_SP_ADULT_PERFECT: return "Perfect-care adult";
    case PT_SP_ADULT_GREAT:   return "Great-care adult";
    case PT_SP_ADULT_NORMAL:  return "Ordinary adult";
    case PT_SP_ADULT_NEGLECT: return "Neglected adult";
    case PT_SP_ADULT_MOON:    return "???";
    default:                  return "";
    }
}

// 全局位索引 -> 槽 / 槽内序号。
static void dex_locate(int abs_idx, pt_gene_slot_t *slot, uint8_t *index)
{
    for (uint8_t s = 0; s < PT_GENE_SLOT_COUNT; s += 1) {
        uint8_t begin = pt_dex_slot_offset((pt_gene_slot_t) s);
        uint8_t end = (uint8_t) (begin
                                 + pt_slot_part_count((pt_gene_slot_t) s));
        if ((uint8_t) abs_idx >= begin && (uint8_t) abs_idx < end) {
            *slot = (pt_gene_slot_t) s;
            *index = (uint8_t) (abs_idx - begin);
            return;
        }
    }
    *slot = PT_GENE_SLOT_BODY;
    *index = 0;
}

// PARTS 当前游标所在页的格数 / 首件全局索引。
static uint8_t dex_parts_shown(void)
{
    pt_gene_slot_t slot;
    uint8_t idx;
    dex_locate((uint8_t) s_dex_cursor, &slot, &idx);
    uint8_t count = pt_slot_part_count(slot);
    uint8_t sh = (uint8_t) (count - (idx / 8) * 8);
    return sh > 8 ? 8 : sh;
}

static uint8_t dex_parts_base(void)
{
    pt_gene_slot_t slot;
    uint8_t idx;
    dex_locate((uint8_t) s_dex_cursor, &slot, &idx);
    return (uint8_t) (pt_dex_slot_offset(slot) + (idx / 8) * 8);
}

// DEX 页脚 5 控件：‹ ALBUM PARTS BADGES ›。focus 为局部序号 0..4，-1=焦点在格内。
static void dex_footer_refresh(int focus, bool prev_off, bool next_off)
{
    lv_obj_t *w[5] = {
        s_df_prev, s_df_tab[0], s_df_tab[1], s_df_tab[2], s_df_next
    };
    bool dis[5] = { prev_off, false, false, false, next_off };
    for (int i = 0; i < 5; i += 1) {
        bool on = (i == focus);
        bool is_tab = (i >= 1 && i <= 3);
        bool active = is_tab && ((i - 1) == s_dex_tab);
        uint32_t fill = is_tab ? (active ? COL_TAB_ON : COL_CARD_WHITE)
                               : (dis[i] ? COL_DIS_BG : COL_DOCK_BG);
        lv_obj_set_style_bg_color(w[i], lv_color_hex(fill), 0);
        if (on) {
            border(w[i], COL_SEL, 2);
        } else if (active) {
            border(w[i], COL_SEL, 1);
        } else {
            border(w[i], dis[i] ? COL_DIS_LINE : COL_INK, 1);
        }
        lv_obj_t *txt = (lv_obj_t *) lv_obj_get_child(w[i], 0);
        lv_obj_set_style_text_color(
            txt, lv_color_hex(dis[i] ? COL_DIS_TX : COL_INK), 0);
    }
}

static void dex_frame(const char *title, const char *page)
{
    lv_obj_clean(s_modal_body);
    memset(s_dex_cell, 0, sizeof(s_dex_cell));
    frame_pv2(false);
    show(s_dfooter);
    lv_label_set_text(s_modal_title, title);
    lv_label_set_text(s_mp_cur, page);
    lv_obj_set_pos(s_mp_cur, 91, 15);
    show(s_mp_cur);
}

// ---------------------------------------------------------------------------
// ALBUM：物种卡 3 页 x3
// ---------------------------------------------------------------------------

static void dex_build_species(void)
{
    const uint8_t pages = 3;
    if (s_dex_page < 0) { s_dex_page = 0; }
    if (s_dex_page >= pages) { s_dex_page = pages - 1; }

    char pg[8];
    snprintf(pg, sizeof(pg), "%u/3", (unsigned) (s_dex_page + 1));
    dex_frame("ALBUM", pg);

    static const char *const CAREW[4] = {
        "Perfect", "Great", "Normal", "Neglect"
    };
    static const uint32_t CAREC[4] = {
        COL_GREEN, COL_BLUE, COL_YELLOW, COL_RED
    };
    static const uint32_t PIPC[3] = { COL_BLUE, COL_GREEN, COL_YELLOW };

    for (uint8_t k = 0; k < 3; k += 1) {
        uint8_t ri = (uint8_t) (s_dex_page * 3 + k);
        if (ri >= PT_DEX_SPECIES) {
            break;
        }
        int y = (int) k * 46;
        pt_species_t sp = pt_dex_roster(ri);
        uint8_t lv = pt_dex_species_level(&s_dex_snap, sp);
        lv_obj_t *box = rect(s_modal_body, 0, y, 192, 42, 10, COL_CARD_WHITE);
        border(box, COL_INK, 1);

        // 物种色牌（未发现：灰底问号）。
        lv_obj_t *tok = rect(box, 8, 7, 28, 28, 8, COL_PIP_OFF);
        if (lv > 0) {
            const profile_t *p = profile_for(ri < 3 ? PT_STAGE_TEEN
                                                    : PT_STAGE_ADULT, sp);
            lv_obj_set_style_bg_color(tok, lv_color_hex(p->body), 0);
            border(tok, p->edge, 2);
            uint32_t ec = p->dark_eyes ? 0xFFFFFFu : COL_INK;
            rect(tok, 9, 13, 3, 3, 1, ec);
            rect(tok, 17, 13, 3, 3, 1, ec);
        } else {
            label(tok, 0, 6, 28, &lv_font_montserrat_14, COL_DIS_TX, "?");
        }

        char line[24];
        snprintf(line, sizeof(line), "#%u %s", (unsigned) ri + 1,
                 lv > 0 ? species_name(sp) : "???");
        llabel(box, 44, 4, 110, &lv_font_montserrat_14, COL_INK, line);

        for (uint8_t pip = 0; pip < 3; pip += 1) {
            bool lit = lv > pip;
            lv_obj_t *p = rect(box, 158 + (int) pip * 9, 8, 7, 7, 2,
                               lit ? PIPC[pip] : COL_PIP_OFF);
            if (lit) {
                border(p, COL_INK, 1);
            }
        }

        if (lv == 0) {
            llabel(box, 44, 23, 140, &lv_font_montserrat_12, COL_SUB,
                   dex_locked_hint(sp));
        } else {
            char pre[24];
            snprintf(pre, sizeof(pre), "x%u raised - ",
                     (unsigned) pt_dex_species_raised_count(&s_dex_snap, sp));
            llabel(box, 44, 23, 100, &lv_font_montserrat_12, COL_SUB, pre);
            uint8_t care = pt_dex_species_best_care(&s_dex_snap, sp);
            if (care < 4) {
                llabel(box, 44 + text_w12(pre), 23, 60,
                       &lv_font_montserrat_12, CAREC[care], CAREW[care]);
            } else {
                llabel(box, 44 + text_w12(pre), 23, 20,
                       &lv_font_montserrat_12, COL_SUB, "-");
            }
        }
    }

    dex_footer_refresh(s_dex_sel, s_dex_page == 0, s_dex_page + 1 >= pages);
}

// ---------------------------------------------------------------------------
// PARTS：4x2 部件格 + 详情卡，游标为 64 件全局索引
// ---------------------------------------------------------------------------

static void dex_build_parts(void)
{
    uint8_t total = pt_dex_part_total();
    if (s_dex_cursor < 0) { s_dex_cursor = 0; }
    if (s_dex_cursor >= total) { s_dex_cursor = total - 1; }

    pt_gene_slot_t slot;
    uint8_t idx;
    dex_locate((uint8_t) s_dex_cursor, &slot, &idx);
    uint8_t count = pt_slot_part_count(slot);
    uint8_t page = (uint8_t) (idx / 8);
    uint8_t shown = (uint8_t) (count - page * 8);
    if (shown > 8) { shown = 8; }

    char pg[8];
    snprintf(pg, sizeof(pg), "%u/%u", (unsigned) (s_dex_cursor + 1),
             (unsigned) total);
    dex_frame("PARTS", pg);

    // 槽名 + 已见计数。
    llabel(s_modal_body, 0, 0, 80, &lv_font_montserrat_14, COL_INK,
           dex_slot_name(slot));
    uint8_t seen_n = 0;
    for (uint8_t i = 0; i < count; i += 1) {
        if (pt_dex_part_has(&s_dex_snap, PT_DEX_PART_SEEN, slot, i)) {
            seen_n += 1;
        }
    }
    char seen[16];
    snprintf(seen, sizeof(seen), "seen %u/%u", (unsigned) seen_n,
             (unsigned) count);
    rlabel12(s_modal_body, 192, 1, COL_SUB, seen);

    // 4x2 格网。
    for (uint8_t k = 0; k < shown; k += 1) {
        uint8_t si = (uint8_t) (page * 8 + k);
        bool seen = pt_dex_part_has(&s_dex_snap, PT_DEX_PART_SEEN, slot, si);
        bool owned = pt_dex_part_has(&s_dex_snap, PT_DEX_PART_OWNED, slot, si);
        bool bred = pt_dex_part_has(&s_dex_snap, PT_DEX_PART_BRED, slot, si);
        pt_rarity_t rar = pt_catalog_rarity(slot, si);
        int x = (int) (k % 4) * 48;
        int y = 20 + (int) (k / 4) * 40;

        // 焦点粉环先画，格矩形盖住内缘，只露外 2px。
        if (s_dex_sel == k) {
            lv_obj_t *ring = rect(s_modal_body, x - 2, y - 2, 48, 40, 10,
                                  COL_CARD_PINK);
            lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
            border(ring, COL_SEL, 2);
        }

        lv_obj_t *cell = rect(s_modal_body, x, y, 44, 36, 8,
                              seen ? COL_CARD_WHITE : COL_PIP_OFF);
        border(cell, seen ? dex_rarity_color(rar) : COL_DOCK_BG, 1);
        s_dex_cell[k] = cell;

        char num[4];
        snprintf(num, sizeof(num), "%u", (unsigned) (si + 1));
        if (seen) {
            uint32_t rc = dex_rarity_color(rar);
            label(cell, 0, 8, 44, &lv_font_montserrat_14, rc, num);
            llabel(cell, 5, 21, 12, &lv_font_montserrat_12, rc,
                   dex_rarity_letter(rar));
            if (owned) {
                rect(cell, 33, 5, 7, 7, 2, COL_GREEN);
            }
            if (bred) {
                rect(cell, 33, 15, 7, 7, 2, COL_SEL);
            }
        } else {
            label(cell, 0, 9, 44, &lv_font_montserrat_14, COL_DIS_TX, "?");
        }
    }

    // 游标所在件详情卡。
    bool cseen = pt_dex_part_has(&s_dex_snap, PT_DEX_PART_SEEN, slot, idx);
    bool cowned = pt_dex_part_has(&s_dex_snap, PT_DEX_PART_OWNED, slot, idx);
    bool cbred = pt_dex_part_has(&s_dex_snap, PT_DEX_PART_BRED, slot, idx);
    lv_obj_t *dc = rect(s_modal_body, 0, 100, 192, 44, 10, COL_CARD_WHITE);
    border(dc, COL_INK, 1);
    if (!cseen) {
        char h[24];
        snprintf(h, sizeof(h), "#%u not seen yet", (unsigned) (idx + 1));
        llabel(dc, 10, 25, 170, &lv_font_montserrat_12, COL_SUB, h);
    } else {
        static const char *const RN[4] = {
            "Common", "Unusual", "Rare", "Legend"
        };
        pt_rarity_t rar = pt_catalog_rarity(slot, idx);
        char head[8];
        snprintf(head, sizeof(head), "#%u ", (unsigned) (idx + 1));
        llabel(dc, 10, 4, 40, &lv_font_montserrat_14, COL_INK, head);
        llabel(dc, 10 + text_wf(&lv_font_montserrat_14, head), 4, 80,
               &lv_font_montserrat_14, dex_rarity_color(rar), RN[rar]);
        uint32_t mc;
        const char *mt;
        if (cowned) {
            mc = COL_GREEN;
            mt = "Your pet has it";
        } else if (cbred) {
            mc = COL_SEL;
            mt = "Bred into family";
        } else {
            mc = COL_SUB;
            mt = "Seen on a visitor";
        }
        rect(dc, 10, 27, 8, 8, 2, mc);
        llabel(dc, 23, 25, 160, &lv_font_montserrat_12, COL_INK, mt);
    }

    int n = (int) shown + 5;
    if (s_dex_sel < 0 || s_dex_sel >= n) {
        s_dex_sel = (int) shown + 2;   // 默认落在当前档 PARTS
    }
    int ffocus = (s_dex_sel >= (int) shown) ? (s_dex_sel - (int) shown) : -1;
    dex_footer_refresh(ffocus, s_dex_cursor < 8,
                       s_dex_cursor + 8 >= (int) total);
}

// ---------------------------------------------------------------------------
// BADGES：总览 + 寿龄 + 6 槽纯血，2 页（6 + 2）
// ---------------------------------------------------------------------------

static void dex_build_badges(void)
{
    const uint8_t pages = 2;
    if (s_dex_page < 0) { s_dex_page = 0; }
    if (s_dex_page >= pages) { s_dex_page = pages - 1; }

    char pg[8];
    snprintf(pg, sizeof(pg), "%u/2", (unsigned) (s_dex_page + 1));
    dex_frame("BADGES", pg);

    uint8_t hc = s_have_ssnap ? pt_social_hall_count(&s_ssnap) : 0;
    uint8_t items = (s_dex_page == 0) ? 6 : 2;
    for (uint8_t k = 0; k < items; k += 1) {
        int y = (int) k * 24;
        lv_obj_t *r = rect(s_modal_body, 0, y, 192, 22, 8, COL_CARD_WHITE);
        border(r, COL_INK, 1);

        // 第 1 页：k0=Album、k1=Oldest、k2..k5 -> 槽 0..3；第 2 页：k0..k1 -> 槽 4..5。
        if (s_dex_page == 0 && k == 0) {
            char line[32];
            snprintf(line, sizeof(line), "Album %u/8  Parts %u/%u",
                     (int) pt_dex_species_seen_count(&s_dex_snap),
                     (int) pt_dex_parts_count(&s_dex_snap, PT_DEX_PART_SEEN),
                     (int) pt_dex_part_total());
            rect(s_modal_body, 9, y + 5, 12, 12, 6, COL_BLUE);
            llabel(s_modal_body, 28, y + 4, 156, &lv_font_montserrat_12,
                   COL_INK, line);
            continue;
        }
        if (s_dex_page == 0 && k == 1) {
            char days[12];
            snprintf(days, sizeof(days), "%u days",
                     (unsigned) s_dex_snap.oldest_days);
            rect(s_modal_body, 9, y + 5, 12, 12, 6, COL_YELLOW);
            llabel(s_modal_body, 28, y + 4, 80, &lv_font_montserrat_12,
                   COL_INK, "Oldest");
            rlabel12(s_modal_body, 184, y + 4, COL_INK, days);
            continue;
        }

        // 纯链条徽章：槽位映射见循环上方注释。
        uint8_t slot_ix = (s_dex_page == 0)
                          ? (uint8_t) (k - 2)
                          : (uint8_t) (k + 4);
        pt_gene_slot_t slot = (pt_gene_slot_t) slot_ix;
        uint8_t chain = pt_dex_pure_chain(hc > 0 ? s_ssnap.hall : NULL, hc,
                                          slot);
        char nm[16];
        snprintf(nm, sizeof(nm), "Pure %s", dex_slot_name(slot));
        rect(s_modal_body, 9, y + 5, 12, 12, 6,
             chain >= 2 ? COL_YELLOW : COL_PIP_OFF);
        llabel(s_modal_body, 28, y + 4, 100, &lv_font_montserrat_12,
               COL_INK, nm);
        if (chain >= 2) {
            char xv[8];
            snprintf(xv, sizeof(xv), "x%u", (unsigned) chain);
            rlabel12(s_modal_body, 184, y + 4, COL_YELLOW, xv);
        } else {
            rlabel12(s_modal_body, 184, y + 4, COL_DIS_TX, "--");
        }
    }

    dex_footer_refresh(s_dex_sel, s_dex_page == 0, s_dex_page + 1 >= pages);
}

static void dex_build(void)
{
    if (s_dex_tab == DEX_TAB_SPECIES) {
        dex_build_species();
    } else if (s_dex_tab == DEX_TAB_PARTS) {
        dex_build_parts();
    } else {
        dex_build_badges();
    }
}

static void dex_open(void)
{
    if (!pet_app_dex_snapshot(&s_dex_snap)) {
        set_msg("Busy", 800);
        return;
    }
    // 徽章页纯血链读名人堂；失败也允许打开（显示空链）。
    (void) pet_app_social_snapshot(&s_ssnap);
    s_have_ssnap = true;
    s_dex_tab = DEX_TAB_SPECIES;
    s_dex_page = 0;
    s_dex_cursor = 0;
    s_dex_sel = 1;
    s_mode = MODE_DEX;
    modal_open();
    dex_build();
    pet_audio_play(SND_CONFIRM);
}

static void dex_goto_tab(int t)
{
    s_dex_tab = t;
    s_dex_page = 0;
    s_dex_cursor = 0;
    s_dex_sel = (t == DEX_TAB_SPECIES) ? 1
              : (t == DEX_TAB_BADGES) ? 3
              : -1;   // PARTS 由 builder 归位到当前档
    dex_build();
    pet_audio_play(SND_CONFIRM);
}

static void handle_dex_key(pet_ui_action_t act)
{
    if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        modal_close();
        pet_audio_play(SND_CANCEL);
        return;
    }

    if (act == PET_UI_ACT_PREV || act == PET_UI_ACT_NEXT) {
        int dir = (act == PET_UI_ACT_NEXT) ? 1 : -1;
        int n = (s_dex_tab == DEX_TAB_PARTS) ? (int) dex_parts_shown() + 5 : 5;
        s_dex_sel += dir;
        if (s_dex_sel < 0) { s_dex_sel += n; }
        if (s_dex_sel >= n) { s_dex_sel -= n; }
        if (s_dex_tab == DEX_TAB_PARTS) {
            uint8_t shown = dex_parts_shown();
            if (s_dex_sel < (int) shown) {
                s_dex_cursor = (int) dex_parts_base() + s_dex_sel;
            }
        }
        dex_build();
        pet_audio_play(SND_CONFIRM);
        return;
    }

    if (act != PET_UI_ACT_CONFIRM) {
        return;
    }

    if (s_dex_tab == DEX_TAB_SPECIES) {
        if (s_dex_sel == 0) {
            if (s_dex_page > 0) {
                s_dex_page -= 1;
                s_dex_sel = 1;
                dex_build();
                pet_audio_play(SND_CONFIRM);
            }
        } else if (s_dex_sel == 2) {
            dex_goto_tab(DEX_TAB_PARTS);
        } else if (s_dex_sel == 3) {
            dex_goto_tab(DEX_TAB_BADGES);
        } else if (s_dex_sel == 4) {
            if (s_dex_page + 1 < 3) {
                s_dex_page += 1;
                s_dex_sel = 1;
                dex_build();
                pet_audio_play(SND_CONFIRM);
            }
        }
        return;
    }

    if (s_dex_tab == DEX_TAB_BADGES) {
        if (s_dex_sel == 0) {
            if (s_dex_page > 0) {
                s_dex_page -= 1;
                s_dex_sel = 3;
                dex_build();
                pet_audio_play(SND_CONFIRM);
            }
        } else if (s_dex_sel == 1) {
            dex_goto_tab(DEX_TAB_SPECIES);
        } else if (s_dex_sel == 2) {
            dex_goto_tab(DEX_TAB_PARTS);
        } else if (s_dex_sel == 4) {
            if (s_dex_page + 1 < 2) {
                s_dex_page += 1;
                s_dex_sel = 3;
                dex_build();
                pet_audio_play(SND_CONFIRM);
            }
        }
        return;
    }

    // PARTS：格本身无 OK 动作；页脚局部序号 0=‹ 1=ALBUM 2=PARTS 3=BADGES 4=›。
    uint8_t shown = dex_parts_shown();
    if (s_dex_sel < (int) shown) {
        return;
    }
    int local = s_dex_sel - (int) shown;
    if (local == 0) {
        if (s_dex_cursor >= 8) {
            s_dex_cursor -= 8;
            pt_gene_slot_t slot;
            uint8_t idx;
            dex_locate((uint8_t) s_dex_cursor, &slot, &idx);
            s_dex_sel = (int) (idx % 8);
            dex_build();
            pet_audio_play(SND_CONFIRM);
        }
    } else if (local == 1) {
        dex_goto_tab(DEX_TAB_SPECIES);
    } else if (local == 3) {
        dex_goto_tab(DEX_TAB_BADGES);
    } else if (local == 4) {
        if (s_dex_cursor + 8 < (int) pt_dex_part_total()) {
            s_dex_cursor += 8;
            pt_gene_slot_t slot;
            uint8_t idx;
            dex_locate((uint8_t) s_dex_cursor, &slot, &idx);
            s_dex_sel = (int) (idx % 8);
            dex_build();
            pet_audio_play(SND_CONFIRM);
        }
    }
}

// ---------------------------------------------------------------------------
// G1 猜大小
// ---------------------------------------------------------------------------

static void game_set_choice_visual(void)
{
    // 选中：粉底粉框墨字；常态：HIGH 浅绿 / LOW 浅蓝，墨框本色字。
    bool hi = s_g_choice == PET_GAME_HIGH;
    lv_obj_set_style_bg_color(s_game_hi,
                              lv_color_hex(hi ? COL_CARD_PINK
                                              : COL_CARD_GREENBG), 0);
    border(s_game_hi, hi ? COL_SEL : COL_INK, hi ? 2 : 1);
    lv_obj_set_style_text_color((lv_obj_t *) lv_obj_get_child(s_game_hi, 0),
                                lv_color_hex(hi ? COL_INK : COL_GREEN), 0);
    lv_obj_set_style_bg_color(s_game_lo,
                              lv_color_hex(hi ? COL_BADGE_BLUE
                                              : COL_CARD_PINK), 0);
    border(s_game_lo, hi ? COL_INK : COL_SEL, hi ? 1 : 2);
    lv_obj_set_style_text_color((lv_obj_t *) lv_obj_get_child(s_game_lo, 0),
                                lv_color_hex(hi ? COL_BLUE : COL_INK), 0);
}

// 两键恢复常态（结果态：无焦点）。
static void game_buttons_idle(void)
{
    lv_obj_set_style_bg_color(s_game_hi,
                              lv_color_hex(COL_CARD_GREENBG), 0);
    border(s_game_hi, COL_INK, 1);
    lv_obj_set_style_text_color((lv_obj_t *) lv_obj_get_child(s_game_hi, 0),
                                lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_bg_color(s_game_lo, lv_color_hex(COL_BADGE_BLUE), 0);
    border(s_game_lo, COL_INK, 1);
    lv_obj_set_style_text_color((lv_obj_t *) lv_obj_get_child(s_game_lo, 0),
                                lv_color_hex(COL_BLUE), 0);
}

static void game_pips_refresh(uint8_t wins)
{
    for (uint8_t i = 0; i < 5; i += 1) {
        bool on = i < wins;
        lv_obj_set_style_bg_color(s_game_pip[i],
                                  lv_color_hex(on ? COL_COIN_BG
                                                  : COL_PIP_OFF), 0);
        border(s_game_pip[i], on ? COL_INK : COL_INK, on ? 1 : 0);
    }
}

static void game_build(void)
{
    lv_obj_clean(s_modal_body);
    memset(s_pv2_card, 0, sizeof(s_pv2_card));
    frame_pv2(false);
    lv_label_set_text(s_modal_title, "HI-LOW");

    char buf[16];
    // 中央卡 64x62
    s_game_box = rect(s_modal_body, 64, 10, 64, 62, 10, COL_CARD_WHITE);
    border(s_game_box, COL_INK, 1);
    snprintf(buf, sizeof(buf), "[ %d ]",
             (int) pet_game_first_card(&s_game));
    s_game_card = label(s_game_box, 0, 21, 64, &lv_font_montserrat_20,
                        COL_INK, buf);
    s_game_sub = label(s_game_box, 0, 12, 64, &lv_font_montserrat_12,
                       COL_SUB, "");
    s_game_result = label(s_game_box, 0, 30, 64, &lv_font_montserrat_20,
                          COL_INK, "");
    hide(s_game_sub);
    hide(s_game_result);

    // HIGH / LOW
    s_game_hi = rect(s_modal_body, 2, 80, 84, 30, 9, COL_CARD_GREENBG);
    label(s_game_hi, 0, 7, 84, &lv_font_montserrat_14, COL_GREEN, "HIGH");
    s_game_lo = rect(s_modal_body, 106, 80, 84, 30, 9, COL_BADGE_BLUE);
    label(s_game_lo, 0, 7, 84, &lv_font_montserrat_14, COL_BLUE, "LOW");
    game_set_choice_visual();

    // 5 档胜场灯
    for (uint8_t i = 0; i < 5; i += 1) {
        s_game_pip[i] = rect(s_modal_body, 82 + (int32_t) i * 8, 112,
                             6, 6, 2, COL_PIP_OFF);
    }
    game_pips_refresh((uint8_t) pet_game_wins(&s_game));

    s_game_hint = label(s_modal_body, 0, 132, 192,
                        &lv_font_montserrat_12, COL_HINT,
                        "UP/DOWN choose  OK deal");
}

static void game_open(void)
{
    if (s_snap.sick) {
        set_msg("Not feeling well", 1200);
        pet_audio_play(SND_SAD);
        return;
    }
    uint32_t seed = (uint32_t) pet_clock_uptime_ms()
        ^ (uint32_t) (s_snap.boot_count * 2246822519u);
    pet_game_init(&s_game, seed);
    s_g_phase = PHASE_CHOOSE;
    s_g_choice = PET_GAME_HIGH;
    s_g_exit_armed_until = 0;
    s_mode = MODE_GAME;
    modal_open();
    game_build();
    pet_audio_play(SND_CONFIRM);
}

static const uint8_t G1_SCORE[6] = { 10, 34, 60, 85, 100, 100 };

// S4 健身角：游戏/打工原始分 +10%（先于体力 ×80% 与老年放宽，05 §7.2）。
static uint8_t decor_adjust_score(uint8_t score)
{
    uint8_t pct = pt_decor_score_pct(&s_dsnap);
    uint16_t v = (uint16_t) ((uint16_t) score
                             + (uint16_t) score * pct / 100u);
    return v > 100 ? 100 : (uint8_t) v;
}

// G1 打工短关结算：3 局制，老年放宽，走工资而非游戏奖金。
static void g1_job_finish(void)
{
    uint8_t wins = pet_game_wins(&s_game);
    uint8_t score = decor_adjust_score(G1_SCORE[wins > 5 ? 5 : wins]);
    uint8_t grade = pet_grade_for_job(score, s_snap.energy,
                                      s_snap.stage == PT_STAGE_SENIOR);
    pet_app_job_work(s_jobs.job, grade);
    s_mood = MOOD_HAPPY;
    s_mood_until = now_ms() + 900;
    s_g1_job = false;
    modal_close();
}

static void game_exit(void)
{
    uint8_t wins = pet_game_wins(&s_game);
    if (s_g1_job) {
        // 长按退出 = 中途放弃：与 G2–G6 短关口径一致，无工资（07 §2）。
        s_g1_job = false;
        pet_audio_play(SND_CANCEL);
        modal_close();
        return;
    }
    if (wins > 0) {
        uint8_t score = decor_adjust_score(G1_SCORE[wins > 5 ? 5 : wins]);
        uint8_t grade = pet_grade_from_score(score, s_snap.energy);
        pet_app_game_result(PT_GAME_G1_HILO, grade);
        s_mood = MOOD_HAPPY;
        s_mood_until = now_ms() + 900;
    } else {
        pet_audio_play(SND_CANCEL);
    }
    modal_close();
}

static void game_key_click(pet_btn_t btn)
{
    if (s_g_phase == PHASE_CHOOSE) {
        if (btn == PET_BTN_UP) {
            s_g_choice = PET_GAME_HIGH;
            game_set_choice_visual();
            pet_audio_play(SND_CONFIRM);
        } else if (btn == PET_BTN_DOWN) {
            s_g_choice = PET_GAME_LOW;
            game_set_choice_visual();
            pet_audio_play(SND_CONFIRM);
        } else if (btn == PET_BTN_OK) {
            pet_game_result_t r = pet_game_choose(&s_game, s_g_choice);
            s_g_phase = PHASE_RESULT;
            char cap[16];
            snprintf(cap, sizeof(cap), "%d -> %d",
                     (int) pet_game_first_card(&s_game),
                     (int) pet_game_second_card(&s_game));
            lv_label_set_text(s_game_sub, cap);
            show(s_game_sub);
            hide(s_game_card);
            if (r == PET_GAME_WIN) {
                lv_label_set_text(s_game_result, "WIN!");
                lv_obj_set_style_text_color(s_game_result,
                                            lv_color_hex(COL_GREEN), 0);
                lv_obj_set_style_bg_color(s_game_box,
                                          lv_color_hex(COL_CARD_GREENBG), 0);
                border(s_game_box, COL_GREEN, 1);
                pet_audio_play(SND_HAPPY);
            } else if (r == PET_GAME_LOSE) {
                lv_label_set_text(s_game_result, "LOST");
                lv_obj_set_style_text_color(s_game_result,
                                            lv_color_hex(COL_RED), 0);
                pet_audio_play(SND_SAD);
            } else {
                lv_label_set_text(s_game_result, "TIE");
                lv_obj_set_style_text_color(s_game_result,
                                            lv_color_hex(COL_INK), 0);
                pet_audio_play(SND_CONFIRM);
            }
            show(s_game_result);
            game_buttons_idle();
            game_pips_refresh((uint8_t) pet_game_wins(&s_game));
            lv_label_set_text(s_game_hint, "OK - next");
        }
    } else {
        // RESULT：OK 进入下一局（TIE 时第一张已在 choose 内重发）。
        if (btn != PET_BTN_OK) {
            return;
        }
        if (s_g1_job && s_game.played >= 3) {
            g1_job_finish();   // 打工短关 3 局封顶
            return;
        }
        pet_game_next_round(&s_game);
        s_g_phase = PHASE_CHOOSE;
        game_build();
    }
}

// ---------------------------------------------------------------------------
// 游戏选择器 + G2–G6 运行时（07 §2 统一框架：READY → 游玩 → 评级结算）
// ---------------------------------------------------------------------------

static const char *const GAMES_NAME[PT_GAME_COUNT] = {
    "Hi-Lo", "Rhythm", "Catch", "Memory", "Walk", "Prefer",
};
static const char *const GAMES_TITLE[PT_GAME_COUNT] = {
    "HI-LOW", "RHYTHM", "CATCH", "MEMORY", "WALK", "PREFER",
};
static const char *const GAMES_SKILL[PT_GAME_COUNT] = {
    "Mind", "Art", "Body", "Mind", "Body", "Bond",
};
static const char *const GAMES_HINT[PT_GAME_COUNT] = {
    "pick hi / lo", "tap the beat", "move 3 lanes",
    "repeat pads", "jump or duck", "read the mood",
};
static const pet_ui_card_t GAMES_ART[PT_GAME_COUNT] = {
    PET_UI_CARD_HILO, PET_UI_CARD_RHYTHM, PET_UI_CARD_CATCH,
    PET_UI_CARD_MEMORY, PET_UI_CARD_WALK, PET_UI_CARD_PREFER,
};
// 技能词配色（定稿 pv2-games.html）：mind 蓝 / art 黄 / body 绿 / bond 粉。
static const uint32_t GAMES_SKILL_COL[PT_GAME_COUNT] = {
    COL_BLUE, COL_YELLOW, COL_GREEN, COL_BLUE, COL_GREEN, COL_SEL,
};
// 两页：页 0 放 G1–G4，页 1 放 G5–G6。
static const uint8_t GAMES_PAGE_FIRST[2] = { 0, 4 };
static const uint8_t GAMES_PAGE_N[2] = { 4, 2 };

// ---- 选择器 ---------------------------------------------------------------

static void gx_open(uint8_t id);   // G2–G6 运行时在下方定义

static void games_build(void)
{
    lv_obj_clean(s_modal_body);
    memset(s_pv2_card, 0, sizeof(s_pv2_card));
    frame_pv2(true);
    footer_pager_only();
    lv_label_set_text(s_modal_title, "GAMES");

    char buf[8];
    snprintf(buf, sizeof(buf), "%u/2", (unsigned) (s_games_page + 1));
    lv_label_set_text(s_mp_cur, buf);
    lv_obj_set_pos(s_mp_cur, 91, 15);
    show(s_mp_cur);

    s_focus_n = 0;
    uint8_t base = GAMES_PAGE_FIRST[s_games_page];
    uint8_t n = GAMES_PAGE_N[s_games_page];
    for (uint8_t i = 0; i < n; i += 1) {
        uint8_t id = (uint8_t) (base + i);
        int32_t y = (int32_t) i * 40;
        lv_obj_t *r = rect(s_modal_body, 0, y, 192, 36, 10, COL_CARD_WHITE);
        border(r, COL_INK, 1);
        icon_img(r, 4, 4, pet_ui_card_dsc(GAMES_ART[id]));
        llabel(r, 40, 3, 132, &lv_font_montserrat_14, COL_INK,
               GAMES_NAME[id]);
        llabel(r, 40, 19, 44, &lv_font_montserrat_12,
               GAMES_SKILL_COL[id], GAMES_SKILL[id]);
        char sub[24];
        snprintf(sub, sizeof(sub), "- %s", GAMES_HINT[id]);
        llabel(r, 40 + text_w12(GAMES_SKILL[id]) + 5, 19, 120,
               &lv_font_montserrat_12, COL_SUB, sub);
        s_pv2_card[i] = r;
        s_pv2_card_bg[i] = COL_CARD_WHITE;
        focus_add(PV2_F_CARD, i);
    }
    focus_add(PV2_F_PREV, 0);
    focus_add(PV2_F_NEXT, 0);

    if ((uint8_t) s_list_sel >= s_focus_n) {
        s_list_sel = 0;
    }
    // pv2_mark() 的端点失效样式读商店页码；GAMES 借用同一套页脚控件。
    s_shop_page = s_games_page;
    s_shop_pages = 2;
    pv2_mark((uint8_t) s_list_sel, true);
}

static void games_open(void)
{
    if (s_snap.sick) {
        set_msg("Not feeling well", 1200);
        pet_audio_play(SND_SAD);
        return;
    }
    s_games_page = 0;
    s_list_sel = 0;
    s_mode = MODE_GAMES;
    modal_open();
    games_build();
    pet_audio_play(SND_CONFIRM);
}

static void handle_games_key(pet_ui_action_t act)
{
    uint8_t slots = s_focus_n;
    if (act == PET_UI_ACT_PREV) {
        s_list_sel = (s_list_sel + slots - 1) % slots;
        pv2_mark((uint8_t) s_list_sel, true);
        pet_audio_play(SND_CONFIRM);
    } else if (act == PET_UI_ACT_NEXT) {
        s_list_sel = (s_list_sel + 1) % slots;
        pv2_mark((uint8_t) s_list_sel, true);
        pet_audio_play(SND_CONFIRM);
    } else if (act == PET_UI_ACT_CONFIRM) {
        uint8_t kind = s_focus_kind[s_list_sel];
        uint8_t param = s_focus_param[s_list_sel];
        if (kind == PV2_F_CARD) {
            uint8_t id = (uint8_t) (GAMES_PAGE_FIRST[s_games_page] + param);
            if (id == PT_GAME_G1_HILO) {
                s_g1_job = false;
                game_open();
            } else {
                gx_open(id);
            }
        } else if (kind == PV2_F_PREV) {
            if (s_games_page > 0) {
                s_games_page -= 1;
                s_list_sel = 0;
                games_build();
                pet_audio_play(SND_CONFIRM);
            }
        } else if (kind == PV2_F_NEXT) {
            if (s_games_page + 1 < 2) {
                s_games_page += 1;
                s_list_sel = 0;
                games_build();
                pet_audio_play(SND_CONFIRM);
            }
        }
    } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        pet_audio_play(SND_CANCEL);
        modal_close();
    }
}

// ---- GX 公共部件（PV2 主体 192x156 局部坐标） ------------------------------

static void gx_hide_all(void)
{
    for (uint8_t i = 0; i < GX_MK_N; i += 1) {
        hide(s_gx_mk[i]);
    }
    for (uint8_t i = 0; i < 4; i += 1) {
        hide(s_gx_ml[i]);
    }
    for (uint8_t i = 0; i < GX_IMG_N; i += 1) {
        hide(s_gx_img[i]);
    }
    hide(s_gx_card);
    hide(s_gx_card_icon);
    hide(s_gx_card_l1);
    hide(s_gx_card_l2);
    hide(s_gx_card_l3);
}

// 圆角色块：bw=0 时无边。
static void gx_box(uint8_t i, int32_t x, int32_t y, int32_t w, int32_t h,
                   int32_t rad, uint32_t fill, uint32_t bcol, int32_t bw)
{
    lv_obj_t *o = s_gx_mk[i];
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, rad, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
    border(o, bcol, bw);
    show(o);
}

static void gx_txt(uint8_t i, int32_t x, int32_t y, int32_t w, uint32_t color,
                   const char *txt)
{
    lv_obj_set_pos(s_gx_ml[i], x, y);
    lv_obj_set_width(s_gx_ml[i], w);
    lv_obj_set_style_text_color(s_gx_ml[i], lv_color_hex(color), 0);
    lv_label_set_text(s_gx_ml[i], txt);
    show(s_gx_ml[i]);
}

static void gx_img(uint8_t i, int32_t x, int32_t y,
                   const lv_image_dsc_t *dsc)
{
    lv_image_set_src(s_gx_img[i], dsc);
    lv_obj_set_pos(s_gx_img[i], x, y);
    show(s_gx_img[i]);
}

// 顶部居中信息胶囊（奶底 r8；hot=粉，用于 WATCH 等强调态）。
static void gx_info(const char *txt, bool hot)
{
    int32_t w = text_w12(txt) + 16;
    if (w < 44) {
        w = 44;
    }
    lv_obj_set_pos(s_gx_pill, 96 - w / 2, 0);
    lv_obj_set_size(s_gx_pill, w, 16);
    lv_obj_set_style_bg_color(s_gx_pill,
                              lv_color_hex(hot ? COL_PILL_PINK : COL_DOCK_BG),
                              0);
    border(s_gx_pill, hot ? COL_SEL : COL_DOCK_EDGE, 1);
    lv_label_set_text(s_gx_info, txt);
    show(s_gx_pill);
    show(s_gx_info);
}

static void gx_hint_set(const char *txt)
{
    lv_label_set_text(s_gx_hint, txt);
    show(s_gx_hint);
}

static const char *gx_grade_word(uint8_t grade)
{
    if (grade == PT_GAME_GRADE_PERFECT) {
        return "PERFECT!";
    }
    if (grade == PT_GAME_GRADE_GREAT) {
        return "GREAT!";
    }
    return "GOOD";
}

static void gx_finish(void);
static void gx_ready_chrome(void);
static void gx_result_chrome(void);

// ---- 各游戏绘制 -----------------------------------------------------------

static void gx_render_g2(void)
{
    gx_hide_all();
    char buf[24];
    snprintf(buf, sizeof(buf), "BEAT %d/5",
             s_g2.beat + (s_g2.done ? 0 : 1));
    gx_info(buf, false);
    gx_hint_set("OK - tap on center");
    gx_box(0, 8, 52, 176, 14, 7, COL_DIS_BG, COL_INK, 0);      // 判定槽
    gx_box(1, 66, 52, 60, 14, 7, COL_PLAY_SWEET, COL_INK, 0);  // 甜区
    int32_t cx = 8 + (s_g2.pos + 100) * 176 / 200;
    gx_box(2, cx - 2, 50, 4, 18, 2, COL_GREEN_DEEP, COL_INK, 0);
    for (uint8_t i = 0; i < PET_G2_BEATS; i += 1) {
        bool done = i < s_g2.beat;
        bool cur = !s_g2.done && i == s_g2.beat;
        gx_box(3 + i, 76 + (int32_t) i * 10, 82, 7, 7, 3,
               done ? COL_GREEN : (cur ? COL_TAB_ON : COL_PIP_OFF),
               COL_INK, (done || cur) ? 1 : 0);
    }
}

static void gx_render_g3(void)
{
    gx_hide_all();
    char buf[26];
    snprintf(buf, sizeof(buf), "%ds  %d",
             (int) ((PET_G3_TICKS - s_g3.tick + 9) / 10),
             (int) s_g3.score);
    gx_info(buf, false);
    gx_hint_set("UP/DOWN - move");
    // 三车道：当前车道白底，其余透明仅发丝线。
    for (uint8_t i = 0; i < PET_G3_LANES; i += 1) {
        gx_box(i, 8 + (int32_t) i * 58, 22, 56, 92, 8,
               i == s_g3.player ? COL_CARD_WHITE : COL_PANEL,
               COL_HAIR, 1);
        if (i != s_g3.player) {
            lv_obj_set_style_bg_opa(s_gx_mk[i], LV_OPA_TRANSP, 0);
        }
    }
    // 掉落物：好=绿圆，坏=红方块。
    uint8_t used = 3;
    for (uint8_t i = 0; i < 5; i += 1) {
        if (s_g3.drops[i].lane < 0 || used > 7) {
            continue;
        }
        int32_t cx = 36 + (int32_t) s_g3.drops[i].lane * 58;
        int32_t y = 30 + (4 - s_g3.drops[i].row) * 20;
        if (s_g3.drops[i].bad) {
            gx_box(used, cx - 6, y, 12, 12, 3, COL_RED, COL_INK, 1);
        } else {
            gx_box(used, cx - 7, y - 1, 14, 14, 7, COL_GREEN, COL_INK, 1);
        }
        used += 1;
    }
    // 笑脸碗接球手。
    int32_t px = 36 + (int32_t) s_g3.player * 58;
    gx_box(8, px - 18, 104, 36, 12, 6, COL_CARD_WHITE, COL_INK, 1);
    gx_box(9, px - 9, 96, 5, 5, 2, COL_TAB_ON, COL_INK, 1);
    gx_box(10, px + 4, 96, 5, 5, 2, COL_TAB_ON, COL_INK, 1);
}

// G4 三块 A/B/C 垫（主体 y24/58/92）。
static void gx_pads(bool lit_mode)
{
    static const char *const PAD[3] = { "A", "B", "C" };
    uint8_t mark = lit_mode ? s_g4_lit : s_gx_sel;
    for (uint8_t i = 0; i < 3; i += 1) {
        bool on = i == mark;
        gx_box(i, 8, 24 + (int32_t) i * 34, 176, 30, 10,
               on ? COL_CARD_PINK : COL_CARD_WHITE,
               on ? COL_SEL : COL_INK, on ? 2 : 1);
        gx_txt(i, 8, 31 + (int32_t) i * 34, 176, COL_INK, PAD[i]);
    }
}

static void gx_render_g4(void)
{
    gx_hide_all();
    if (s_g4.phase == PET_G4_SHOW) {
        gx_info("WATCH...", true);
        gx_hint_set("watch the pads");
        gx_pads(true);
    } else {
        char buf[20];
        snprintf(buf, sizeof(buf), "REPEAT %d/7",
                 s_g4.level > 7 ? 7 : (int) s_g4.level);
        gx_info(buf, false);
        gx_hint_set("UP/DOWN - OK");
        gx_pads(false);
    }
}

static void gx_render_g5(void)
{
    gx_hide_all();
    char buf[28];
    snprintf(buf, sizeof(buf), "%d/%d  combo %d", (int) s_g5.cleared,
             PET_G5_OBSTACLES, (int) s_g5.combo);
    gx_info(buf, false);
    gx_hint_set("UP jump  DOWN duck");
    gx_box(0, 8, 106, 176, 2, 1, COL_INK, COL_INK, 0);          // 地面
    gx_box(1, 24, 84, 20, 20, 7, COL_BADGE_BLUE, COL_INK, 1);  // 玩家
    gx_box(2, 30, 91, 3, 3, 1, COL_INK, COL_INK, 0);           // 眼
    gx_box(3, 27, 104, 5, 4, 2, COL_BLUE, COL_INK, 1);         // 腿
    gx_box(4, 37, 104, 5, 4, 2, COL_BLUE, COL_INK, 1);
    if (s_g5.next_obs < PET_G5_OBSTACLES) {
        const pet_g5_obs_t *o = &s_g5.obs[s_g5.next_obs];
        int32_t d = o->arrive - s_g5.tick;
        int32_t x = 30 + d * 5;
        if (x < 48) {
            x = 48;
        }
        if (x > 172) {
            x = 172;
        }
        if (o->action == PET_G5_UP) {
            gx_box(5, x - 5, 90, 10, 16, 2, COL_COIN_BG, COL_INK, 1);
        } else {
            // 顶线垂下的横杆（DOWN 蹲）。
            gx_box(6, x, 22, 1, 32, 0, COL_GREEN, COL_GREEN, 1);
            gx_box(7, x - 14, 54, 28, 9, 3, COL_CARD_GREENBG,
                   COL_GREEN, 1);
        }
    }
}

static void gx_render_g6(void)
{
    gx_hide_all();
    static const char *const HINT[3] = {
        "Hungry!", "Playful!", "Cuddly!",
    };
    static const char *const OPT[3] = { "Food", "Toy", "Hug" };
    static const pet_ui_card_t OPT_ART[3] = {
        PET_UI_CARD_MEAL, PET_UI_CARD_BALL, PET_UI_CARD_BOND,
    };
    char buf[16];
    snprintf(buf, sizeof(buf), "Q %d/%d",
             pet_g6_done(&s_g6) ? PET_G6_QUESTIONS : s_g6.question + 1,
             PET_G6_QUESTIONS);
    gx_info(buf, false);
    gx_hint_set("UP/DOWN - OK");
    // 心情气泡
    gx_box(0, 8, 18, 176, 20, 8, COL_CARD_WHITE, COL_INK, 1);
    gx_txt(0, 8, 22, 176, COL_INK, HINT[s_g6.hint]);
    // 三个选项行（图标 28px 缩到 ~22px）
    for (uint8_t i = 0; i < 3; i += 1) {
        int32_t y = 44 + (int32_t) i * 26;
        bool on = i == s_gx_sel;
        gx_box(1 + i, 8, y, 176, 24, 9,
               on ? COL_CARD_PINK : COL_CARD_WHITE,
               on ? COL_SEL : COL_INK, on ? 2 : 1);
        gx_img(i, 12, y + 1, pet_ui_card_dsc(OPT_ART[i]));
        gx_txt(1 + i, 8, y + 6, 176, COL_INK, OPT[i]);
    }
}

static void gx_render(void)
{
    if (s_gx_phase == 0) {
        gx_hide_all();
        gx_ready_chrome();
        return;
    }
    if (s_gx_phase == 2) {
        gx_hide_all();
        gx_result_chrome();
        return;
    }
    switch (s_gx_id) {
    case PT_GAME_G2_RHYTHM: gx_render_g2(); break;
    case PT_GAME_G3_CATCH:  gx_render_g3(); break;
    case PT_GAME_G4_MEMORY: gx_render_g4(); break;
    case PT_GAME_G5_WALK:   gx_render_g5(); break;
    case PT_GAME_G6_PREFER: gx_render_g6(); break;
    default: break;
    }
}

// ---- 生命周期 -------------------------------------------------------------

static const char *gx_hint_text(void)
{
    switch (s_gx_id) {
    case PT_GAME_G2_RHYTHM: return "OK - tap on center";
    case PT_GAME_G3_CATCH:  return "UP/DOWN - move";
    case PT_GAME_G4_MEMORY: return "UP/DOWN - OK";
    case PT_GAME_G5_WALK:   return "UP jump  DOWN duck";
    case PT_GAME_G6_PREFER: return "UP/DOWN - OK";
    default:                return "";
    }
}

// READY 中央卡：图标骑卡顶边，1.2s 自动开始（gx_refresh 推进）。
// 定稿坐标为面板系，body 原点面板 (8,42)，以下均换算为 body 局部坐标。
static void gx_ready_chrome(void)
{
    lv_obj_set_pos(s_gx_card, 20, 2);
    lv_obj_set_size(s_gx_card, 136, 86);
    lv_image_set_src(s_gx_card_icon, pet_ui_card_dsc(GAMES_ART[s_gx_id]));
    lv_obj_set_pos(s_gx_card_icon, 74, -12);
    lv_obj_set_style_text_font(s_gx_card_l1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_gx_card_l1, lv_color_hex(COL_INK), 0);
    lv_obj_set_pos(s_gx_card_l1, 20, 27);
    lv_label_set_text(s_gx_card_l1, GAMES_NAME[s_gx_id]);
    lv_obj_set_pos(s_gx_card_l2, 20, 48);
    lv_label_set_text(s_gx_card_l2, "GET READY");
    lv_obj_set_pos(s_gx_card_l3, 20, 67);
    lv_label_set_text(s_gx_card_l3, gx_hint_text());
    show(s_gx_card);
    show(s_gx_card_icon);
    show(s_gx_card_l1);
    show(s_gx_card_l2);
    show(s_gx_card_l3);
    hide(s_gx_pill);
    hide(s_gx_info);
    hide(s_gx_hint);
}

// RESULT 中央卡：评级 + 分数/工资，1.5s 自动退。
static void gx_result_chrome(void)
{
    lv_obj_set_pos(s_gx_card, 20, 0);
    lv_obj_set_size(s_gx_card, 136, 80);
    lv_obj_set_style_text_font(s_gx_card_l1, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(s_gx_card_l1, 20, 14);
    lv_obj_set_pos(s_gx_card_l2, 20, 43);
    lv_obj_set_pos(s_gx_card_l3, 20, 60);
    lv_label_set_text(s_gx_card_l3, "OK");
    show(s_gx_card);
    show(s_gx_card_l1);
    show(s_gx_card_l2);
    show(s_gx_card_l3);
    hide(s_gx_card_icon);
    hide(s_gx_hint);
}

static void gx_build_shell(void)
{
    lv_obj_clean(s_modal_body);
    frame_pv2(false);
    lv_label_set_text(s_modal_title, GAMES_TITLE[s_gx_id]);

    // 信息胶囊
    s_gx_pill = rect(s_modal_body, 80, 0, 32, 16, 8, COL_DOCK_BG);
    border(s_gx_pill, COL_DOCK_EDGE, 1);
    hide(s_gx_pill);
    s_gx_info = label(s_modal_body, 0, 2, 192, &lv_font_montserrat_12,
                      COL_INK, "");
    hide(s_gx_info);
    s_gx_hint = label(s_modal_body, 0, 132, 192, &lv_font_montserrat_12,
                      COL_HINT, "");
    hide(s_gx_hint);

    for (uint8_t i = 0; i < GX_MK_N; i += 1) {
        s_gx_mk[i] = rect(s_modal_body, 0, 0, 4, 4, 4, COL_PANEL);
        border(s_gx_mk[i], COL_INK, 0);
        hide(s_gx_mk[i]);
    }
    for (uint8_t i = 0; i < 4; i += 1) {
        s_gx_ml[i] = label(s_modal_body, 0, 0, 192,
                           &lv_font_montserrat_12, COL_INK, "");
        hide(s_gx_ml[i]);
    }
    // G6 选项图标：28px 位图以左上角为锚缩放到 ~22px。
    for (uint8_t i = 0; i < GX_IMG_N; i += 1) {
        s_gx_img[i] = icon_img(s_modal_body, 0, 0,
                               pet_ui_card_dsc(PET_UI_CARD_MEAL));
        lv_obj_set_style_transform_pivot_x(s_gx_img[i], 0, 0);
        lv_obj_set_style_transform_pivot_y(s_gx_img[i], 0, 0);
        lv_image_set_scale(s_gx_img[i], 205);
        hide(s_gx_img[i]);
    }

    // READY / RESULT 共用中央卡（创建坐标为 body 局部；定稿面板坐标 - (8,42)）。
    s_gx_card = rect(s_modal_body, 20, 2, 136, 86, 14, COL_CARD_WHITE);
    border(s_gx_card, COL_INK, 1);
    hide(s_gx_card);
    s_gx_card_icon = icon_img(s_modal_body, 74, -12,
                              pet_ui_card_dsc(GAMES_ART[s_gx_id]));
    hide(s_gx_card_icon);
    s_gx_card_l1 = label(s_modal_body, 20, 27, 136,
                         &lv_font_montserrat_14, COL_INK, "");
    s_gx_card_l2 = label(s_modal_body, 20, 48, 136,
                         &lv_font_montserrat_12, COL_SEL, "GET READY");
    lv_obj_set_style_text_letter_space(s_gx_card_l2, 2, 0);
    s_gx_card_l3 = label(s_modal_body, 20, 67, 136,
                         &lv_font_montserrat_12, COL_SUB, "");
    hide(s_gx_card_l1);
    hide(s_gx_card_l2);
    hide(s_gx_card_l3);
}

static void gx_open(uint8_t id)
{
    if (s_snap.sick) {
        set_msg("Not feeling well", 1200);
        pet_audio_play(SND_SAD);
        return;
    }
    s_gx_id = id;
    s_gx_job = false;
    uint32_t seed = (uint32_t) pet_clock_uptime_ms()
        ^ (uint32_t) (s_snap.boot_count * 2246822519u)
        ^ (uint32_t) (id * 2654435761u);
    switch (id) {
    case PT_GAME_G2_RHYTHM: pet_g2_init(&s_g2); s_g2_from = -100; break;
    case PT_GAME_G3_CATCH:  pet_g3_init(&s_g3, seed); break;
    case PT_GAME_G4_MEMORY: pet_g4_init(&s_g4, seed); s_g4_lit = 0xFF; break;
    case PT_GAME_G5_WALK:   pet_g5_init(&s_g5, seed); break;
    case PT_GAME_G6_PREFER: pet_g6_init(&s_g6, seed, PT_FOOD_TAG_MEAL);
                            s_gx_sel = 0; break;
    default: return;
    }
    s_gx_phase = 0;
    s_gx_t0 = now_ms();
    s_gx_next_ms = 0;
    s_gx_exit_armed_until = 0;
    s_mode = MODE_GX;
    modal_open();
    gx_build_shell();
    gx_render();
    pet_audio_play(SND_CONFIRM);
}

static void gx_finish(void)
{
    uint8_t score = 0;
    switch (s_gx_id) {
    case PT_GAME_G2_RHYTHM: score = pet_g2_score(&s_g2); break;
    case PT_GAME_G3_CATCH:  score = pet_g3_score(&s_g3); break;
    case PT_GAME_G4_MEMORY: score = pet_g4_score(&s_g4); break;
    case PT_GAME_G5_WALK:   score = pet_g5_score(&s_g5); break;
    case PT_GAME_G6_PREFER: score = pet_g6_score(&s_g6); break;
    default: break;
    }
    score = decor_adjust_score(score);
    uint8_t grade = pet_grade_from_score(score, s_snap.energy);
    if (s_gx_job) {        // 打工短关：老年阈值放宽 10%，结算走工资（引擎只扣体力，不碰游戏预算）。
        grade = pet_grade_for_job(score, s_snap.energy,
                                  s_snap.stage == PT_STAGE_SENIOR);
        pet_app_job_work(s_jobs.job, grade);
    } else {
        pet_app_game_result(s_gx_id, grade);
    }
    s_gx_phase = 2;
    s_gx_t0 = now_ms();
    s_mood = MOOD_HAPPY;
    s_mood_until = now_ms() + 900;
    char buf[24];
    if (s_gx_job) {
        snprintf(buf, sizeof(buf), "+%u G wage",
                 (unsigned) pt_job_wage((pt_job_id_t) s_jobs.job, grade));
    } else {
        snprintf(buf, sizeof(buf), "Score %d", (int) score);
    }
    gx_hide_all();
    lv_obj_set_style_text_color(s_gx_card_l1,
                                lv_color_hex(grade == PT_GAME_GRADE_GOOD
                                             ? COL_INK : COL_GREEN), 0);
    lv_label_set_text(s_gx_card_l1, gx_grade_word(grade));
    lv_label_set_text(s_gx_card_l2, buf);
    gx_result_chrome();
    pet_audio_play(grade == PT_GAME_GRADE_GOOD ? SND_CONFIRM : SND_HAPPY);
}

// G2 一拍结束（按下或漏拍）：满 5 拍结算，否则从另一端重新扫。
static void g2_after_beat(void)
{
    if (s_g2.done) {
        gx_finish();
        return;
    }
    s_g2_from = (int16_t) -s_g2_from;
    s_g2.pos = (int8_t) s_g2_from;
    s_g2.dir = s_g2_from < 0 ? 1 : -1;
    gx_render_g2();
}

// 定时器推进（仅 LVGL 任务内调用，与输入串行）。
static void gx_refresh(int64_t now)
{
    if (s_gx_phase == 0) {
        if (now - s_gx_t0 >= 1200) {
            s_gx_phase = 1;
            s_gx_next_ms = now;
            gx_render();
        }
        return;
    }
    if (s_gx_phase == 2) {
        if (now - s_gx_t0 >= 1500) {
            modal_close();
        }
        return;
    }
    switch (s_gx_id) {
    case PT_GAME_G2_RHYTHM:
        pet_g2_tick(&s_g2);
        if (s_g2.pos == (int8_t) -s_g2_from) {
            pet_g2_miss(&s_g2);
            pet_audio_play(SND_SAD);
            g2_after_beat();
        } else {
            gx_render_g2();
        }
        break;
    case PT_GAME_G3_CATCH:
        // 打工短关：15s（普通游戏 30s）。
        if (s_gx_job && s_g3.tick >= PET_G3_TICKS / 2) {
            gx_finish();
            break;
        }
        pet_g3_tick(&s_g3);
        pet_g3_tick(&s_g3);
        if (pet_g3_done(&s_g3)) {
            gx_finish();
        } else {
            gx_render_g3();
        }
        break;
    case PT_GAME_G4_MEMORY:
        if (s_g4.phase == PET_G4_SHOW && now >= s_gx_next_ms) {
            uint8_t pad = pet_g4_next_pad(&s_g4);
            if (pad == 0xFFu) {
                pet_g4_start_input(&s_g4);
                s_gx_sel = 0;
                s_gx_next_ms = now + 300;
            } else {
                s_g4_lit = pad;
                s_gx_next_ms = now + 420;
            }
            gx_render_g4();
        }
        break;
    case PT_GAME_G5_WALK:
        // 打工短关：4 个障碍（普通散步 6 个）。
        if (s_gx_job && s_g5.next_obs >= 4) {
            gx_finish();
            break;
        }
        pet_g5_tick(&s_g5);
        if (pet_g5_done(&s_g5)) {
            gx_finish();
        } else {
            gx_render_g5();
        }
        break;
    case PT_GAME_G6_PREFER:
        break;
    default:
        break;
    }
}

static void gx_click(pet_btn_t btn)
{
    if (s_gx_phase == 0) {
        s_gx_phase = 1;                       // 跳过 READY
        s_gx_t0 = now_ms();
        s_gx_next_ms = now_ms();
        gx_render();
        return;
    }
    if (s_gx_phase == 2) {
        modal_close();
        return;
    }
    switch (s_gx_id) {
    case PT_GAME_G2_RHYTHM:
        if (btn == PET_BTN_OK) {
            uint8_t d = (uint8_t) (s_g2.pos < 0 ? -s_g2.pos : s_g2.pos);
            pet_g2_press(&s_g2);
            pet_audio_play(d <= 30 ? SND_HAPPY : SND_CONFIRM);
            g2_after_beat();
        }
        break;
    case PT_GAME_G3_CATCH:
        if (btn == PET_BTN_UP) {
            pet_g3_move(&s_g3, -1);
        } else if (btn == PET_BTN_DOWN) {
            pet_g3_move(&s_g3, 1);
        }
        gx_render_g3();
        break;
    case PT_GAME_G4_MEMORY:
        if (s_g4.phase != PET_G4_INPUT) {
            break;
        }
        if (btn == PET_BTN_UP) {
            s_gx_sel = (s_gx_sel + 2) % 3;
            gx_render_g4();
        } else if (btn == PET_BTN_DOWN) {
            s_gx_sel = (s_gx_sel + 1) % 3;
            gx_render_g4();
        } else if (btn == PET_BTN_OK) {
            if (pet_g4_input(&s_g4, s_gx_sel)) {
                pet_audio_play(SND_CONFIRM);
                if (s_g4.phase == PET_G4_SHOW) {
                    // 打工短关：过完 5 级即结算（普通 7 级）。
                    if (s_gx_job && s_g4.level >= 6) {
                        gx_finish();
                    } else {
                        s_g4_lit = 0xFF;
                        s_gx_next_ms = now_ms() + 500;
                    }
                } else {   // DONE（7 全过）
                    gx_finish();
                }
            } else {
                pet_audio_play(SND_SAD);
                gx_finish();
            }
        }
        break;
    case PT_GAME_G5_WALK:
        if (btn == PET_BTN_UP || btn == PET_BTN_DOWN) {
            uint8_t ok = pet_g5_act(&s_g5,
                                    btn == PET_BTN_UP ? PET_G5_UP : PET_G5_DOWN);
            pet_audio_play(ok ? SND_HAPPY : SND_SAD);
            if ((s_gx_job && s_g5.next_obs >= 4) || pet_g5_done(&s_g5)) {
                gx_finish();
            } else {
                gx_render_g5();
            }
        }
        break;
    case PT_GAME_G6_PREFER:
        if (btn == PET_BTN_UP) {
            s_gx_sel = (s_gx_sel + 2) % 3;
            gx_render_g6();
        } else if (btn == PET_BTN_DOWN) {
            s_gx_sel = (s_gx_sel + 1) % 3;
            gx_render_g6();
        } else if (btn == PET_BTN_OK) {
            bool ok = pet_g6_answer(&s_g6, (pet_g6_choice_t) s_gx_sel);
            pet_audio_play(ok ? SND_HAPPY : SND_SAD);
            if (pet_g6_done(&s_g6)) {
                gx_finish();
            } else {
                gx_render_g6();
            }
        }
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// 成年职业：办公室 + 职介所（07 §5）
// ---------------------------------------------------------------------------

// PV2 职介所第二行：全大写（无下伸部，不压双行卡底边），ASCII '-' 分隔。
static void job_req_text(const pt_job_def_t *d, char *buf, size_t n)
{
    if (d->always) {
        snprintf(buf, n, "NO REQUIREMENT");
        return;
    }
    int len = 0;
    if (d->need_mind) {
        len += snprintf(buf + len, n - (size_t) len, "MIND %d", d->need_mind);
    }
    if (d->need_body) {
        len += snprintf(buf + len, n - (size_t) len, "%sBODY %d",
                        len ? " - " : "", d->need_body);
    }
    if (d->need_art) {
        snprintf(buf + len, n - (size_t) len, "%sART %d",
                 len ? " - " : "", d->need_art);
    }
}

// PV2 办公室（冻结稿 pv2-job 帧①-③）：hero 卡 + 班次珠/工资两行 + 两张动作卡。
static void job_office_build(void)
{
    lv_obj_clean(s_modal_body);
    frame_pv2(false);
    lv_label_set_text(s_modal_title, "WORK");
    const pt_job_def_t *d = pt_job_def((pt_job_id_t) s_jobs.job);
    char buf[24];

    // hero：职业名 + 绑定小游戏；劳模贴纸周在卡内右上（无贴纸时不留跳动）。
    lv_obj_t *hero = rect(s_modal_body, 0, 0, 192, 42, 10, COL_CARD_WHITE);
    border(hero, COL_HAIR, 1);
    llabel(hero, 12, 4, 120, &lv_font_montserrat_14, COL_INK, d->name);
    snprintf(buf, sizeof(buf), "%s mini-game", GAMES_NAME[d->game]);
    llabel(hero, 12, 24, 120, &lv_font_montserrat_12, COL_SUB, buf);
    if (s_jobs.worker_sticker) {
        // 劳模徽章：金星 + "Model"（实测 14pt 最长职业名 Weathercast 止于 x104、
        // 12pt 最长副标题大写顶约 x127 且顶点 y27.6，故徽章右钉、宽按实测、y9）。
        const char *mwtxt = "Model";
        int32_t mtw = text_w12(mwtxt);
        int32_t mpw = mtw + 17;   // 星点 5 + 间距 + 两侧内边距
        int32_t mpx = 192 - 2 - mpw;
        lv_obj_t *mw = rect(s_modal_body, mpx, 9, mpw, 16, 8,
                            COL_CARD_GREENBG);
        border(mw, COL_GREEN, 1);
        rect(s_modal_body, mpx + 5, 15, 5, 5, 2, COL_YELLOW);
        llabel(s_modal_body, mpx + 13, 11, 0, &lv_font_montserrat_12,
               COL_GREEN, mwtxt);
    }

    // 信息两行：班次珠（0..2）与三档工资金丸。
    // "Shifts today" 实测宽 78.4（止于 x90），班次珠自 x96 起。
    llabel(s_modal_body, 12, 51, 90, &lv_font_montserrat_12, COL_INK,
           "Shifts today");
    for (uint8_t i = 0; i < PT_JOB_SHIFTS_PER_DAY; i += 1) {
        rect(s_modal_body, 96 + i * 18, 56, 14, 6, 3,
             i < s_jobs.shifts_today ? COL_TAB_ON : COL_PIP_OFF);
    }
    llabel(s_modal_body, 12, 69, 40, &lv_font_montserrat_12, COL_SUB,
           "Wage");
    snprintf(buf, sizeof(buf), "%u/%u/%u G",
             (unsigned) d->wage[0], (unsigned) d->wage[1],
             (unsigned) d->wage[2]);
    int32_t pw = text_w12(buf) + 12;
    lv_obj_t *gp = rect(s_modal_body, 184 - pw, 66, pw, 16, 8, COL_COIN_BG);
    border(gp, COL_YELLOW, 1);
    label(gp, 0, 2, pw, &lv_font_montserrat_12, COL_BROWN, buf);

    // 两张动作卡：不可开工时 Work shift 整卡置灰，右缘显实际班次计数。
    bool can = pt_jobs_can_work(&s_jobs) && !s_snap.sick
        && (s_snap.stage == PT_STAGE_ADULT
            || s_snap.stage == PT_STAGE_SENIOR)
        && s_snap.energy >= PT_CFG_JOB_MIN_ENERGY;
    s_job_row[0] = rect(s_modal_body, 0, 94, 192, 28, 10,
                        can ? COL_CARD_WHITE : COL_DIS_BG);
    llabel(s_job_row[0], 12, 6, 100, &lv_font_montserrat_12,
           can ? COL_INK : COL_DIS_TX, "Work shift");
    if (can) {
        rlabel12(s_job_row[0], 180, 6, COL_SEL, "Start");
    } else {
        char cnt[6];
        snprintf(cnt, sizeof(cnt), "%u/3",
                 (unsigned) s_jobs.shifts_today);
        rlabel12(s_job_row[0], 180, 6, COL_DIS_TX, cnt);
    }
    s_job_row[1] = rect(s_modal_body, 0, 126, 192, 28, 10, COL_CARD_WHITE);
    llabel(s_job_row[1], 12, 6, 100, &lv_font_montserrat_12, COL_INK,
           "Job agency");
    rlabel12(s_job_row[1], 180, 6, COL_BLUE, "Go");
    for (uint8_t i = 0; i < 2; i += 1) {
        border(s_job_row[i], i == (uint8_t) s_job_sel ? COL_SEL : COL_HAIR,
               i == (uint8_t) s_job_sel ? 2 : 1);
    }
}

static void job_office_open(void)
{
    if (s_snap.stage != PT_STAGE_ADULT && s_snap.stage != PT_STAGE_SENIOR) {
        set_msg("Grow up first", 1200);
        pet_audio_play(SND_CANCEL);
        return;
    }
    (void) pet_app_jobs_snapshot(&s_jobs);
    s_job_sel = 0;
    s_mode = MODE_JOB;
    modal_open();
    job_office_build();
    pet_audio_play(SND_CONFIRM);
}

// 开工：按当前职业绑定的玩法开缩短版短关；G1 沿用旧界面，其余复用 GX 运行时。
static void job_start_shift(void)
{
    if (!pt_jobs_can_work(&s_jobs)) {
        set_msg("3 shifts done today", 1200);
        pet_audio_play(SND_CANCEL);
        return;
    }
    if (s_snap.sick || s_snap.energy < PT_CFG_JOB_MIN_ENERGY) {
        set_msg(s_snap.sick ? "Not feeling well" : "Too tired to work", 1200);
        pet_audio_play(SND_SAD);
        return;
    }
    const pt_job_def_t *d = pt_job_def((pt_job_id_t) s_jobs.job);
    if (d->game == PT_GAME_G1_HILO) {
        s_g1_job = true;
        game_open();
    } else {
        s_gx_job = true;
        gx_open(d->game);
        s_gx_job = true;   // gx_open 内部会复位，打开后再置位
    }
}

// PV2 职介所（冻结稿 pv2-job 帧④⑤）：双行卡 192x30、步进 31，窗口 5 行随焦点滚动。
#define JOB_AG_WIN 5
static void agency_build(void)
{
    lv_obj_clean(s_modal_body);
    frame_pv2(false);
    lv_label_set_text(s_modal_title, "AGENCY");
    s_agency_n = pt_jobs_list(s_snap.skill, s_agency_ids, PT_JOB_COUNT);
    if ((uint8_t) s_agency_sel >= s_agency_n) {
        s_agency_sel = 0;
    }
    // 滚动窗口：焦点落到第 6 行起窗口跟随；到底钳制。
    uint8_t win = 0;
    if (s_agency_n > JOB_AG_WIN) {
        if ((uint8_t) s_agency_sel >= JOB_AG_WIN) {
            win = (uint8_t) s_agency_sel - (JOB_AG_WIN - 1);
        }
        uint8_t maxwin = s_agency_n - JOB_AG_WIN;
        if (win > maxwin) {
            win = maxwin;
        }
    }
    uint8_t shown = s_agency_n - win;
    if (shown > JOB_AG_WIN) {
        shown = JOB_AG_WIN;
    }

    for (uint8_t k = 0; k < shown; k += 1) {
        uint8_t idx = win + k;
        const pt_job_def_t *d = pt_job_def(s_agency_ids[idx]);
        bool on = idx == (uint8_t) s_agency_sel;
        lv_obj_t *r = rect(s_modal_body, 0, (int32_t) k * 31, 192, 30,
                           8, COL_CARD_WHITE);
        border(r, on ? COL_SEL : COL_HAIR, on ? 2 : 1);
        llabel(r, 10, 1, 104, &lv_font_montserrat_12, COL_INK, d->name);
        if (s_agency_ids[idx] == s_jobs.job) {
            rlabel12(r, 180, 1, COL_GREEN, "NOW");
        } else {
            char wbuf[20];
            snprintf(wbuf, sizeof(wbuf), "%u/%u/%u G",
                     (unsigned) d->wage[0], (unsigned) d->wage[1],
                     (unsigned) d->wage[2]);
            int32_t pw = text_w12(wbuf) + 12;
            lv_obj_t *p = rect(r, 184 - pw, 3, pw, 14, 7, COL_COIN_BG);
            border(p, COL_YELLOW, 1);
            label(p, 0, 1, pw, &lv_font_montserrat_12, COL_BROWN, wbuf);
        }
        char req[32];
        job_req_text(d, req, sizeof(req));
        llabel(r, 10, 14, 172, &lv_font_montserrat_12, COL_SUB, req);
    }
}

static void agency_open(void)
{
    s_agency_sel = 0;
    s_mode = MODE_AGENCY;
    agency_build();
    pet_audio_play(SND_CONFIRM);
}

static void handle_job_key(pet_ui_action_t act)
{
    if (act == PET_UI_ACT_PREV) {
        s_job_sel = (s_job_sel + 1) % 2;
        job_office_build();
    } else if (act == PET_UI_ACT_NEXT) {
        s_job_sel = (s_job_sel + 1) % 2;
        job_office_build();
    } else if (act == PET_UI_ACT_CONFIRM) {
        if (s_job_sel == 0) {
            job_start_shift();
        } else {
            agency_open();
        }
    } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        pet_audio_play(SND_CANCEL);
        modal_close();
    }
}

static void handle_agency_key(pet_ui_action_t act)
{
    if (act == PET_UI_ACT_PREV) {
        s_agency_sel = (int8_t) ((s_agency_sel + s_agency_n - 1)
                                 % s_agency_n);
        agency_build();
    } else if (act == PET_UI_ACT_NEXT) {
        s_agency_sel = (int8_t) ((s_agency_sel + 1) % s_agency_n);
        agency_build();
    } else if (act == PET_UI_ACT_CONFIRM) {
        pt_job_id_t pick = s_agency_ids[s_agency_sel];
        if (pick == s_jobs.job) {
            set_msg("Already working here", 1100);
            pet_audio_play(SND_CANCEL);
            return;
        }
        if (!pt_jobs_can_switch(&s_jobs, s_snap.day_id)) {
            set_msg("Try again next month", 1300);
            pet_audio_play(SND_CANCEL);
            return;
        }
        pet_app_job_switch((uint8_t) pick);
        s_jobs.job = (uint8_t) pick;   // 乐观镜像（裁决条件与本地一致）
        s_jobs.switch_period = (uint16_t) (s_snap.day_id
                                           / PT_JOB_SWITCH_PERIOD_D);
        set_msg("New job!", 1200);
        pet_audio_play(SND_HAPPY);
        modal_close();
    } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        pet_audio_play(SND_CANCEL);
        s_mode = MODE_JOB;
        job_office_build();
    }
}

// ---------------------------------------------------------------------------
// 全屏演出与死亡纪念
// ---------------------------------------------------------------------------

// PV2 batch9 全屏演出几何（屏坐标，定稿 pv2-overlay.html）。
#define EFF_CX          120
#define EFF_RING_CY     112   // 悬念双环中心
#define EFF_BURST_CY    114   // 爆开中心（对齐宠体视觉中心）
#define EFF_RING_OUT_R  44
#define EFF_RING_IN_R   26
#define EFF_BURST_R0    26
#define EFF_BURST_R1    48

static void eff_border_opa_cb(lv_obj_t *o, int32_t v)
{
    lv_obj_set_style_border_opa(o, (lv_opa_t) v, 0);
}

static void eff_bg_opa_cb(lv_obj_t *o, int32_t v)
{
    lv_obj_set_style_bg_opa(o, (lv_opa_t) v, 0);
}

// 呼吸：描边不透明度在 lo/hi 间无限往返。
static void eff_breathe(lv_obj_t *o, int32_t lo, int32_t hi, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, lo, hi);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_playback_duration(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) eff_border_opa_cb);
    lv_anim_start(&a);
}

// 爆环：v 0..256，半径 26->48 扩散，不透明度 200->0。
static void eff_burst_cb(lv_obj_t *o, int32_t v)
{
    int32_t r = EFF_BURST_R0
              + (EFF_BURST_R1 - EFF_BURST_R0) * v / 256;
    lv_obj_set_size(o, r * 2, r * 2);
    lv_obj_set_pos(o, EFF_CX - r, EFF_BURST_CY - r);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_opa(o,
        (lv_opa_t) (200 * (256 - v) / 256), 0);
}

static void eff_burst_play(void)
{
    static const int16_t sp_x[4] = { 78, 156, 82, 152 };
    static const int16_t sp_y[4] = { 76, 76, 150, 150 };

    lv_obj_set_size(s_eff_burst, EFF_BURST_R0 * 2, EFF_BURST_R0 * 2);
    lv_obj_set_pos(s_eff_burst, EFF_CX - EFF_BURST_R0,
                   EFF_BURST_CY - EFF_BURST_R0);
    lv_obj_set_style_radius(s_eff_burst, EFF_BURST_R0, 0);
    lv_obj_set_style_border_opa(s_eff_burst, (lv_opa_t) 200, 0);
    show(s_eff_burst);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_eff_burst);
    lv_anim_set_values(&a, 0, 256);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) eff_burst_cb);
    lv_anim_start(&a);

    for (int i = 0; i < 4; i += 1) {
        lv_obj_t *sp = s_eff_spark[i];
        lv_obj_set_pos(sp, 117, 111);   // 6x6 居中起
        lv_obj_set_style_bg_opa(sp, LV_OPA_COVER, 0);
        show(sp);

        lv_anim_t ax;
        lv_anim_init(&ax);
        lv_anim_set_var(&ax, sp);
        lv_anim_set_duration(&ax, 500);
        lv_anim_set_path_cb(&ax, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&ax, (lv_anim_exec_xcb_t) lv_obj_set_x);
        lv_anim_set_values(&ax, 117, sp_x[i]);
        lv_anim_start(&ax);

        lv_anim_t ay;
        lv_anim_init(&ay);
        lv_anim_set_var(&ay, sp);
        lv_anim_set_duration(&ay, 500);
        lv_anim_set_path_cb(&ay, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&ay, (lv_anim_exec_xcb_t) lv_obj_set_y);
        lv_anim_set_values(&ay, 111, sp_y[i]);
        lv_anim_start(&ay);

        lv_anim_t ao;
        lv_anim_init(&ao);
        lv_anim_set_var(&ao, sp);
        lv_anim_set_values(&ao, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_duration(&ao, 500);
        lv_anim_set_path_cb(&ao, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&ao, (lv_anim_exec_xcb_t) eff_bg_opa_cb);
        lv_anim_start(&ao);
    }
}

static void eff_start(int kind)
{
    s_eff_kind = kind;
    s_eff_t0 = now_ms();
    s_eff_rebuilt = false;
    s_mode = MODE_OVERLAY;
    // 演出全屏接管：停掉蛋的持续摆动；reveal 时 build_creature 会按新阶段重建。
    lv_anim_delete(s_creature, NULL);
    // 孵化/进化可能在喂食等弹层开着时发生：弹层让位，避免演出结束后残留。
    hide(s_modal);

    // 复位上轮揭晓件。
    lv_anim_delete(s_eff_burst, NULL);
    hide(s_eff_burst);
    for (int i = 0; i < 4; i += 1) {
        lv_anim_delete(s_eff_spark[i], NULL);
        hide(s_eff_spark[i]);
    }

    // 悬念件：粉 !/HATCHING 或金 ?/EVOLVING，双环呼吸重启。
    bool hatch = kind == EFF_HATCH;
    lv_label_set_text(s_eff_glyph, hatch ? "!" : "?");
    lv_obj_set_style_text_color(s_eff_glyph,
        lv_color_hex(hatch ? COL_SEL : COL_YELLOW), 0);
    lv_label_set_text(s_eff_word, hatch ? "HATCHING" : "EVOLVING");
    lv_anim_delete(s_eff_ring_out, NULL);
    lv_anim_delete(s_eff_ring_in, NULL);
    lv_obj_set_style_border_opa(s_eff_ring_out, (lv_opa_t) 76, 0);
    lv_obj_set_style_border_opa(s_eff_ring_in, (lv_opa_t) 204, 0);
    show(s_eff_ring_out);
    show(s_eff_ring_in);
    show(s_eff_glyph);
    show(s_eff_word);
    eff_breathe(s_eff_ring_out, 76, 204, 700);
    eff_breathe(s_eff_ring_in, 204, 128, 700);

    show(s_eff_cont);
    lv_obj_move_foreground(s_eff_cont);
    lv_obj_set_style_bg_opa(s_eff_cont, LV_OPA_COVER, 0);
}

static void memorial_open(void)
{
    lv_label_set_text(s_memorial_name, species_name(s_snap.species));
    char buf[48];
    snprintf(buf, sizeof(buf), "Age %d days", (int) s_snap.age_days);
    lv_label_set_text(s_memorial_line, buf);
    // 遗物占位：终养评价（designs 13 P0-C；正式遗物系统随世代期上线）。
    pt_care_tier_t tier = pt_care_tier(&s_snap.ledger, false);
    const char *word;
    switch (tier) {
    case PT_CARE_PERFECT: word = "A beloved friend"; break;
    case PT_CARE_GREAT:   word = "A cherished friend"; break;
    case PT_CARE_NORMAL:  word = "A steady friend"; break;
    default:              word = "Remember gently"; break;
    }
    lv_label_set_text(s_memorial_tier, word);
    show(s_memorial);
    lv_obj_move_foreground(s_memorial);
    s_mode = MODE_MEMORIAL;
}

// ---------------------------------------------------------------------------
// 事件处理
// ---------------------------------------------------------------------------

static void on_app_event(const pt_event_t *ev)
{
    switch (ev->kind) {
    case PT_EV_HATCH:
        eff_start(EFF_HATCH);
        break;
    case PT_EV_EVOLUTION:
        eff_start(EFF_EVOLVE);
        break;
    case PT_EV_DEATH:
        memorial_open();
        break;
    case PT_EV_CALL_RAISED: {
        pet_dock_focus_call(&s_dock, (pt_call_kind_t) ev->a);
        dock_refresh_selected();
        if (ev->a == PT_CALL_HUNGRY) {
            set_msg("Hungry!", 1500);
        } else if (ev->a == PT_CALL_SAD) {
            set_msg("Wants attention", 1500);
        } else {
            set_msg("Lights out?", 1500);
        }
        lv_obj_t *box = s_dock_w[pet_dock_selected(&s_dock)].box;
        if (box != NULL) {
            bounce(box, DOCK_TILE_Y, -6, 180);
        }
        break;
    }
    case PT_EV_SICK:
        set_msg("Not feeling well", 1600);
        dock_build_widgets();
        break;
    case PT_EV_CURED:
        set_msg("Feeling better!", 1400);
        s_mood = MOOD_HAPPY;
        s_mood_until = now_ms() + 900;
        break;
    case PT_EV_MEDICINE_FAILED:
        set_msg("Medicine didn't work", 1400);
        break;
    case PT_EV_CALL_EXPIRED:
    case PT_EV_MISTAKE_SMALL:
    case PT_EV_MISTAKE_BIG:
        set_msg("Oops...", 1200);
        break;
    case PT_EV_REFUSED:
        if (ev->a == PT_INTENT_GAME_RESULT) {
            set_msg("Too sick to play", 1200);
        } else {
            set_msg("It refuses", 1000);
        }
        break;
    case PT_EV_POOP:
        set_msg("...", 800);
        break;
    case PT_EV_SOC_CANDIDATE:
        set_msg("A visitor came by!", 1800);
        if (s_dock_w[PET_ICON_MATE].box != NULL) {
            bounce(s_dock_w[PET_ICON_MATE].box, DOCK_TILE_Y, -6, 180);
        }
        break;
    case PT_EV_SOC_WEDDING:
        set_msg("Just married!", 2200);
        s_mood = MOOD_HAPPY;
        s_mood_until = now_ms() + 2000;
        bounce(s_creature, CREATURE_Y, -12, 300);
        break;
    case PT_EV_SOC_EGG_READY:
        set_msg("An egg is on the way!", 2200);
        s_mood = MOOD_HAPPY;
        s_mood_until = now_ms() + 2000;
        break;
    case PT_EV_SOC_PARENTS_LEAVE:
        set_msg("Your parents set off traveling", 2400);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// 主屏动作
// ---------------------------------------------------------------------------

static void toggle_lights(void)
{
    if (s_snap.stage < PT_STAGE_BABY) {
        return;
    }
    if (s_snap.lights_off) {
        pet_app_send_intent(PT_INTENT_LIGHTS_ON, 0);
        set_msg("Lights on", 900);
    } else {
        pet_app_send_intent(PT_INTENT_LIGHTS_OFF, 0);
        set_msg("Lights off", 900);
    }
    pet_audio_play(SND_CONFIRM);
}

// PV2 batch2：打开 STATUS，焦点落在当前页（首页 INFO）段。
static void status_open(void)
{
    s_status_page = 0;
    s_status_focus = 1;
    s_mode = MODE_STATUS;
    modal_open();
    status_build();
}

static void activate_icon(pet_icon_t icon)
{
    switch (icon) {
    case PET_ICON_FEED:
        s_list_kind = LIST_FOOD;
        s_list_sel = 0;
        s_mode = MODE_LIST;
        modal_open();
        food_build();
        pet_audio_play(SND_CONFIRM);
        break;
    case PET_ICON_CLEAN:
        if (s_snap.poops > 0) {
            pet_app_send_intent(PT_INTENT_CLEAN, 0);
            pet_audio_play(SND_CLEAN);
            set_msg("Clean!", 900);
            bounce(s_creature, CREATURE_Y, -10, 220);
        } else {
            set_msg("Already clean", 900);
        }
        break;
    case PET_ICON_STATUS:
        status_open();
        pet_audio_play(SND_CONFIRM);
        break;
    case PET_ICON_LIGHTS:
        toggle_lights();
        break;
    case PET_ICON_MED:
        if (s_snap.sick) {
            pet_app_send_intent(PT_INTENT_MEDICINE, 0);
            pet_audio_play(SND_CONFIRM);
            set_msg("Here you go", 900);
        } else {
            set_msg("Healthy", 900);
        }
        break;
    case PET_ICON_GAME:
        games_open();
        break;
    case PET_ICON_JOB:
        job_office_open();
        break;
    case PET_ICON_MATE:
        mate_open();
        break;
    case PET_ICON_DEX:
        dex_open();
        break;
    case PET_ICON_PAT:
        pet_app_send_intent(PT_INTENT_PAT, 0);
        pet_audio_play(SND_CONFIRM);
        s_mood = MOOD_HAPPY;
        s_mood_until = now_ms() + 800;
        bounce(s_creature, CREATURE_Y, -12, 260);
        break;
    case PET_ICON_SHOP:
        shop_open();
        break;
    case PET_ICON_SETTINGS:
        s_list_kind = LIST_SETTINGS;
        s_sound_page = 0;
        s_list_sel = pet_app_volume();
        s_mode = MODE_LIST;
        modal_open();
        sound_build();
        pet_audio_play(SND_CONFIRM);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// 按键分发（已持有 LVGL 锁）
// ---------------------------------------------------------------------------

// 组合键状态（designs 10 §1）：UP+OK 静音切换、DOWN+OK 状态摘要。
// 三键同按 2 秒的度假模式依赖 P3 日历系统，P0 不接线。
static bool s_up_held;
static bool s_down_held;
static bool s_up_chorded;
static bool s_down_chorded;
static int64_t s_up_chord_at;   // 和弦触发时刻(lv_tick ms)，用于过期残留 latch
static int64_t s_down_chord_at;
static uint8_t s_prev_volume = 2;

static void open_status_summary(void)
{
    status_open();
}

static void toggle_mute_chord(void)
{
    uint8_t vol = pet_app_volume();
    if (vol == 0) {
        pet_app_set_volume(s_prev_volume == 0 ? 2 : s_prev_volume);
        set_msg("Sound on", 900);
    } else {
        s_prev_volume = vol;
        pet_app_set_volume(0);
        set_msg("Muted", 900);
    }
}

static void handle_room(pet_ui_action_t act)
{
    switch (act) {
    case PET_UI_ACT_PREV:
        pet_dock_move(&s_dock, -1);
        dock_refresh_selected();
        break;
    case PET_UI_ACT_NEXT:
        pet_dock_move(&s_dock, 1);
        dock_refresh_selected();
        break;
    case PET_UI_ACT_CONFIRM:
        // 主屏短按 OK 先判组合键：UP+OK 静音 / DOWN+OK 状态摘要。
        // 触发后立即清 held：共享 ADC 分压在和弦时会给方向键制造一次额外通断，
        // 当这段"二次按住"长于短按窗(180ms)时 iot_button 会吞掉方向键终态点击
        // （走 PRESS_END 而非 SINGLE/DOUBLE），若靠终态点击清 held 会永久卡死。
        if (s_up_held) {
            s_up_held = false;
            s_up_chorded = true;
            s_up_chord_at = now_ms();
            toggle_mute_chord();
            return;
        }
        if (s_down_held) {
            s_down_held = false;
            s_down_chorded = true;
            s_down_chord_at = now_ms();
            open_status_summary();
            pet_audio_play(SND_CONFIRM);
            return;
        }
        activate_icon(pet_dock_selected(&s_dock));
        break;
    case PET_UI_ACT_LIGHTS:
        toggle_lights();
        break;
    case PET_UI_ACT_MENU:
        status_open();
        break;
    default:
        break;
    }
}

static void handle_list_key(pet_ui_action_t act)
{
    if (s_list_kind == LIST_FOOD) {
        // PV2 卡片焦点：UP/DOWN 只改样式不重建。
        if (act == PET_UI_ACT_PREV) {
            s_list_sel = (s_list_sel + s_focus_n - 1) % s_focus_n;
            pv2_mark((uint8_t) s_list_sel, false);
        } else if (act == PET_UI_ACT_NEXT) {
            s_list_sel = (s_list_sel + 1) % s_focus_n;
            pv2_mark((uint8_t) s_list_sel, false);
        } else if (act == PET_UI_ACT_CONFIRM) {
            pt_intent_kind_t intent;
            if (s_snap.stage == PT_STAGE_BABY) {
                intent = PT_INTENT_FEED_BOTTLE;
            } else if (s_list_sel == 0) {
                intent = PT_INTENT_FEED_MEAL;
            } else {
                intent = PT_INTENT_FEED_SNACK;
            }
            pet_app_send_intent(intent, 0);
            pet_audio_play(SND_EAT);
            s_mood = MOOD_EAT;
            s_mood_until = now_ms() + 800;
            bounce(s_creature, CREATURE_Y, -10, 220);
            modal_close();
        } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
            pet_audio_play(SND_CANCEL);
            modal_close();
        }
        return;
    }

    // PV2 batch2：SOUND 卡片弹层（音量四档页 + 免打扰两档页）。
    uint8_t slots = s_focus_n;
    if (act == PET_UI_ACT_PREV) {
        s_list_sel = (s_list_sel + slots - 1) % slots;
        pv2_mark((uint8_t) s_list_sel, true);
    } else if (act == PET_UI_ACT_NEXT) {
        s_list_sel = (s_list_sel + 1) % slots;
        pv2_mark((uint8_t) s_list_sel, true);
    } else if (act == PET_UI_ACT_CONFIRM) {
        uint8_t kind = s_focus_kind[s_list_sel];
        uint8_t param = s_focus_param[s_list_sel];
        if (kind == PV2_F_CARD) {
            if (s_sound_page == 0) {
                pet_app_set_volume(param);
            } else {
                pet_app_set_quiet(param == 0);
            }
            pet_audio_play(SND_CONFIRM);
            sound_build();      // 绿勾移到新当前值
        } else if (kind == PV2_F_PREV) {
            if (s_sound_page > 0) {
                s_sound_page -= 1;
                s_list_sel = 0;
                sound_build();
                pet_audio_play(SND_CONFIRM);
            }
        } else if (kind == PV2_F_NEXT) {
            if (s_sound_page + 1 < 2) {
                s_sound_page += 1;
                s_list_sel = 0;
                sound_build();
                pet_audio_play(SND_CONFIRM);
            }
        }
    } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        pet_audio_play(SND_CANCEL);
        modal_close();
    }
}

static void status_goto(int page)
{
    s_status_page = page;
    s_status_focus = page + 1;
    status_build();
    pet_audio_play(SND_CONFIRM);
}

static void handle_status_key(pet_ui_action_t act)
{
    if (act == PET_UI_ACT_PREV) {
        s_status_focus = (s_status_focus + 4) % 5;
        status_footer_refresh();
    } else if (act == PET_UI_ACT_NEXT) {
        s_status_focus = (s_status_focus + 1) % 5;
        status_footer_refresh();
    } else if (act == PET_UI_ACT_CONFIRM) {
        if (s_status_focus == 0) {
            if (s_status_page > 0) {
                status_goto(s_status_page - 1);
            }
        } else if (s_status_focus == 4) {
            if (s_status_page < 2) {
                status_goto(s_status_page + 1);
            }
        } else {
            int page = s_status_focus - 1;
            if (page != s_status_page) {
                status_goto(page);
            }
        }
    } else if (act == PET_UI_ACT_BACK || act == PET_UI_ACT_MENU) {
        modal_close();
    }
}

static void handle_key_locked(pet_btn_t btn, pet_ev_t ev)
{
    if (s_mode == MODE_MEMORIAL) {
        if (btn == PET_BTN_OK && ev == PET_EV_CLICK) {
            hide(s_memorial);
            s_mode = MODE_ROOM;
            s_have_prev = false;
            s_checkin_prompted = false;
            pet_app_new_game();
            set_msg("A new egg...", 1500);
            pet_audio_play(SND_CONFIRM);
        }
        return;
    }
    if (s_mode == MODE_OVERLAY || s_eff_kind != EFF_NONE) {
        return;   // 进化/孵化演出全程锁输入（揭晓后房间已刷新但 FX 未散）
    }
    if (s_mode == MODE_GAME) {
        if (ev == PET_EV_LONG && btn == PET_BTN_OK) {
            // 二次长按确认退出，防误触。
            if (now_ms() < s_g_exit_armed_until) {
                game_exit();
            } else {
                s_g_exit_armed_until = now_ms() + 2000;
                lv_label_set_text(s_game_hint, "Hold OK again to exit");
            }
            return;
        }
        if (ev == PET_EV_CLICK) {
            game_key_click(btn);
        }
        return;
    }
    if (s_mode == MODE_GX) {
        if (ev == PET_EV_LONG && btn == PET_BTN_OK) {
            // 中途退出无任何收益（07 §2）；二次长按防误触。
            if (now_ms() < s_gx_exit_armed_until) {
                pet_audio_play(SND_CANCEL);
                modal_close();
            } else {
                s_gx_exit_armed_until = now_ms() + 2000;
                lv_label_set_text(s_gx_hint, "Hold OK again to exit");
                show(s_gx_hint);
            }
            return;
        }
        if (ev == PET_EV_CLICK) {
            gx_click(btn);
        }
        return;
    }

    bool modal_open_flag = (s_mode == MODE_LIST || s_mode == MODE_STATUS
                            || s_mode == MODE_SHOP || s_mode == MODE_DECOR
                            || s_mode == MODE_CHECKIN
                            || s_mode == MODE_GAMES
                            || s_mode == MODE_JOB || s_mode == MODE_AGENCY
                            || s_mode == MODE_MATE || s_mode == MODE_DEX);
    pet_ui_action_t act = pet_input_map(btn, ev, modal_open_flag);
    if (act == PET_UI_ACT_NONE) {
        return;
    }

    switch (s_mode) {
    case MODE_ROOM:
        handle_room(act);
        break;
    case MODE_LIST:
        handle_list_key(act);
        break;
    case MODE_STATUS:
        handle_status_key(act);
        break;
    case MODE_SHOP:
        handle_shop_key(act);
        break;
    case MODE_DECOR:
        handle_style_key(act);
        break;
    case MODE_CHECKIN:
        handle_checkin_key(act);
        break;
    case MODE_GAMES:
        handle_games_key(act);
        break;
    case MODE_JOB:
        handle_job_key(act);
        break;
    case MODE_AGENCY:
        handle_agency_key(act);
        break;
    case MODE_MATE:
        handle_mate_key(act);
        break;
    case MODE_DEX:
        handle_dex_key(act);
        break;
    default:
        break;
    }
}

void pet_ui_button(pet_btn_t btn, pet_ev_t ev)
{
    // 组合键：记录方向键按住状态（designs 10 §1）。
    // 和弦触发后 held 已立即清掉；chorded latch 只用来吞掉和弦瞬间方向键可能产生
    // 的一次残留点击（约 300ms 内到达）。若那次终态事件被 iot_button 吞掉，latch 会
    // 残留——靠这里：800ms 之后的新按压属于新手势，解除旧 latch，避免误吞一次导航。
    if (ev == PET_EV_PRESS) {
        const int64_t now = now_ms();
        if (btn == PET_BTN_UP) {
            if (s_up_chorded && now - s_up_chord_at > 800) s_up_chorded = false;
            s_up_held = true;
        } else if (btn == PET_BTN_DOWN) {
            if (s_down_chorded && now - s_down_chord_at > 800) s_down_chorded = false;
            s_down_held = true;
        }
        return;
    }
    if (btn == PET_BTN_UP) {
        bool chorded = s_up_chorded;
        s_up_held = false;
        s_up_chorded = false;
        if (chorded) {
            return;   // 和弦已消费，吞掉松手时的 CLICK
        }
    } else if (btn == PET_BTN_DOWN) {
        bool chorded = s_down_chorded;
        s_down_held = false;
        s_down_chorded = false;
        if (chorded) {
            return;
        }
    }
    if (bsp_lvgl_lock(250)) {
        handle_key_locked(btn, ev);
        bsp_lvgl_unlock();
    }
}

// ---------------------------------------------------------------------------
// 定时刷新（LVGL 任务语境，已持锁）
// ---------------------------------------------------------------------------

// 05 §5.3：按各槽动画帧重合成画布。RGB565 无 alpha，底填仍取当前墙色
// （夜间为夜墙色，见 canvas_wall565）；直接改外部缓冲后必须 lv_obj_invalidate
// 让 LVGL 重绘该区域。
static void recompose_pet_canvas(void)
{
    if (s_pet_canvas_obj == NULL) {
        return;
    }
    uint16_t wall565 = canvas_wall565();
    pt_compose_pet(&s_snap.genome, s_pose, wall565, s_pet_canvas_px,
                   &s_pet_art);
    lv_obj_invalidate(s_pet_canvas_obj);
}

// 帧号语义见 assets/pet/manifest.json frame_notes：
// body 0 静止 / 1 呼吸 / 2 跳跃；eyes 0 正常 / 1 眨眼 / 2 笑 / 3 睡 / 4 病；
// face 0 正常 / 1 吃 / 2 哭（哭帧留给后续社交期，本切片不触发）。
static void update_creature_mood(void)
{
    bool sleeping = s_snap.sleeping;
    int mood = s_mood;
    int64_t now = now_ms();
    if (now > s_mood_until) {
        mood = MOOD_NORMAL;
    }

    int want_row;
    if (sleeping) {
        want_row = 2;
    } else if (s_snap.sick) {
        want_row = 3;
    } else if (now < s_blink_until || mood == MOOD_HAPPY) {
        want_row = 1;
    } else {
        want_row = 0;
    }
    for (int row = 0; row < 4; row += 1) {
        for (int i = 0; i < 2; i += 1) {
            if (s_eye[row][i] != NULL) {
                if (row == want_row) {
                    show(s_eye[row][i]);
                } else {
                    hide(s_eye[row][i]);
                }
            }
        }
    }
    if (s_zz != NULL) {
        if (sleeping) {
            show(s_zz);
        } else {
            hide(s_zz);
        }
    }
    if (s_mouth[0] != NULL) {
        bool eat = !sleeping && mood == MOOD_EAT;
        if (eat) {
            hide(s_mouth[0]);
            show(s_mouth[1]);
        } else {
            show(s_mouth[0]);
            hide(s_mouth[1]);
        }
    }

    // 合成宠（非蛋/悼念页）：把同一套表情语义映射到各槽动画帧。
    if (s_pet_canvas_obj != NULL) {
        uint8_t want[PT_GENE_SLOT_COUNT];
        memset(want, 0, sizeof(want));
        if (sleeping) {
            want[PT_GENE_SLOT_EYES] = 3;
        } else if (s_snap.sick) {
            want[PT_GENE_SLOT_EYES] = 4;
        } else if (now < s_blink_until) {
            want[PT_GENE_SLOT_EYES] = 1;
        } else if (mood == MOOD_HAPPY) {
            want[PT_GENE_SLOT_EYES] = 2;
            want[PT_GENE_SLOT_BODY] = 2;
        } else if (mood == MOOD_EAT) {
            want[PT_GENE_SLOT_FACE] = 1;
        }
        // 待机呼吸：约每 3.6 秒有 0.4 秒身体 +1px（睡眠/跳跃/进食时不叠加）。
        if (!sleeping && want[PT_GENE_SLOT_BODY] == 0
            && (now % 3600) < 400) {
            want[PT_GENE_SLOT_BODY] = 1;
        }
        if (s_pose_dirty
            || memcmp(want, s_pose, sizeof(want)) != 0) {
            memcpy(s_pose, want, sizeof(want));
            s_pose_dirty = false;
            recompose_pet_canvas();
        }
    }
}

static void update_effect(void)
{
    int64_t elapsed = now_ms() - s_eff_t0;
    int32_t reveal = (s_eff_kind == EFF_HATCH) ? 350 : 1000;
    int32_t finish = (s_eff_kind == EFF_HATCH) ? 900 : 2100;

    if (!s_eff_rebuilt && elapsed >= reveal) {
        s_eff_rebuilt = true;
        // 揭晓即回到房间模式：本帧后续 refresh 会重建宠体/坞栏/顶栏并画胶囊，
        // 与爆开动画同框（旧白屏时代靠全屏遮挡，这些控件在 finish 前都是旧值）。
        s_mode = MODE_ROOM;
        build_creature();
        dock_build_widgets();
        dock_refresh_selected();
        bounce(s_creature, CREATURE_Y, -14, 320);
        lv_obj_set_style_bg_opa(s_eff_cont, LV_OPA_TRANSP, 0);
        // 悬念件退场，零位图爆开件上场。
        lv_anim_delete(s_eff_ring_out, NULL);
        lv_anim_delete(s_eff_ring_in, NULL);
        hide(s_eff_ring_out);
        hide(s_eff_ring_in);
        hide(s_eff_glyph);
        hide(s_eff_word);
        eff_burst_play();
        if (s_eff_kind == EFF_EVOLVE) {
            set_msg(species_name(s_snap.species), 1800);
        } else {
            set_msg("Hello!", 1200);
        }
    }
    if (elapsed >= finish) {
        hide(s_eff_cont);
        s_eff_kind = EFF_NONE;
        s_mode = MODE_ROOM;
    }
}

static void refresh(lv_timer_t *timer)
{
    (void) timer;
    int64_t t = now_ms();


    if (!pet_app_snapshot(&s_snap)) {
        return;
    }

    // 先吃引擎事件（演出/纪念可能改模式）。
    pt_event_t ev;
    while (pet_app_take_event(&ev)) {
        on_app_event(&ev);
    }

    if (!pet_app_econ_snapshot(&s_esnap)) {
        return;
    }
    // P2-S3a：社交面板打开时每帧跟随引擎裁决（关系值/扣款/相位变化后重建）。
    if (s_mode == MODE_MATE) {
        pt_social_t next;
        if (pet_app_social_snapshot(&next)) {
            pt_soc_phase_t old_phase = s_ssnap.phase;
            if (!s_have_ssnap || memcmp(&next, &s_ssnap, sizeof(next)) != 0) {
                s_ssnap = next;
                s_have_ssnap = true;
                // S3b：迎接新蛋执行成功（EGG_READY → SINGLE，主状态回蛋期）。
                if (old_phase == PT_SOC_EGG_READY
                    && next.phase == PT_SOC_SINGLE) {
                    modal_close();
                    set_msg("A new egg!", 1800);
                } else {
                    mate_build();
                }
            }
        }
    }
    {
        uint8_t rv;
        if (pet_app_take_soc_result(&rv)) {
            if (rv != PT_SOC_OK) {
                set_msg(mate_soc_msg(rv), 1200);
            }
        }
    }
    // 购买/使用后引擎任务已落账，下一帧重建商店行（钱包/库存/次数）。
    if (s_mode == MODE_SHOP && s_shop_dirty) {
        s_shop_dirty = false;
        shop_build();
    }
    if (!pet_app_decor_snapshot(&s_dsnap)) {
        return;
    }
    // S5：月初津贴等到账提示。
    {
        char note[40];
        if (pet_app_take_notice(note, sizeof(note))) {
            set_msg(note, 1800);
        }
    }
    if (s_mode == MODE_DECOR && s_dec_dirty) {
        s_dec_dirty = false;
        style_build();
    }
    // 装饰变化 → 房间底色 / 家具标记 / 身上穿戴（收藏操作不常见，全量重建即可）。
    bool decor_changed = !s_have_dprev
                         || memcmp(&s_dprev, &s_dsnap, sizeof(s_dsnap)) != 0;
    if (decor_changed) {
        // 按键路径的立即重建可能早于引擎快照到账；快照真正变化后补一次，
        // 避免 STYLE 行停留在旧钱包/旧归属（历史：主题购买后行不刷新）。
        if (s_mode == MODE_DECOR) {
            style_build();
        }
        if (!s_have_dprev || s_dprev.theme != s_dsnap.theme) {
            apply_room_theme();
            // 合成画布以墙色为底，主题换色后必须重算；蛋期重建仅重开晃动动画，无害。
            build_creature();
        }
        if (!s_have_dprev || s_dprev.placed != s_dsnap.placed) {
            build_furniture();
        }
        if (!s_have_dprev
            || memcmp(s_dprev.worn, s_dsnap.worn, sizeof(s_dsnap.worn)) != 0) {
            build_creature();
        }
        s_dprev = s_dsnap;
        s_have_dprev = true;
    }


    // 时钟与电量
    int32_t mod = s_snap.minute % 1440;
    if (mod < 0) {
        mod += 1440;
    }
    char buf[40];
    snprintf(buf, sizeof(buf), "%02d:%02d", (int) (mod / 60), (int) (mod % 60));
    lv_label_set_text(s_clock, buf);

    s_battery_div += 1;
    if (s_battery_div >= 8) {
        s_battery_div = 0;
        s_battery_cache = bsp_battery_soc();
    }
    if (s_battery_cache < 0) {
        // 电量计不可用（模拟器无 CW2017）：隐藏电池文本，顶栏右侧留白。
        hide(s_battery);
    } else {
        show(s_battery);
        snprintf(buf, sizeof(buf), "%d%%", s_battery_cache);
        lv_label_set_text(s_battery, buf);
        lv_obj_set_style_text_color(s_battery,
            s_battery_cache < 20 ? lv_color_hex(COL_RED) : lv_color_hex(COL_INK), 0);
    }

    if (s_eff_kind != EFF_NONE) {
        update_effect();
    }
    if (s_mode == MODE_OVERLAY) {
        return;
    }
    if (s_mode == MODE_MEMORIAL) {
        return;
    }

    // 冷启动即处于死亡态：直接落纪念页。
    if (s_snap.stage == PT_STAGE_DEAD && s_have_prev) {
        memorial_open();
        return;
    }

    // 每日首次开机签到：进入幼儿期后每天自动弹一次；错过可在商店首页补领。
    // 演出揭晓后的残余 FX 窗口内不抢弹（s_eff_kind 尚未清）。
    if (!s_checkin_prompted && s_mode == MODE_ROOM
        && s_eff_kind == EFF_NONE
        && s_snap.stage >= PT_STAGE_CHILD
        && pt_econ_can_checkin(&s_esnap, s_snap.day_id)) {
        s_checkin_prompted = true;
        checkin_open();
    }

    bool stage_changed = !s_have_prev || s_prev.stage != s_snap.stage;
    bool species_changed = !s_have_prev || s_prev.species != s_snap.species;
    if (stage_changed || species_changed) {
        build_creature();
    }
    if (stage_changed) {
        dock_build_widgets();
    }

    // 顶栏阶段/年龄胶囊
    snprintf(buf, sizeof(buf), "%s D%d", stage_name(s_snap.stage),
             (int) s_snap.age_days);
    lv_label_set_text(s_top_stage, buf);

    // 四心计量：0..100 映射 0..4 心，阈值 12/37/62/87（每 25 一档）。
    // 仅在心数变化时换图，避免每次刷新都让 8 个图片对象失效重绘。
    static int8_t s_heart_full_cache[4] = { [0 ... 3] = -1 };
    static int8_t s_heart_fun_cache[4] = { [0 ... 3] = -1 };
    for (int i = 0; i < 4; i += 1) {
        int8_t f = s_snap.fullness >= 12 + i * 25 ? 1 : 0;
        int8_t h = s_snap.happiness >= 12 + i * 25 ? 1 : 0;
        if (s_heart_full_cache[i] != f) {
            s_heart_full_cache[i] = f;
            lv_image_set_src(s_heart_full[i], pet_ui_small_dsc(
                f ? PET_UI_SMALL_HEART_GREEN : PET_UI_SMALL_HEART_EMPTY));
        }
        if (s_heart_fun_cache[i] != h) {
            s_heart_fun_cache[i] = h;
            lv_image_set_src(s_heart_fun[i], pet_ui_small_dsc(
                h ? PET_UI_SMALL_HEART_YELLOW : PET_UI_SMALL_HEART_EMPTY));
        }
    }

    // 体重胶囊（生病时变红字 SICK；便便/呼叫/睡眠在房间内另有表现）
    if (s_snap.sick) {
        lv_label_set_text(s_weight_lbl, "SICK");
        lv_obj_set_style_text_color(s_weight_lbl, lv_color_hex(COL_RED), 0);
    } else {
        snprintf(buf, sizeof(buf), "%dg", (int) s_snap.weight);
        lv_label_set_text(s_weight_lbl, buf);
        lv_obj_set_style_text_color(s_weight_lbl, lv_color_hex(COL_INK), 0);
    }
    snprintf(buf, sizeof(buf), "%d", (int) s_snap.bond);
    lv_label_set_text(s_bond_lbl, buf);

    // 顶栏日月：关灯后为月亮
    lv_image_set_src(s_celestial, pet_ui_small_dsc(
        s_snap.lights_off ? PET_UI_SMALL_MOON : PET_UI_SMALL_SUN));

    // 便便
    if (!s_have_prev || s_prev.poops != s_snap.poops) {
        build_poops();
    }


    // 呼叫气泡：无限浮动动画只在气泡首次出现时建一次，不能每个刷新周期删了
    // 重建——那会让动画系统持续产生失效区。
    bool bubble_wanted = s_snap.active_call != PT_CALL_NONE && !s_snap.sleeping
                         && s_snap.stage != PT_STAGE_EGG;
    if (bubble_wanted) {
        lv_label_set_text(s_bubble_txt, call_text(s_snap.active_call));
        // 复位基准位（上次动画被删时 y 可能停在中途），再浮动。
        lv_obj_set_pos(s_bubble, 146, 36);
        show(s_bubble);
        if (!s_bubble_anim_on) {
            s_bubble_anim_on = true;
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, s_bubble);
            lv_anim_set_values(&a, 36, 31);
            lv_anim_set_duration(&a, 450);
            lv_anim_set_playback_duration(&a, 450);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) lv_obj_set_y);
            lv_anim_start(&a);
        }
    } else {
        if (s_bubble_anim_on) {
            s_bubble_anim_on = false;
            lv_anim_delete(s_bubble, NULL);
        }
        hide(s_bubble);
    }

    // 关灯后的夜间房间层
    if (s_snap.lights_off) {
        show(s_night);
    } else {
        hide(s_night);
    }
    // RGB565 画布无 alpha：灯光一切，画布底色要跟着换成夜墙色/主题墙色。
    if (s_pet_canvas_obj != NULL && s_night_canvas != s_snap.lights_off) {
        s_night_canvas = s_snap.lights_off;
        s_pose_dirty = true;   // 紧接着的 update_creature_mood 会重合成
    }

    update_creature_mood();
    dock_refresh_selected();


    // 眨眼节奏
    if (t >= s_next_blink) {
        s_blink_until = t + 220;
        s_next_blink = t + 2800 + (t % 1700);
    }

    // 情境说明胶囊：仅在弹层/子模式或临时消息时出现，纯坞位浏览时隐藏。
    const char *caption = NULL;
    if (t < s_msg_until) {
        caption = s_msg;
    } else if (s_mode == MODE_GAME) {
        caption = "High-Low";
    } else if (s_mode == MODE_GAMES) {
        caption = "Games";
    } else if (s_mode == MODE_JOB) {
        caption = "Work";
    } else if (s_mode == MODE_AGENCY) {
        caption = "Agency";
    } else if (s_mode == MODE_GX) {
        caption = GAMES_NAME[s_gx_id];
    } else if (s_mode == MODE_SHOP) {
        caption = s_shop_bag ? "Bag" : "Shop";
    } else if (s_mode == MODE_DECOR) {
        caption = "Style";
    } else if (s_mode == MODE_CHECKIN) {
        caption = "Daily bonus";
    }
    if (caption != NULL) {
        lv_label_set_text(s_caption_txt, caption);
        show(s_caption);
    } else {
        hide(s_caption);
    }

    // 游戏退出二次确认超时
    if (s_mode == MODE_GAME && s_g_exit_armed_until != 0
        && t >= s_g_exit_armed_until) {
        s_g_exit_armed_until = 0;
        if (s_g_phase == PHASE_CHOOSE) {
            lv_label_set_text(s_game_hint, "UP/DOWN choose  OK deal");
        }
    }
    if (s_mode == MODE_GX) {
        if (s_gx_exit_armed_until != 0 && t >= s_gx_exit_armed_until) {
            s_gx_exit_armed_until = 0;
            if (s_gx_phase == 1) {
                lv_label_set_text(s_gx_hint, gx_hint_text());
            }
        }
        gx_refresh(t);
    }

    s_prev = s_snap;
    s_have_prev = true;
}

// ---------------------------------------------------------------------------
// 初始化：唯一主屏
// ---------------------------------------------------------------------------

void pet_ui_init(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    // 顶栏（奶油带 0..26）
    rect(s_scr, 0, 0, LCD_W, 26, 0, COL_CREAM);
    lv_obj_t *hair = rect(s_scr, 0, 25, LCD_W, 1, 0, COL_INK);
    lv_obj_set_style_bg_opa(hair, LV_OPA_20, 0);
    lv_obj_t *stage_pill = rect(s_scr, 6, 4, 66, 18, 9, COL_PILL_PINK);
    border(stage_pill, COL_INK, 1);
    s_top_stage = lv_label_create(stage_pill);
    lv_obj_set_style_text_font(s_top_stage, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_top_stage, lv_color_hex(COL_INK), 0);
    lv_label_set_text(s_top_stage, "EGG");
    lv_obj_center(s_top_stage);
    s_celestial = icon_img(s_scr, 128, 7, pet_ui_small_dsc(PET_UI_SMALL_SUN));
    s_clock = label(s_scr, 142, 5, 54, &lv_font_montserrat_12, COL_INK, "08:00");
    s_battery = label(s_scr, 196, 5, 40, &lv_font_montserrat_12, COL_INK, "--");
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    hide(s_battery);   // 电量计可读后由刷新逻辑显示

    // 房间（26..252）：墙 + 地板 + 地毯落影 + 贴花
    s_room = rect(s_scr, 0, 26, LCD_W, 226, 0, COL_ROOM);
    s_wall_top = rect(s_room, 0, 0, LCD_W, 186, 0, COL_ROOM_TOP);
    s_floor_obj = rect(s_room, 0, 186, LCD_W, 40, 0, COL_FLOOR);
    s_floor_edge = rect(s_room, 0, 186, LCD_W, 2, 0, COL_FLOOR_EDGE);
    lv_obj_t *rug = rect(s_room, 64, 182, 112, 16, 8, COL_RUG);
    lv_obj_set_style_border_width(rug, 0, 0);
    lv_obj_t *shadow = rect(s_room, 80, 182, 80, 12, 6, COL_SHADOW);
    lv_obj_set_style_border_width(shadow, 0, 0);
    lv_obj_set_style_bg_opa(shadow, 140, 0);
    // 太阳/盆栽贴花（白天显示；夜间层在其上方直接盖住）
    icon_img(s_room, 191, 9, pet_ui_deco_dsc(PET_UI_DECO_SUN));
    icon_img(s_room, 13, 165, pet_ui_deco_dsc(PET_UI_DECO_PLANT));

    // 夜间房间层（定稿 mockup）：不透明深蓝墙/地 + 月亮星星 + 暗色地毯。
    // z 序在家具/宠物之下——盖住白天墙地贴花，宠物、气泡、Zzz 仍留在画面里。
    // 全用不透明色块：整屏 alpha 合成在模拟器软件渲染下会触发 WDT。
    s_night = lv_obj_create(s_room);
    lv_obj_remove_flag(s_night, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_night, 0, 0);
    lv_obj_set_size(s_night, LCD_W, 226);
    lv_obj_set_style_bg_opa(s_night, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_night, 0, 0);
    lv_obj_set_style_pad_all(s_night, 0, 0);
    rect(s_night, 0, 0, LCD_W, 186, 0, COL_NIGHT_WALL);
    rect(s_night, 0, 186, LCD_W, 40, 0, COL_NIGHT_FLOOR);
    rect(s_night, 0, 186, LCD_W, 2, 0, COL_NIGHT_EDGE);
    lv_obj_t *n_shadow = rect(s_night, 80, 182, 80, 12, 6, COL_NIGHT_SHADOW);
    lv_obj_set_style_border_width(n_shadow, 0, 0);
    lv_obj_t *n_rug = rect(s_night, 64, 182, 112, 16, 8, COL_NIGHT_RUG);
    lv_obj_set_style_border_width(n_rug, 0, 0);
    // mockup 月亮屏坐标 (200,58) r12 → 房间 (200,32)；26px 贴图左上 (187,19)
    icon_img(s_night, 187, 19, pet_ui_deco_dsc(PET_UI_DECO_MOON));
    static const int8_t night_star_xy[5][2] = {
        { 35, 43 }, { 59, 21 }, { 119, 13 }, { 219, 93 }, { 23, 103 },
    };
    for (int i = 0; i < 5; i += 1) {
        lv_obj_t *nst = rect(s_night, night_star_xy[i][0],
                             night_star_xy[i][1], 2, 2, 1, COL_NIGHT_STAR);
        lv_obj_set_style_border_width(nst, 0, 0);
    }
    hide(s_night);

    // S4 家具标记层（在宠物之下）
    s_furn_layer = lv_obj_create(s_room);
    lv_obj_remove_flag(s_furn_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_furn_layer, 0, 0);
    lv_obj_set_size(s_furn_layer, LCD_W, 226);
    lv_obj_set_style_bg_opa(s_furn_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_furn_layer, 0, 0);
    lv_obj_set_style_pad_all(s_furn_layer, 0, 0);

    // S4 星空主题点缀（固定位置，仅 Starry 主题显示）
    static const int8_t star_xy[6][2] = {
        { 24, 26 }, { 70, 16 }, { 120, 30 },
        { 168, 14 }, { 206, 40 }, { 150, 64 },
    };
    for (int i = 0; i < 6; i += 1) {
        s_star[i] = rect(s_room, star_xy[i][0], star_xy[i][1], 3, 3, 1,
                         0xFFE97A);
        hide(s_star[i]);
    }

    s_creature = lv_obj_create(s_room);
    lv_obj_remove_flag(s_creature, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_creature, CREATURE_X, CREATURE_Y);
    lv_obj_set_size(s_creature, 120, 112);
    lv_obj_set_style_bg_opa(s_creature, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_creature, 0, 0);
    lv_obj_set_style_pad_all(s_creature, 0, 0);
    // P2-S2：128x128 合成画布比 120x112 容器略大并负偏移居中，允许子对象越界绘制
    // （画布仍完全落在 s_room 范围内，不会被更外层裁掉）。
    lv_obj_add_flag(s_creature, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    s_bubble = lv_obj_create(s_room);
    lv_obj_remove_flag(s_bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_bubble, 146, 36);
    lv_obj_set_size(s_bubble, 32, 32);
    lv_obj_set_style_radius(s_bubble, 16, 0);
    lv_obj_set_style_bg_color(s_bubble, lv_color_hex(0xFFE97A), 0);
    lv_obj_set_style_bg_opa(s_bubble, LV_OPA_COVER, 0);
    border(s_bubble, 0xD9B53A, 2);
    lv_obj_set_style_pad_all(s_bubble, 0, 0);
    s_bubble_txt = label(s_bubble, 0, 4, 32, &lv_font_montserrat_20, COL_INK, "!");
    hide(s_bubble);

    s_poops = lv_obj_create(s_room);
    lv_obj_remove_flag(s_poops, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_poops, 0, 180);
    lv_obj_set_size(s_poops, LCD_W, 34);
    lv_obj_set_style_bg_opa(s_poops, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_poops, 0, 0);
    lv_obj_set_style_pad_all(s_poops, 0, 0);

    // 情境提示胶囊（地板下缘居中，仅子模式/临时消息可见）
    s_caption = rect(s_scr, 60, 236, 120, 16, 8, COL_CREAM);
    border(s_caption, COL_INK, 1);
    s_caption_txt = lv_label_create(s_caption);
    lv_obj_set_style_text_font(s_caption_txt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_caption_txt, lv_color_hex(COL_INK), 0);
    lv_obj_center(s_caption_txt);
    hide(s_caption);

    // 微状态行（奶油带 252..276）
    rect(s_scr, 0, 252, LCD_W, 24, 0, COL_CREAM);
    icon_img(s_scr, 10, 258, pet_ui_small_dsc(PET_UI_SMALL_BOWL));
    for (int i = 0; i < 4; i += 1) {
        s_heart_full[i] = icon_img(s_scr, 23 + i * 12, 258,
                                   pet_ui_small_dsc(PET_UI_SMALL_HEART_EMPTY));
    }
    icon_img(s_scr, 86, 258, pet_ui_small_dsc(PET_UI_SMALL_SMILE));
    for (int i = 0; i < 4; i += 1) {
        s_heart_fun[i] = icon_img(s_scr, 103 + i * 12, 258,
                                  pet_ui_small_dsc(PET_UI_SMALL_HEART_EMPTY));
    }
    lv_obj_t *weight_pill = rect(s_scr, 158, 257, 34, 14, 7, COL_PILL_BLUE);
    border(weight_pill, COL_INK, 1);
    s_weight_lbl = lv_label_create(weight_pill);
    lv_obj_set_style_text_font(s_weight_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_weight_lbl, lv_color_hex(COL_INK), 0);
    lv_label_set_text(s_weight_lbl, "0g");
    lv_obj_center(s_weight_lbl);
    icon_img(s_scr, 202, 258, pet_ui_small_dsc(PET_UI_SMALL_HEART_PINK));
    s_bond_lbl = label(s_scr, 214, 258, 22, &lv_font_montserrat_12, COL_INK, "0");

    // 坞带（276..320）
    rect(s_scr, 0, 276, LCD_W, 44, 0, COL_DOCK_BAND);
    rect(s_scr, 0, 276, LCD_W, 2, 0, COL_DOCK_EDGE);
    s_dockbox = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_dockbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_dockbox, 0, 276);
    lv_obj_set_size(s_dockbox, LCD_W, 44);
    lv_obj_set_style_bg_opa(s_dockbox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dockbox, 0, 0);
    lv_obj_set_style_pad_all(s_dockbox, 0, 0);

    // 通用弹层：整面不透明（原拓麻歌子式整屏切换）。不用全屏半透明遮罩——
    // 整屏 alpha 合成在模拟器软件渲染下会耗尽 CPU，真机上也无必要。
    s_modal = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_set_size(s_modal, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(s_modal, lv_color_hex(COL_ROOM_TOP), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_modal, 0, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);
    lv_obj_set_style_pad_all(s_modal, 0, 0);
    lv_obj_t *panel = rect(s_modal, 16, 40, 208, 240, 16, COL_PANEL);
    border(panel, COL_DIM, 2);
    // PV2：14px 加粗左对齐标题（20 留给孵化/进化英雄时刻）。
    s_modal_title = label(panel, 14, 13, 104, &lv_font_montserrat_14,
                          COL_INK, "");
    lv_obj_set_style_text_align(s_modal_title, LV_TEXT_ALIGN_LEFT, 0);
    s_mhair = rect(panel, 0, 36, 208, 1, 0, COL_HAIR);
    // 金币胶囊（SHOP/BAG）。
    s_mcoin = rect(panel, 150, 12, 40, 17, 9, COL_COIN_BG);
    border(s_mcoin, COL_INK, 1);
    s_mcoin_lbl = label(s_mcoin, 11, 2, 27, &lv_font_montserrat_12,
                        COL_INK, "");
    // PV2 batch6 STYLE：贝壳券 chip（金币 chip 左侧，蓝底蓝字蓝边）。
    s_tshell = rect(panel, 112, 12, 34, 17, 8, COL_PILL_BLUE);
    border(s_tshell, COL_BLUE, 1);
    s_tshell_lbl = label(s_tshell, 0, 2, 34, &lv_font_montserrat_12,
                         COL_BLUE, "");
    hide(s_tshell);
    // 页码 < 1/2 >（标题右侧）。
    s_mp_prev = label(panel, 56, 15, 12, &lv_font_montserrat_12, COL_INK,
                       "<");
    s_mp_cur = label(panel, 66, 15, 26, &lv_font_montserrat_12, COL_INK,
                      "1/1");
    s_mp_next = label(panel, 90, 15, 12, &lv_font_montserrat_12, COL_INK,
                       ">");

    s_modal_body = lv_obj_create(panel);
    lv_obj_remove_flag(s_modal_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_modal_body, 8, 44);
    lv_obj_set_size(s_modal_body, 192, 188);
    lv_obj_set_style_bg_opa(s_modal_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_modal_body, 0, 0);
    lv_obj_set_style_pad_all(s_modal_body, 0, 0);

    // 页脚操作条：‹ / SHOP / BAG / ›（PV2 卡片弹层）。
    s_mfooter = lv_obj_create(panel);
    lv_obj_remove_flag(s_mfooter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_mfooter, 0, 200);
    lv_obj_set_size(s_mfooter, 208, 22);
    lv_obj_set_style_bg_opa(s_mfooter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_mfooter, 0, 0);
    lv_obj_set_style_pad_all(s_mfooter, 0, 0);
    s_ft_prev = rect(s_mfooter, 14, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_ft_prev, COL_INK, 1);
    label(s_ft_prev, 0, 3, 20, &lv_font_montserrat_12, COL_INK, "<");
    s_ft_shop = rect(s_mfooter, 28, 0, 76, 20, 9, COL_TAB_ON);
    border(s_ft_shop, COL_INK, 1);
    label(s_ft_shop, 0, 3, 76, &lv_font_montserrat_12, COL_INK, "SHOP");
    s_ft_bag = rect(s_mfooter, 104, 0, 76, 20, 9, COL_DOCK_BG);
    border(s_ft_bag, COL_INK, 1);
    label(s_ft_bag, 0, 3, 76, &lv_font_montserrat_12, COL_INK, "BAG");
    s_ft_next = rect(s_mfooter, 174, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_ft_next, COL_INK, 1);
    label(s_ft_next, 0, 3, 20, &lv_font_montserrat_12, COL_INK, ">");

    // PV2 batch2 STATUS 页脚：‹ INFO VITALS CARE ›（与 s_mfooter 同高、互斥显示）。
    s_sfooter = lv_obj_create(panel);
    lv_obj_remove_flag(s_sfooter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_sfooter, 0, 200);
    lv_obj_set_size(s_sfooter, 208, 22);
    lv_obj_set_style_bg_opa(s_sfooter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_sfooter, 0, 0);
    lv_obj_set_style_pad_all(s_sfooter, 0, 0);
    s_sf_prev = rect(s_sfooter, 10, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_sf_prev, COL_INK, 1);
    label(s_sf_prev, 0, 3, 20, &lv_font_montserrat_12, COL_INK, "<");
    static const char *const SF_TAB_NAME[3] = { "INFO", "VITALS", "CARE" };
    static const int32_t SF_TAB_X[3] = { 34, 81, 128 };
    for (int i = 0; i < 3; i += 1) {
        s_sf_tab[i] = rect(s_sfooter, SF_TAB_X[i], 0, 46, 20, 9, COL_DOCK_BG);
        border(s_sf_tab[i], COL_INK, 1);
        label(s_sf_tab[i], 0, 3, 46, &lv_font_montserrat_12, COL_INK,
              SF_TAB_NAME[i]);
    }
    s_sf_next = rect(s_sfooter, 178, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_sf_next, COL_INK, 1);
    label(s_sf_next, 0, 3, 20, &lv_font_montserrat_12, COL_INK, ">");
    hide(s_sfooter);

    // PV2 batch4 DEX 页脚：‹ ALBUM PARTS BADGES ›（几何同 STATUS 页脚）。
    s_dfooter = lv_obj_create(panel);
    lv_obj_remove_flag(s_dfooter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_dfooter, 0, 200);
    lv_obj_set_size(s_dfooter, 208, 22);
    lv_obj_set_style_bg_opa(s_dfooter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dfooter, 0, 0);
    lv_obj_set_style_pad_all(s_dfooter, 0, 0);
    s_df_prev = rect(s_dfooter, 10, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_df_prev, COL_INK, 1);
    label(s_df_prev, 0, 3, 20, &lv_font_montserrat_12, COL_INK, "<");
    static const char *const DF_TAB_NAME[3] = { "ALBUM", "PARTS", "BADGES" };
    static const int32_t DF_TAB_X[3] = { 34, 81, 128 };
    for (int i = 0; i < 3; i += 1) {
        s_df_tab[i] = rect(s_dfooter, DF_TAB_X[i], 0, 46, 20, 9, COL_CARD_WHITE);
        border(s_df_tab[i], COL_INK, 1);
        label(s_df_tab[i], 0, 3, 46, &lv_font_montserrat_12, COL_INK,
              DF_TAB_NAME[i]);
    }
    s_df_next = rect(s_dfooter, 178, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_df_next, COL_INK, 1);
    label(s_df_next, 0, 3, 20, &lv_font_montserrat_12, COL_INK, ">");
    hide(s_dfooter);

    // PV2 batch5 FAMILY 页脚：三态 BACK/TREE、仅 BACK、‹ BACK ›。
    s_mffooter = lv_obj_create(panel);
    lv_obj_remove_flag(s_mffooter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_mffooter, 0, 200);
    lv_obj_set_size(s_mffooter, 208, 22);
    lv_obj_set_style_bg_opa(s_mffooter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_mffooter, 0, 0);
    lv_obj_set_style_pad_all(s_mffooter, 0, 0);
    s_mf_back = rect(s_mffooter, 10, 0, 44, 20, 9, COL_DOCK_BG);
    border(s_mf_back, COL_INK, 1);
    label(s_mf_back, 0, 3, 44, &lv_font_montserrat_12, COL_INK, "BACK");
    s_mf_tree = rect(s_mffooter, 154, 0, 44, 20, 9, COL_DOCK_BG);
    border(s_mf_tree, COL_INK, 1);
    label(s_mf_tree, 0, 3, 44, &lv_font_montserrat_12, COL_INK, "TREE");
    s_mf_prev = rect(s_mffooter, 10, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_mf_prev, COL_INK, 1);
    label(s_mf_prev, 0, 3, 20, &lv_font_montserrat_12, COL_INK, "<");
    s_mf_mid = rect(s_mffooter, 34, 0, 46, 20, 9, COL_DOCK_BG);
    border(s_mf_mid, COL_INK, 1);
    label(s_mf_mid, 0, 3, 46, &lv_font_montserrat_12, COL_INK, "BACK");
    s_mf_next = rect(s_mffooter, 178, 0, 20, 20, 8, COL_DOCK_BG);
    border(s_mf_next, COL_INK, 1);
    label(s_mf_next, 0, 3, 20, &lv_font_montserrat_12, COL_INK, ">");
    hide(s_mffooter);

    // PV2 batch6 签到：单颗居中 CLAIM 丸（常驻粉框，环长 1）。
    s_cclaim = rect(panel, 72, 200, 64, 20, 9, COL_DOCK_BG);
    border(s_cclaim, COL_SEL, 2);
    label(s_cclaim, 0, 3, 64, &lv_font_montserrat_12, COL_INK, "CLAIM");
    hide(s_cclaim);

    // PV2 batch6 STYLE 页脚：WEAR / ROOM / THEME 三档（当前档粉填充，焦点 2px 粉框）。
    s_tfooter = lv_obj_create(panel);
    lv_obj_remove_flag(s_tfooter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_tfooter, 0, 200);
    lv_obj_set_size(s_tfooter, 208, 22);
    lv_obj_set_style_bg_opa(s_tfooter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tfooter, 0, 0);
    lv_obj_set_style_pad_all(s_tfooter, 0, 0);
    static const int32_t TF_X[3] = { 10, 76, 142 };
    static const char *const TF_NAME[3] = { "WEAR", "ROOM", "THEME" };
    for (int i = 0; i < 3; i += 1) {
        s_tf_tab[i] = rect(s_tfooter, TF_X[i], 0, 56, 20, 9, COL_DOCK_BG);
        border(s_tf_tab[i], COL_INK, 1);
        label(s_tf_tab[i], 0, 3, 56, &lv_font_montserrat_12, COL_INK,
              TF_NAME[i]);
    }
    hide(s_tfooter);

    s_mback = label(panel, 0, 223, 208, &lv_font_montserrat_12, COL_HINT,
                    "hold OK - back");

    // 面板内顶部 toast：拒绝/反馈胶囊，覆盖在卡片之上。
    s_toast_box = rect(panel, 20, 48, 168, 22, 11, COL_PANEL);
    border(s_toast_box, COL_RED, 1);
    lv_obj_t *alert = rect(s_toast_box, 8, 6, 10, 10, 5, COL_RED);
    (void) alert;
    label(s_toast_box, 8, 3, 10, &lv_font_montserrat_12, COL_PANEL, "!");
    s_toast_msg = label(s_toast_box, 22, 3, 142, &lv_font_montserrat_12,
                        COL_RED, "");
    hide(s_toast_box);

    // 初始全部 chrome 隐藏，由各 builder 按范式点亮。
    hide(s_mhair);
    hide(s_mcoin);
    hide(s_mp_prev);
    hide(s_mp_cur);
    hide(s_mp_next);
    hide(s_mfooter);
    hide(s_mback);
    hide(s_modal);

    // 孵化/进化全屏演出（PV2 batch9：奶白接管 + 呼吸双环 + 爆开，零位图）。
    s_eff_cont = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_eff_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_eff_cont, 0, 0);
    lv_obj_set_size(s_eff_cont, LCD_W, LCD_H);
    lv_obj_set_style_radius(s_eff_cont, 0, 0);
    lv_obj_set_style_bg_color(s_eff_cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_eff_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_eff_cont, 0, 0);
    lv_obj_set_style_pad_all(s_eff_cont, 0, 0);
    s_eff_ring_out = rect(s_eff_cont, EFF_CX - EFF_RING_OUT_R,
                          EFF_RING_CY - EFF_RING_OUT_R,
                          EFF_RING_OUT_R * 2, EFF_RING_OUT_R * 2,
                          EFF_RING_OUT_R, COL_BG);
    lv_obj_set_style_bg_opa(s_eff_ring_out, LV_OPA_TRANSP, 0);
    border(s_eff_ring_out, COL_YELLOW, 2);
    s_eff_ring_in = rect(s_eff_cont, EFF_CX - EFF_RING_IN_R,
                         EFF_RING_CY - EFF_RING_IN_R,
                         EFF_RING_IN_R * 2, EFF_RING_IN_R * 2,
                         EFF_RING_IN_R, COL_BG);
    lv_obj_set_style_bg_opa(s_eff_ring_in, LV_OPA_TRANSP, 0);
    border(s_eff_ring_in, COL_SEL, 2);
    s_eff_glyph = label(s_eff_cont, 0, 100, LCD_W,
                        &lv_font_montserrat_20, COL_SEL, "!");
    s_eff_word = label(s_eff_cont, 0, 158, LCD_W,
                       &lv_font_montserrat_12, COL_SUB, "HATCHING");
    lv_obj_set_style_text_letter_space(s_eff_word, 2, 0);
    s_eff_burst = rect(s_eff_cont, EFF_CX - EFF_BURST_R0,
                       EFF_BURST_CY - EFF_BURST_R0,
                       EFF_BURST_R0 * 2, EFF_BURST_R0 * 2,
                       EFF_BURST_R0, COL_BG);
    lv_obj_set_style_bg_opa(s_eff_burst, LV_OPA_TRANSP, 0);
    border(s_eff_burst, COL_SEL, 2);
    hide(s_eff_burst);
    static const uint32_t s_eff_spark_col[4] = {
        COL_YELLOW, COL_SEL, COL_SEL, COL_YELLOW
    };
    for (int i = 0; i < 4; i += 1) {
        s_eff_spark[i] = rect(s_eff_cont, 117, 111, 6, 6, 3,
                              s_eff_spark_col[i]);
        hide(s_eff_spark[i]);
    }
    hide(s_eff_cont);

    // 死亡纪念（PV2 batch8：全屏深紫 + 居中纪念卡，零位图提灯标记）。
    s_memorial = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_memorial, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_memorial, 0, 0);
    lv_obj_set_size(s_memorial, LCD_W, LCD_H);
    lv_obj_set_style_radius(s_memorial, 0, 0);
    lv_obj_set_style_bg_color(s_memorial, lv_color_hex(0x201A2E), 0);
    lv_obj_set_style_bg_opa(s_memorial, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_memorial, 0, 0);
    lv_obj_set_style_pad_all(s_memorial, 0, 0);
    // 纪念卡：屏 (36,64) 168x192 r16，深紫卡 + 薰衣紫 28% 发丝边。
    lv_obj_t *mcard = rect(s_memorial, 36, 64, 168, 192, 16, 0x2B2442);
    lv_obj_set_style_border_color(mcard, lv_color_hex(0xC9BBDD), 0);
    lv_obj_set_style_border_width(mcard, 1, 0);
    lv_obj_set_style_border_opa(mcard, 71, 0);
    // 提灯标记：36x36 圆环（2px）+ 8x8 金焰点，全用矩形，无位图。
    lv_obj_t *mring = rect(s_memorial, 66, 88, 36, 36, 18, 0x2B2442);
    lv_obj_set_style_bg_opa(mring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(mring, lv_color_hex(0xC9BBDD), 0);
    lv_obj_set_style_border_width(mring, 2, 0);
    rect(s_memorial, 80, 102, 8, 8, 4, 0xF2B134);
    // 基线 y152/176/210/230/250（冻结稿）；label 顶 = 基线 - (line_height-base_line)。
    lv_obj_t *mkicker = label(s_memorial, 0, 140, LCD_W,
                              &lv_font_montserrat_12, 0x9C8FB0,
                              "IN MEMORY OF");
    lv_obj_set_style_text_letter_space(mkicker, 2, 0);
    // 名字与年龄行在事件时回填（固定对象，避免重复创建）。
    s_memorial_name = label(s_memorial, 0, 158, LCD_W,
                            &lv_font_montserrat_20, 0xF0E6D8, "");
    lv_obj_t *mhair = rect(s_memorial, 96, 190, 48, 1, 0, 0xC9BBDD);
    lv_obj_set_style_bg_opa(mhair, 77, 0);
    s_memorial_line = label(s_memorial, 0, 198, LCD_W,
                            &lv_font_montserrat_12, 0xC9BBDD, "");
    s_memorial_tier = label(s_memorial, 0, 218, LCD_W,
                            &lv_font_montserrat_12, 0x9C8FB0, "");
    lv_obj_t *mrip = label(s_memorial, 0, 237, LCD_W,
                           &lv_font_montserrat_14, 0xF0E6D8, "R.I.P.");
    lv_obj_set_style_text_letter_space(mrip, 1, 0);
    label(s_memorial, 0, 268, LCD_W, &lv_font_montserrat_12, 0x7E7294,
          "OK - new egg");
    hide(s_memorial);

    // 先拿一帧状态再开定时器，避免首帧空画。
    pet_app_snapshot(&s_snap);
    pet_app_decor_snapshot(&s_dsnap);
    s_have_dprev = false;
    apply_room_theme();
    build_creature();
    build_poops();
    build_furniture();
    s_dprev = s_dsnap;
    s_have_dprev = true;
    dock_build_widgets();
    dock_refresh_selected();
    s_prev = s_snap;
    s_have_prev = true;
    s_next_blink = now_ms() + 2500;

    s_timer = lv_timer_create(refresh, 200, NULL);
    lv_screen_load(s_scr);
}

