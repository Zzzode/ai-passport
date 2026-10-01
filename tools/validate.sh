#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_demo_navigation.c main/demo_navigation.c \
        -o "${test_dir}/test_demo_navigation"
    "${test_dir}/test_demo_navigation"
    # PasPet 宠物应用：纯逻辑内核的主机测试（见 designs/Tomagotchi）。
    pet_core_srcs=(
        main/pet_core/pt_engine.c
        main/pet_core/pt_evolve.c
        main/pet_core/pt_genome.c
        main/pet_core/pt_rng.c
        main/pet_core/pt_events.c
        main/pet_core/pt_config.c
    )
    for pet_test in rng core timeline; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core \
            "tests/test_pt_${pet_test}.c" "${pet_core_srcs[@]}" \
            -o "${test_dir}/test_pt_${pet_test}"
        "${test_dir}/test_pt_${pet_test}"
    done
    # 进化分档/物种表/隐藏角色/寿命掷定为纯函数，只需 pt_evolve+pt_rng+pt_config。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core \
        tests/test_pt_evolve.c main/pet_core/pt_evolve.c main/pet_core/pt_rng.c \
        main/pet_core/pt_config.c \
        -o "${test_dir}/test_pt_evolve"
    "${test_dir}/test_pt_evolve"
    # P2-S1 L2 基因/遗传：目录/稀有度/育种概率表/亲和/纯血/性格/确定性（白盒含 .c）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core \
        tests/test_pt_genome.c main/pet_core/pt_rng.c \
        -o "${test_dir}/test_pt_genome"
    "${test_dir}/test_pt_genome"
    # P2-S3a 离线社交：媒婆推荐/冷却/衰减/送礼/求婚接受率/婚礼/同住育种（纯逻辑）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core \
        tests/test_pt_social.c main/pet_core/pt_social.c \
        "${pet_core_srcs[@]}" \
        -o "${test_dir}/test_pt_social"
    "${test_dir}/test_pt_social"
    # P2-S2 部件管线：灰度 PNG -> RLE C 表（零三方依赖），含字节稳定/预算校验。
    python3 tools/pet_assets/build_parts.py --out "${test_dir}/pet_gen"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/pet_assets/test_pipeline.py
    # PV1 UI 图标管线：RGBA PNG → ARGB8888 BGRA C 表/尺寸/预算校验。
    python3 tools/pet_assets/build_icons.py --out "${test_dir}/pet_gen" >/dev/null
    PYTHONDONTWRITEBYTECODE=1 python3 tools/pet_assets/test_icons_pipeline.py
    # P2-S2 运行时合成：RLE 解码/256 项 LUT/5 层叠加/真实 48 部件穷举/坏流韧性。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core \
        -I"${test_dir}/pet_gen" \
        tests/test_pet_compose.c main/pet_core/pt_compose.c \
        main/pet_core/pt_genome.c main/pet_core/pt_rng.c \
        "${test_dir}/pet_gen/pet_parts_data.c" \
        -o "${test_dir}/test_pet_compose"
    "${test_dir}/test_pet_compose"
    # pet_app 纯逻辑：存档编解码往返（测试同时驱动引擎，需要 pet_core 源码）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_save.c main/pet_app/pet_save.c main/pet_app/pet_econ.c \
        main/pet_app/pet_jobs.c main/pet_app/pet_decor.c \
        "${pet_core_srcs[@]}" \
        -o "${test_dir}/test_pet_save"
    "${test_dir}/test_pet_save"
    # P2-S3a 社交存档：独立第二 blob 的 magic/version/CRC/往返/损坏拒绝。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_social_save.c main/pet_app/pet_social_save.c \
        main/pet_core/pt_social.c "${pet_core_srcs[@]}" \
        -o "${test_dir}/test_pet_social_save"
    "${test_dir}/test_pet_social_save"
    # P2-S4 图鉴：物种三级点亮/部件三标记/纯血链/里程碑发奖（纯逻辑）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core \
        tests/test_pt_dex.c main/pet_core/pt_dex.c main/pet_core/pt_social.c \
        "${pet_core_srcs[@]}" \
        -o "${test_dir}/test_pt_dex"
    "${test_dir}/test_pt_dex"
    # P2-S4 图鉴存档：第三 blob PET3 的 magic/version/CRC/字段合法性。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_dex_save.c main/pet_app/pet_dex_save.c \
        main/pet_core/pt_dex.c main/pet_core/pt_social.c "${pet_core_srcs[@]}" \
        -o "${test_dir}/test_pet_dex_save"
    "${test_dir}/test_pet_dex_save"
    # P1-S4 换装/家具/主题：购买/穿戴/摆放/折扣/增益纯逻辑。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_decor.c main/pet_app/pet_decor.c main/pet_app/pet_econ.c \
        main/pet_core/pt_rng.c \
        -o "${test_dir}/test_pet_decor"
    "${test_dir}/test_pet_decor"
    # P1 经济底座：签到/货架/购买/库存纯逻辑。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_econ.c main/pet_app/pet_econ.c main/pet_core/pt_rng.c \
        -o "${test_dir}/test_pet_econ"
    "${test_dir}/test_pet_econ"
    # P1-S5 30 天经济仿真：津贴/软税/收支带（真实收支原语，三画像）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_econ_sim.c main/pet_app/pet_econ.c \
        main/pet_app/pet_jobs.c main/pet_core/pt_rng.c \
        -o "${test_dir}/test_pet_econ_sim"
    "${test_dir}/test_pet_econ_sim"
    # P1-S3 职业：门槛/工资/班次/月限切换/劳模贴纸纯逻辑。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_jobs.c main/pet_app/pet_jobs.c \
        -o "${test_dir}/test_pet_jobs"
    "${test_dir}/test_pet_jobs"
    # G1 猜大小纯逻辑（自带 RNG，无其他依赖）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_app \
        tests/test_pet_game.c main/pet_app/pet_game.c \
        -o "${test_dir}/test_pet_game"
    "${test_dir}/test_pet_game"
    # G2–G6 纯逻辑内核（评级阈值/确定性回放，只用 pt_config 宏）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/pet_core -Imain/pet_app \
        tests/test_pet_games2.c main/pet_app/pet_games2.c \
        -o "${test_dir}/test_pet_games2"
    "${test_dir}/test_pet_games2"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_display_rounding.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_display_rounding"
    "${test_dir}/test_bsp_display_rounding"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_es8311_sleep_check.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_es8311_sleep_check"
    "${test_dir}/test_bsp_es8311_sleep_check"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_button.c -o "${test_dir}/test_bsp_button"
    "${test_dir}/test_bsp_button"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_lvgl_init.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_lvgl_init"
    "${test_dir}/test_bsp_lvgl_init"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/audio_stubs -Icomponents/bsp/include -Icomponents/bsp/src \
        tests/test_bsp_audio_recovery.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_audio_recovery"
    "${test_dir}/test_bsp_audio_recovery"
    # macOS 的 ld64 不认识 GNU 的 --gc-sections，用等价的 -dead_strip。
    gc_sections="-Wl,--gc-sections"
    if [[ "$(uname -s)" == "Darwin" ]]; then
        gc_sections="-Wl,-dead_strip"
    fi
    for demo in audio low_power ble wifi; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
            -ffunction-sections -fdata-sections -Itests/demo_stubs -Imain \
            "tests/test_demo_${demo}_runtime.c" "${gc_sections}" \
            -o "${test_dir}/test_demo_${demo}_runtime"
        "${test_dir}/test_demo_${demo}_runtime"
    done
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_deep_sleep_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_check_repo.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_verify_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_archive_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_install_passport_skills.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/archive_firmware.py create \
        "${validation_build_dir}" --archive-root "${repo_root}/build/firmware"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
