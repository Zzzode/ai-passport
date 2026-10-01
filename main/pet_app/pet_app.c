#include "pet_app.h"

#include <stdio.h>
#include <string.h>

#include "bsp_display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "pet_audio.h"
#include "pet_clock.h"
#include "pet_decor.h"
#include "pet_dex_save.h"
#include "pet_econ.h"
#include "pet_jobs.h"
#include "pet_social_save.h"
#include "pet_store.h"
#include "pet_ui.h"
#include "pt_config.h"
#include "pt_dex.h"
#include "pt_engine.h"
#include "pt_events.h"
#include "pt_social.h"

static const char *TAG = "pet_app";

#define TICK_MS        100
#define INTENT_Q_DEPTH 8
#define UI_EV_DEPTH    32
#define SAVE_MERGE_MS  10000

typedef enum {
    CMD_INTENT = 1,
    CMD_NEW_GAME,
    CMD_ECON,
    CMD_SOC,
} cmd_kind_t;

typedef enum {
    SOC_GREET = 1,
    SOC_GIFT,
    SOC_PROPOSE,
    SOC_START_EGG,   // S3b：EGG_READY → 迎接子代新蛋（世代交替）
} soc_sub_t;

typedef enum {
    ECON_CHECKIN = 1,
    ECON_BUY,
    ECON_EAT,
    ECON_PLAY,
    ECON_GAME,
    ECON_JOB_WORK,
    ECON_JOB_SWITCH,
    ECON_DECOR_OP,
} econ_sub_t;

typedef struct {
    cmd_kind_t kind;
    pt_intent_kind_t intent;
    uint8_t param;
    uint8_t econ_sub;
    uint8_t soc_sub;
    uint16_t item;
} app_cmd_t;

static pt_state_t s_state;
static pt_econ_t s_econ;
static pt_jobs_t s_jobs;
static pt_decor_t s_decor;
static pt_social_t s_social;
static pt_dex_t s_dex;
static pt_events_t s_core_events;
static QueueHandle_t s_cmd_q;
static QueueHandle_t s_ui_ev_q;
static SemaphoreHandle_t s_state_mu;

static bool s_med_unlocked;
static bool s_quiet;
static uint8_t s_volume;
static int64_t s_dirty_at_ms;
static int64_t s_last_save_ms;
static int32_t s_econ_day;
static char s_notice[40];      // S5：引擎任务给 UI 的一次性提示（如月初津贴）
static bool s_notice_pending;

// P2-S3a：社交动作裁决结果一次性槽（UI 下个刷新周期取走做浮条）。
static uint8_t s_soc_rv;
static bool s_soc_rv_pending;

// P2-S4：图鉴见闻跟踪。s_obs_species 记录上次已入账的物种（用于少年→成年
// 时给少年形态补 RAISED）；新档/读档时与当前物种对齐。
static pt_species_t s_obs_species;

// 本 tick 已扣库存/次数、等待引擎裁决的动作队列；REFUSED 时按意图种类退还对应物品。
// 用列表而非单变量：连点不同玩具时不会互相覆盖。
typedef struct {
    uint16_t item;
    bool toy;       // true=玩具次数, false=食物库存
} pending_use_t;
// 16 > INTENT_Q_DEPTH(8)：单 tick 最多排空 8 条命令，保证每条成功预扣都有退款跟踪。
static pending_use_t s_pending[16];
static uint8_t s_pending_n;

// 开机第一颗蛋从当天 08:00 开始（夜眠窗口之外，体验最顺）。
static int32_t morning_eight_today(int32_t now_minute)
{
    int32_t day = now_minute / 1440;
    if (day < 0) {
        day = 0;
    }
    return day * 1440 + 8 * 60;
}

static void do_save_now(void)
{
    xSemaphoreTake(s_state_mu, portMAX_DELAY);
    bool ok = pet_store_save(&s_state, &s_econ, &s_jobs, &s_decor);
    if (ok) {
        uint8_t blob[PET_SOC_SAVE_BYTES];
        if (pet_soc_save_encode(&s_social, blob, sizeof(blob))
            == PET_SOC_SAVE_BYTES) {
            ok = pet_store_social_save(blob, sizeof(blob));
        } else {
            ok = false;
        }
    }
    if (ok) {
        uint8_t blob[PET_DEX_SAVE_BYTES];
        if (pet_dex_save_encode(&s_dex, blob, sizeof(blob))
            == PET_DEX_SAVE_BYTES) {
            ok = pet_store_dex_save(blob, sizeof(blob));
        } else {
            ok = false;
        }
    }
    s_dirty_at_ms = 0;
    s_last_save_ms = pet_clock_uptime_ms();
    xSemaphoreGive(s_state_mu);
    if (!ok) {
        ESP_LOGW(TAG, "save failed (both banks kept intact)");
    }
}

// 把当前摆放家具的增益推给引擎（运行期参数；开机/装饰变更后调用）。
static void decor_sync_engine(void)
{
    pt_engine_set_room_buffs(pt_decor_meal_bonus(&s_decor),
                             pt_decor_happy_pct(&s_decor),
                             pt_decor_energy_pct(&s_decor));
}

// 经济货架种子与生命 RNG 同源但不同盐，互不干扰。
static uint32_t shelf_seed_from(uint32_t life_seed)
{
    return life_seed ^ 0x9E3779B9u;
}

static void start_new_game(void)
{
    int32_t start = morning_eight_today(pet_clock_now_minute());
    uint32_t seed = (uint32_t) esp_random()
        ^ (uint32_t) (pet_clock_uptime_ms() * 2654435761u)
        ^ 0x5a5a1234u;

    xSemaphoreTake(s_state_mu, portMAX_DELAY);
    pt_events_clear(&s_core_events);
    pt_state_new(&s_state, seed, start);
    pt_econ_init(&s_econ, shelf_seed_from(seed));
    pt_jobs_init(&s_jobs);
    pt_social_init(&s_social, seed ^ 0x50C1A1u, s_state.day_id);
    pt_dex_init(&s_dex);
    s_obs_species = PT_SP_EGG;
    s_econ_day = s_state.day_id;
    s_pending_n = 0;
    s_notice_pending = false;
    s_soc_rv_pending = false;
    xSemaphoreGive(s_state_mu);

    pet_clock_anchor(start);

    // 清掉上一世代留给 UI 的残余事件。
    pt_event_t stale;
    while (xQueueReceive(s_ui_ev_q, &stale, 0) == pdTRUE) {
    }

    do_save_now();
    ESP_LOGI(TAG, "new egg at minute %d", (int) start);
}

// 免打扰时段：22:00–次日 08:00（designs 03 §4）。睡眠期引擎本就不起呼叫、
// 不记失误；这里只负责时段内把呼叫蜂鸣静音（呼叫灯/气泡照常提示）。
static bool in_quiet_hours(int32_t minute)
{
    int32_t mod = minute % 1440;
    if (mod < 0) {
        mod += 1440;
    }
    return mod >= 22 * 60 || mod < 8 * 60;
}

// ---- P2-S4 图鉴：事件见闻入账（调用方持 s_state_mu）----
static bool genome_nonzero(const pt_genome_t *g)
{
    const uint8_t *raw = (const uint8_t *) g;
    for (size_t i = 0; i < sizeof(*g); i += 1) {
        if (raw[i] != 0) {
            return true;
        }
    }
    return false;
}

static void dex_observe_event(const pt_event_t *ev)
{
    switch (ev->kind) {
    case PT_EV_HATCH:
        // 孵出：自家宠基因型全部记 OWNED（含当前一代）。
        pt_dex_observe_genome(&s_dex, &s_state.genome, PT_DEX_PART_OWNED);
        s_obs_species = s_state.species;
        break;
    case PT_EV_EVOLUTION:
        // a=新阶段 b=新物种。少年→成年：少年形态养大（RAISED）。
        if (ev->a == PT_STAGE_TEEN) {
            pt_dex_observe_species(&s_dex, (pt_species_t) ev->b,
                                   PT_DEX_LV_SEEN);
        } else if (ev->a == PT_STAGE_ADULT) {
            pt_dex_raise_species(&s_dex, s_obs_species,
                                 pt_dex_species_care(s_obs_species));
            pt_dex_raise_species(&s_dex, (pt_species_t) ev->b,
                                 pt_dex_species_care((pt_species_t) ev->b));
            // 成年定型的基因就是从小携带的基因，补 OWNED（老档/首只宠）。
            pt_dex_observe_genome(&s_dex, &s_state.genome,
                                  PT_DEX_PART_OWNED);
        }
        s_obs_species = (pt_species_t) ev->b;
        break;
    case PT_EV_STAGE_CHANGED:
        // 成年→老年：当前物种 MASTERED（养老）。
        if (ev->a == PT_STAGE_SENIOR) {
            pt_dex_observe_species(&s_dex, s_state.species,
                                   PT_DEX_LV_MASTERED);
        } else if (ev->a == PT_STAGE_CHILD) {
            s_obs_species = PT_SP_CHILD;
        }
        break;
    case PT_EV_DEATH:
        pt_dex_record_age(&s_dex, s_state.age_days);
        break;
    case PT_EV_SOC_CANDIDATE:
        // 媒婆名片上的陌生人：部件只记 SEEN。
        if (ev->a < PT_SOC_CANDIDATES && s_social.cand[ev->a].valid) {
            pt_dex_observe_genome(&s_dex, &s_social.cand[ev->a].genome,
                                  PT_DEX_PART_SEEN);
        }
        break;
    case PT_EV_SOC_WEDDING:
        if (genome_nonzero(&s_social.spouse)) {
            pt_dex_observe_genome(&s_dex, &s_social.spouse,
                                  PT_DEX_PART_SEEN);
        }
        // 成家：当前成年形态 MASTERED。
        pt_dex_observe_species(&s_dex, s_state.species, PT_DEX_LV_MASTERED);
        break;
    case PT_EV_SOC_EGG_READY:
        // 育种子代：部件记 BRED（自动含 SEEN）。
        pt_dex_observe_genome(&s_dex, &s_social.child_genome,
                              PT_DEX_PART_BRED);
        break;
    default:
        break;
    }
}

// 老档首次升级 S4：从当前宠 + 名人堂回填图鉴，避免老玩家收藏清零。
static void dex_backfill(void)
{
    pt_dex_observe_genome(&s_dex, &s_state.genome, PT_DEX_PART_OWNED);
    for (uint8_t i = 0; i < PT_SOC_CANDIDATES; i += 1) {
        if (s_social.cand[i].valid) {
            pt_dex_observe_genome(&s_dex, &s_social.cand[i].genome,
                                  PT_DEX_PART_SEEN);
        }
    }
    if (s_state.stage == PT_STAGE_TEEN) {
        pt_dex_observe_species(&s_dex, s_state.species, PT_DEX_LV_SEEN);
    } else if (s_state.stage == PT_STAGE_ADULT
               || s_state.stage == PT_STAGE_SENIOR
               || s_state.stage == PT_STAGE_DEAD) {
        pt_dex_raise_species(&s_dex, s_state.species,
                             pt_dex_species_care(s_state.species));
        if (s_state.stage == PT_STAGE_SENIOR
            || s_state.stage == PT_STAGE_DEAD) {
            pt_dex_observe_species(&s_dex, s_state.species,
                                   PT_DEX_LV_MASTERED);
        }
    }
    for (uint8_t i = 0; i < s_social.hall_count; i += 1) {
        const pt_soc_hall_t *h = &s_social.hall[i];
        uint8_t care = h->care < PT_DEX_CARE_COUNT
            ? h->care : pt_dex_species_care((pt_species_t) h->species);
        pt_dex_raise_species(&s_dex, (pt_species_t) h->species, care);
        pt_dex_observe_species(&s_dex, (pt_species_t) h->species,
                               PT_DEX_LV_MASTERED);   // 入堂皆已成家
        pt_dex_observe_genome(&s_dex, &h->pet, PT_DEX_PART_OWNED);
        if (genome_nonzero(&h->mate)) {
            pt_dex_observe_genome(&s_dex, &h->mate, PT_DEX_PART_SEEN);
        }
    }
    s_obs_species = s_state.species;
}

// 引擎事件 → 音效；并返回该事件是否需要立即存档。
static bool map_sound(const pt_event_t *ev)
{
    switch (ev->kind) {
    case PT_EV_HATCH:
        pet_audio_play(SND_HAPPY);
        return true;
    case PT_EV_EVOLUTION:
        pet_audio_play(SND_EVOLVE);
        return true;
    case PT_EV_STAGE_CHANGED:
        pet_audio_play(SND_CONFIRM);
        return true;
    case PT_EV_DEATH:
        pet_audio_play(SND_DEATH);
        return true;
    case PT_EV_CALL_RAISED:
        if (!(s_quiet && in_quiet_hours(s_state.minute))) {
            pet_audio_play(SND_CALL);
        }
        break;
    case PT_EV_CALL_RESOLVED:
        pet_audio_play(SND_CONFIRM);
        break;
    case PT_EV_CALL_EXPIRED:
    case PT_EV_MISTAKE_SMALL:
    case PT_EV_MISTAKE_BIG:
        pet_audio_play(SND_SAD);
        break;
    case PT_EV_SICK:
        pet_audio_play(SND_SICK);
        if (!s_med_unlocked) {
            s_med_unlocked = true;
            pet_store_set_med(true);
        }
        break;
    case PT_EV_CURED:
        pet_audio_play(SND_HAPPY);
        break;
    case PT_EV_MEDICINE_FAILED:
        pet_audio_play(SND_SAD);
        break;
    case PT_EV_REFUSED:
        pet_audio_play(SND_CANCEL);
        break;
    case PT_EV_SOC_CANDIDATE:
        pet_audio_play(SND_CONFIRM);
        break;
    case PT_EV_SOC_WEDDING:
        pet_audio_play(SND_EVOLVE);
        return true;
    case PT_EV_SOC_EGG_READY:
        pet_audio_play(SND_HAPPY);
        return true;
    case PT_EV_SOC_PARENTS_LEAVE:
        pet_audio_play(SND_EVOLVE);
        return true;
    default:
        break;
    }
    return false;
}

static void engine_task(void *arg)
{
    (void) arg;

    for (;;) {
        // 1) 先消费命令（意图 / 新蛋 / 经济动作），再推进时间。
        app_cmd_t cmd;
        bool touched = false;
        while (xQueueReceive(s_cmd_q, &cmd, 0) == pdTRUE) {
            if (cmd.kind == CMD_NEW_GAME) {
                start_new_game();
                touched = false;
                continue;
            }
            xSemaphoreTake(s_state_mu, portMAX_DELAY);
            if (cmd.kind == CMD_ECON) {
                switch (cmd.econ_sub) {
                case ECON_CHECKIN: {
                    pt_checkin_result_t r;
                    if (pt_econ_checkin(&s_econ, s_state.day_id, &r)) {
                        touched = true;
                        pet_audio_play(r.week_bonus ? SND_EVOLVE : SND_HAPPY);
                    }
                    break;
                }
                case ECON_BUY: {
                    pt_econ_rv_t rv = pt_econ_buy(&s_econ, s_state.stage,
                                                  s_state.minute,
                                                  (pt_item_t) cmd.item);
                    if (rv == PT_ECON_OK) {
                        touched = true;
                        pet_audio_play(SND_CONFIRM);
                    } else {
                        pet_audio_play(SND_CANCEL);
                    }
                    break;
                }
                case ECON_EAT: {
                    pt_item_t item = (pt_item_t) cmd.item;
                    if (pt_econ_take_food(&s_econ, item) == PT_ECON_OK) {
                        // 红线：经济层只扣库存，效果由引擎裁决；拒绝后在事件排空时补回。
                        if (s_pending_n < 16) {
                            s_pending[s_pending_n].item = cmd.item;
                            s_pending[s_pending_n].toy = false;
                            s_pending_n += 1;
                        }
                        const pt_item_def_t *d = pt_item_def(item);
                        uint8_t tag = d != NULL ? d->food_tag
                                               : (uint8_t) PT_FOOD_TAG_MEAL;
                        pt_handle_intent(&s_state, &s_core_events,
                                         pt_item_eat_intent(item), tag);
                        touched = true;
                    } else {
                        pet_audio_play(SND_CANCEL);
                    }
                    break;
                }
                case ECON_PLAY: {
                    pt_item_t item = (pt_item_t) cmd.item;
                    if (pt_econ_use_toy(&s_econ, item) == PT_ECON_OK) {
                        s_pending[s_pending_n].item = cmd.item;
                        s_pending[s_pending_n].toy = true;
                        s_pending_n += 1;
                        pt_handle_intent(&s_state, &s_core_events,
                                         PT_INTENT_PLAY_TOY, 0);
                        touched = true;
                    } else {
                        pet_audio_play(SND_CANCEL);
                    }
                    break;
                }
                case ECON_GAME: {
                    // 游戏结算走同一条串行管线：先引擎（心情/技能/体重/体力/亲密度），
                    // 再经济层发 G 币；生病/蛋期引擎会拒绝，拒绝不发奖。
                    uint8_t pack = (uint8_t) cmd.item;
                    pt_handle_intent(&s_state, &s_core_events,
                                     PT_INTENT_GAME_RESULT, pack);
                    if (!s_state.sick && s_state.stage != PT_STAGE_EGG) {
                        (void) pt_econ_game_payout(&s_econ,
                                                   PT_GAME_UNPACK_GRADE(pack));
                    }
                    touched = true;
                    break;
                }
                case ECON_JOB_WORK: {
                    // item = job*4 + grade。班次闸/门槛在职业层，体力闸由引擎裁决：
                    // 引擎拒绝（未成年/生病/体力<25）则不记班、不发工资。
                    uint8_t job = (uint8_t) (cmd.item / 4u);
                    uint8_t grade = (uint8_t) (cmd.item % 4u);
                    if (job < PT_JOB_COUNT && pt_jobs_can_work(&s_jobs)) {
                        const pt_job_def_t *d = pt_job_def((pt_job_id_t) job);
                        uint8_t cost = pt_cfg_game_energy[d->game];
                        bool allow = !s_state.sick
                            && (s_state.stage == PT_STAGE_ADULT
                                || s_state.stage == PT_STAGE_SENIOR)
                            && s_state.energy >= PT_CFG_JOB_MIN_ENERGY;
                        pt_handle_intent(&s_state, &s_core_events,
                                         PT_INTENT_JOB_SHIFT, cost);
                        if (allow) {
                            uint16_t wage = pt_jobs_work(&s_jobs,
                                                        (pt_job_id_t) job,
                                                        grade);
                            (void) pt_econ_credit_income(&s_econ, wage);
                            pet_audio_play(grade == PT_GAME_GRADE_GOOD
                                           ? SND_CONFIRM : SND_HAPPY);
                            touched = true;
                        } else {
                            pet_audio_play(SND_CANCEL);
                        }
                    } else {
                        pet_audio_play(SND_CANCEL);
                    }
                    break;
                }
                case ECON_JOB_SWITCH: {
                    if (cmd.item < PT_JOB_COUNT
                        && pt_jobs_switch(&s_jobs, (pt_job_id_t) cmd.item,
                                          s_state.skill, s_state.day_id)) {
                        touched = true;
                        pet_audio_play(SND_CONFIRM);
                    } else {
                        pet_audio_play(SND_CANCEL);
                    }
                    break;
                }
                case ECON_DECOR_OP: {
                    // item = op*256 + arg。装饰为本地收藏操作：无引擎裁决/退款。
                    uint8_t op = (uint8_t) (cmd.item >> 8);
                    uint8_t arg = (uint8_t) (cmd.item & 0xff);
                    pt_decor_rv_t rv = PT_DECOR_BAD_ID;
                    switch (op) {
                    case 1:
                        rv = pt_decor_buy_outfit(&s_decor, &s_econ,
                                                 s_state.stage, s_state.day_id,
                                                 (pt_outfit_t) arg);
                        break;
                    case 2:
                        rv = pt_decor_buy_furn(&s_decor, &s_econ,
                                               s_state.stage, s_state.day_id,
                                               (pt_furn_t) arg);
                        break;
                    case 3:
                        rv = pt_decor_buy_theme(&s_decor, &s_econ,
                                                (pt_theme_t) arg);
                        break;
                    case 4:
                        rv = pt_decor_equip(&s_decor, (pt_outfit_t) arg);
                        break;
                    case 5:
                        rv = pt_decor_unequip_slot(&s_decor,
                                                   (pt_slot_t) arg);
                        break;
                    case 6:
                        rv = pt_decor_place(&s_decor, (pt_furn_t) arg);
                        break;
                    case 7:
                        rv = pt_decor_unplace(&s_decor, (pt_furn_t) arg);
                        break;
                    case 8:
                        rv = pt_decor_set_theme(&s_decor, (pt_theme_t) arg);
                        break;
                    default:
                        break;
                    }
                    if (rv == PT_DECOR_OK) {
                        touched = true;
                        decor_sync_engine();
                        pet_audio_play(SND_CONFIRM);
                    } else {
                        pet_audio_play(SND_CANCEL);
                    }
                    break;
                }
                default:
                    break;
                }
            } else if (cmd.kind == CMD_SOC) {
                uint8_t ci = cmd.param;
                pt_soc_rv_t rv = PT_SOC_BAD_STATE;
                switch (cmd.soc_sub) {
                case SOC_GREET: {
                    rv = pt_social_greet(&s_social, ci, s_state.minute);
                    break;
                }
                case SOC_GIFT: {
                    rv = pt_social_gift(&s_social, ci, s_state.minute,
                                        &s_econ.coins,
                                        s_state.genome.personality);
                    break;
                }
                case SOC_PROPOSE: {
                    pt_ring_t ring = cmd.item != 0u ? PT_RING_DIAMOND
                                                    : PT_RING_SIMPLE;
                    rv = pt_social_propose(&s_social, ci, ring,
                                           s_state.minute, &s_econ.coins,
                                           &s_core_events);
                    if (rv == PT_SOC_OK) {
                        // 成家奖励 10 张贝壳券（08 §4 遗产/奖励）。
                        pt_econ_grant_shells(&s_econ,
                                             PT_CFG_FAMILY_SHELL_REWARD);
                    }
                    break;
                }
                case SOC_START_EGG: {
                    // S3b 世代交替：归档父母、代次+1、祖辈环过户，随后用育种
                    // 基因组开启子代新蛋；钱包/家具/职业等家族资产保留。
                    pt_genome_t child = s_social.child_genome;
                    if (pt_social_begin_generation(&s_social, &s_state)) {
                        uint32_t seed = (uint32_t) esp_random()
                            ^ (uint32_t) (pet_clock_uptime_ms()
                                          * 2654435761u)
                            ^ 0x3b6a11u;
                        int32_t now = s_state.minute;
                        pt_state_new_egg_genome(&s_state, seed, now, &child);
                        s_econ_day = s_state.day_id;
                        pt_engine_set_family_care(true);
                        rv = PT_SOC_OK;
                    }
                    break;
                }
                default:
                    break;
                }
                s_soc_rv = (uint8_t) rv;
                s_soc_rv_pending = true;
                if (rv == PT_SOC_OK) {
                    touched = true;
                    pet_audio_play(cmd.soc_sub == (uint8_t) SOC_PROPOSE
                                   ? SND_EVOLVE : SND_CONFIRM);
                } else {
                    pet_audio_play(SND_CANCEL);
                }
            } else {
                pt_handle_intent(&s_state, &s_core_events, cmd.intent, cmd.param);
            }
            xSemaphoreGive(s_state_mu);
        }

        // 2) 按当前时钟推进（在线逐分钟；锚定后通常每次只跨 0~1 分钟）。
        int32_t target = pet_clock_now_minute();
        xSemaphoreTake(s_state_mu, portMAX_DELAY);
        pt_advance_to(&s_state, &s_core_events, target);
        // P2-S3a：媒婆推荐/关系衰减/同住计时（可能产候选/婚礼/蛋事件）。
        uint8_t soc_ev_before = pt_events_count(&s_core_events);
        bool care_before = pt_social_care_active(&s_social);
        pt_social_on_minute(&s_social, &s_state, &s_core_events);
        // 育儿期增益跟随社交相位（到子代进入少年止）。
        bool care_after = pt_social_care_active(&s_social);
        if (care_before != care_after) {
            pt_engine_set_family_care(care_after);
        }
        if (pt_events_count(&s_core_events) != soc_ev_before) {
            touched = true;
        }
        if (s_econ_day != s_state.day_id) {
            // 清醒日界：玩具每日次数/职业班次重置（签到/货架只看 day_id，无需额外通知）。
            int32_t old_day = s_econ_day;
            s_econ_day = s_state.day_id;
            pt_econ_on_day(&s_econ);
            pt_jobs_on_day(&s_jobs);
            // S5：跨过的每个"每月 5 号"补一笔月初津贴（在线通常只跨 1 天；
            // 时钟跳跃跨多月也不漏发）。
            uint32_t stipend = 0;
            for (int32_t d = old_day + 1; d <= s_state.day_id; d += 1) {
                if (pt_econ_is_stipend_day(d)) {
                    stipend += pt_econ_add_coins(&s_econ,
                                                 PT_ECON_STIPEND_COINS);
                }
            }
            if (stipend > 0) {
                snprintf(s_notice, sizeof(s_notice), "Stipend +%luG!",
                         (unsigned long) stipend);
                s_notice_pending = true;
            }
            touched = true;
        }

        // 3) 引擎事件 → UI 队列 + 音效；喂食/玩具被拒则回补经济层的预扣。
        bool save_now = false;
        pt_event_t ev;
        while (pt_events_take(&s_core_events, &ev)) {
            dex_observe_event(&ev);
            if (ev.kind == PT_EV_SOC_PARENTS_LEAVE) {
                // 父母离队：50% G 币储蓄留给子代（08 §4）。
                s_econ.coins /= 2u;
                touched = true;
                save_now = true;
            }
            if (ev.kind == PT_EV_REFUSED) {
                bool want_food = (ev.a == PT_INTENT_FEED_MEAL
                                  || ev.a == PT_INTENT_FEED_SNACK);
                bool want_toy = (ev.a == PT_INTENT_PLAY_TOY);
                if (want_food || want_toy) {
                    // 退给第一个同种类的待裁决物品（同 tick 多次拒绝也一一对应）。
                    for (uint8_t k = 0; k < s_pending_n; k += 1) {
                        if (s_pending[k].toy != want_toy) {
                            continue;
                        }
                        pt_item_t pi = (pt_item_t) s_pending[k].item;
                        if (want_toy) {
                            pt_econ_refund_toy_use(&s_econ, pi);
                        } else {
                            (void) pt_econ_add(&s_econ, pi, 1);
                        }
                        touched = true;
                        s_pending[k] = s_pending[s_pending_n - 1];
                        s_pending_n -= 1;
                        break;
                    }
                }
            }
            if (map_sound(&ev)) {
                save_now = true;
            }
            (void) xQueueSend(s_ui_ev_q, &ev, 0);
        }
        s_pending_n = 0;
        // 图鉴里程碑贝壳奖（每个 tick 轮询，claimed 保证只发一次；也覆盖
        // 老档升级后的回填首发）。
        uint16_t dex_shells = 0;
        uint8_t miles = pt_dex_poll_rewards(&s_dex, &dex_shells);
        if (miles != 0) {
            pt_econ_grant_shells(&s_econ, dex_shells);
            snprintf(s_notice, sizeof(s_notice),
                     "Album milestone +%u shells!", (unsigned) dex_shells);
            s_notice_pending = true;
            touched = true;
        }
        xSemaphoreGive(s_state_mu);

        if (save_now) {
            do_save_now();
        } else {
            int64_t now_ms = pet_clock_uptime_ms();
            if (touched && s_dirty_at_ms == 0) {
                s_dirty_at_ms = now_ms;
            }
            // 脏位合并 10 秒：即使之后没有新操作也要落盘。
            if (s_dirty_at_ms != 0
                && now_ms - s_dirty_at_ms >= SAVE_MERGE_MS
                && now_ms - s_last_save_ms >= SAVE_MERGE_MS) {
                do_save_now();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void pet_app_start(bool audio_available)
{
    s_state_mu = xSemaphoreCreateMutex();
    s_cmd_q = xQueueCreate(INTENT_Q_DEPTH, sizeof(app_cmd_t));
    s_ui_ev_q = xQueueCreate(UI_EV_DEPTH, sizeof(pt_event_t));
    pt_events_init(&s_core_events);

    pet_store_init();

    bool loaded = pet_store_load(&s_state, &s_econ, &s_jobs, &s_decor,
                                 &s_med_unlocked, &s_volume);
    s_quiet = pet_store_quiet();
    pet_audio_set_volume(s_volume);
    pet_audio_start(audio_available);

    int32_t start_minute;
    if (loaded) {
        // P0 无电池 RTC：冷启动无法得知离线时长，从存档分钟继续（离线差按 0）。
        s_state.boot_count += 1;
        start_minute = s_state.minute;
        s_econ_day = s_state.day_id;
        ESP_LOGI(TAG, "save loaded: stage=%d minute=%d boots=%lu coins=%lu",
                 (int) s_state.stage, (int) s_state.minute,
                 (unsigned long) s_state.boot_count,
                 (unsigned long) s_econ.coins);
        // P2-S3a/b：社交第二 blob；v2=512B，兼容读取 S3a 的 v1=256B。
        uint8_t soc_blob[PET_SOC_SAVE_BYTES];
        bool soc_ok = pet_store_social_load(soc_blob, sizeof(soc_blob))
            && pet_soc_save_decode(&s_social, soc_blob, sizeof(soc_blob));
        if (!soc_ok) {
            uint8_t old_blob[PET_SOC_SAVE_BYTES_V1];
            if (pet_store_social_load(old_blob, sizeof(old_blob))
                && pet_soc_save_decode(&s_social, old_blob,
                                       sizeof(old_blob))) {
                ESP_LOGI(TAG, "social save migrated v1->v2");
            } else {
                pt_social_init(&s_social,
                               s_state.rng_state ^ 0x50C1A1u, s_state.day_id);
            }
        }
        // P2-S4：图鉴第三 blob；老档无记录时按当前宠/家谱回填。
        uint8_t dex_blob[PET_DEX_SAVE_BYTES];
        if (pet_store_dex_load(dex_blob, sizeof(dex_blob))
            && pet_dex_save_decode(&s_dex, dex_blob, sizeof(dex_blob))) {
            s_obs_species = s_state.species;
        } else {
            pt_dex_init(&s_dex);
            dex_backfill();
            ESP_LOGI(TAG, "dex new or unreadable, backfilled from family tree");
        }
    } else {
        start_minute = morning_eight_today(0);
        uint32_t seed = (uint32_t) esp_random()
            ^ (uint32_t) (pet_clock_uptime_ms() * 2654435761u)
            ^ 0x5a5a1234u;
        pt_state_new(&s_state, seed, start_minute);
        pt_econ_init(&s_econ, shelf_seed_from(seed));
        pt_jobs_init(&s_jobs);
        pt_decor_init(&s_decor);
        pt_social_init(&s_social, seed ^ 0x50C1A1u, s_state.day_id);
        pt_dex_init(&s_dex);
        s_obs_species = PT_SP_EGG;
        s_econ_day = s_state.day_id;
        ESP_LOGI(TAG, "no save, new egg at minute %d", (int) start_minute);
    }
    // 功能家具增益是引擎运行期参数：冷启动必须重新推送。
    decor_sync_engine();
    // P2-S3b：育儿期增益同为运行期参数，按社交存档恢复。
    pt_engine_set_family_care(pt_social_care_active(&s_social));
    pet_clock_anchor(start_minute);
    s_last_save_ms = pet_clock_uptime_ms();
    do_save_now();   // 首次也落一份（新蛋建 bank；读档则续指针）

    xTaskCreate(engine_task, "pet_engine", 4096, NULL, 4, NULL);

    // UI 在 LVGL 任务语境之外创建，按 AGENTS.md 持锁。
    if (bsp_lvgl_lock(2000)) {
        pet_ui_init();
        bsp_lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL lock timeout, UI not created");
    }
}

void pet_app_button(pet_btn_t btn, pet_ev_t ev)
{
    pet_ui_button(btn, ev);
}

bool pet_app_send_intent(pt_intent_kind_t kind, uint8_t param)
{
    app_cmd_t cmd = { .kind = CMD_INTENT, .intent = kind, .param = param };
    return xQueueSend(s_cmd_q, &cmd, 0) == pdTRUE;
}

void pet_app_new_game(void)
{
    app_cmd_t cmd = { .kind = CMD_NEW_GAME, .intent = 0, .param = 0,
                      .econ_sub = 0, .item = 0 };
    (void) xQueueSend(s_cmd_q, &cmd, 0);
}

// ---- L3 经济：UI → 引擎任务命令（全部非阻塞，单写者在引擎任务） ----

static bool send_econ(uint8_t sub, pt_item_t item)
{
    app_cmd_t cmd = { .kind = CMD_ECON, .intent = 0, .param = 0,
                      .econ_sub = sub, .item = (uint16_t) item };
    return xQueueSend(s_cmd_q, &cmd, 0) == pdTRUE;
}

void pet_app_econ_checkin(void)
{
    (void) send_econ(ECON_CHECKIN, PT_ITEM_NONE);
}

void pet_app_econ_buy(pt_item_t buy)
{
    (void) send_econ(ECON_BUY, buy);
}

void pet_app_econ_eat(pt_item_t item)
{
    (void) send_econ(ECON_EAT, item);
}

void pet_app_econ_play(pt_item_t item)
{
    (void) send_econ(ECON_PLAY, item);
}

// 小游戏完成（G1–G6 统一入口）：item 字段打包 PT_GAME_PACK(game_id, grade)。
void pet_app_game_result(uint8_t game_id, uint8_t grade)
{
    (void) send_econ(ECON_GAME,
                     (pt_item_t) PT_GAME_PACK(game_id, grade));
}

// ---- 职业（S3）------------------------------------------------------------

void pet_app_job_work(uint8_t job, uint8_t grade)
{
    (void) send_econ(ECON_JOB_WORK,
                     (pt_item_t) ((uint16_t) job * 4u + grade));
}

void pet_app_job_switch(uint8_t job)
{
    (void) send_econ(ECON_JOB_SWITCH, (pt_item_t) job);
}

bool pet_app_jobs_snapshot(pt_jobs_t *out)
{
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    *out = s_jobs;
    xSemaphoreGive(s_state_mu);
    return true;
}

// ---- 装饰收藏（S4）--------------------------------------------------------

static bool send_decor(uint8_t op, uint8_t arg)
{
    app_cmd_t cmd = { .kind = CMD_ECON, .intent = 0, .param = 0,
                      .econ_sub = ECON_DECOR_OP,
                      .item = (uint16_t) ((uint16_t) op << 8 | arg) };
    return xQueueSend(s_cmd_q, &cmd, 0) == pdTRUE;
}

bool pet_app_decor_snapshot(pt_decor_t *out)
{
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    *out = s_decor;
    xSemaphoreGive(s_state_mu);
    return true;
}

void pet_app_decor_buy(uint8_t kind, uint8_t id)
{
    uint8_t op = kind == 0 ? 1 : (kind == 1 ? 2 : 3);
    (void) send_decor(op, id);
}

void pet_app_decor_equip(uint8_t outfit)
{
    (void) send_decor(4, outfit);
}

void pet_app_decor_unequip(uint8_t slot)
{
    (void) send_decor(5, slot);
}

void pet_app_decor_place(uint8_t furniture)
{
    (void) send_decor(6, furniture);
}

void pet_app_decor_unplace(uint8_t furniture)
{
    (void) send_decor(7, furniture);
}

void pet_app_decor_set_theme(uint8_t theme)
{
    (void) send_decor(8, theme);
}

bool pet_app_econ_snapshot(pt_econ_t *out)
{
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    *out = s_econ;
    xSemaphoreGive(s_state_mu);
    return true;
}

bool pet_app_take_notice(char *out, uint8_t cap)
{
    if (out == NULL || cap == 0) {
        return false;
    }
    out[0] = '\0';
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    bool got = false;
    if (s_notice_pending) {
        snprintf(out, cap, "%s", s_notice);
        s_notice_pending = false;
        got = true;
    }
    xSemaphoreGive(s_state_mu);
    return got;
}

bool pet_app_snapshot(pt_state_t *out)
{
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    *out = s_state;
    xSemaphoreGive(s_state_mu);
    return true;
}

bool pet_app_take_event(pt_event_t *out)
{
    return xQueueReceive(s_ui_ev_q, out, 0) == pdTRUE;
}

uint8_t pet_app_volume(void)
{
    return s_volume;
}

void pet_app_set_volume(uint8_t level)
{
    if (level > 3) {
        return;
    }
    s_volume = level;
    pet_audio_set_volume(level);
    pet_store_set_volume(level);
}

bool pet_app_med_unlocked(void)
{
    return s_med_unlocked;
}

bool pet_app_quiet(void)
{
    return s_quiet;
}

void pet_app_set_quiet(bool enabled)
{
    s_quiet = enabled;
    pet_store_set_quiet(enabled);
}

// ---- P2-S3a 社交（08）：UI 快照/动作/结果槽，均非阻塞 ----------------------

bool pet_app_social_snapshot(pt_social_t *out)
{
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    *out = s_social;
    xSemaphoreGive(s_state_mu);
    return true;
}

static bool send_soc(uint8_t sub, uint8_t ci, uint16_t item)
{
    app_cmd_t cmd = { .kind = CMD_SOC, .intent = 0, .param = ci,
                      .econ_sub = 0, .soc_sub = sub, .item = item };
    return xQueueSend(s_cmd_q, &cmd, 0) == pdTRUE;
}

void pet_app_soc_greet(uint8_t ci)
{
    (void) send_soc(SOC_GREET, ci, 0);
}

void pet_app_soc_gift(uint8_t ci)
{
    (void) send_soc(SOC_GIFT, ci, 0);
}

void pet_app_soc_propose(uint8_t ci, uint8_t ring)
{
    (void) send_soc(SOC_PROPOSE, ci, ring);
}

void pet_app_soc_start_egg(void)
{
    (void) send_soc(SOC_START_EGG, 0, 0);
}

bool pet_app_take_soc_result(uint8_t *rv)
{
    bool had = false;
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (s_soc_rv_pending) {
            *rv = s_soc_rv;
            s_soc_rv_pending = false;
            had = true;
        }
        xSemaphoreGive(s_state_mu);
    }
    return had;
}

// ---- P2-S4 图鉴（04 §8.3 / 05 §8）：UI 只读快照 -----------------------------

bool pet_app_dex_snapshot(pt_dex_t *out)
{
    if (xSemaphoreTake(s_state_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    *out = s_dex;
    xSemaphoreGive(s_state_mu);
    return true;
}
