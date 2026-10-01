# L2 部件艺术：灰度 PNG + manifest → RLE C 表（designs 05 §5.1，零三方依赖）。
set(PET_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/pet")
set(PET_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/pet_gen")

idf_component_register(
    SRCS "main.c"
         "pet_core/pt_engine.c"
         "pet_core/pt_evolve.c"
         "pet_core/pt_genome.c"
         "pet_core/pt_compose.c"
         "pet_core/pt_rng.c"
         "pet_core/pt_events.c"
         "pet_core/pt_config.c"
         "pet_core/pt_social.c"
         "pet_core/pt_dex.c"
         "pet_app/pet_app.c"
         "pet_app/pet_store.c"
         "pet_app/pet_save.c"
         "pet_app/pet_social_save.c"
         "pet_app/pet_dex_save.c"
         "pet_app/pet_input.c"
         "pet_app/pet_dock.c"
         "pet_app/pet_clock.c"
         "pet_app/pet_audio.c"
         "pet_app/pet_game.c"
         "pet_app/pet_games2.c"
         "pet_app/pet_jobs.c"
         "pet_app/pet_econ.c"
         "pet_app/pet_decor.c"
         "pet_app/pet_ui.c"
         "pet_app/pet_ui_icons.c"
    INCLUDE_DIRS "." "pet_core" "pet_app"
    REQUIRES bsp esp_timer nvs_flash
)

# IDF 需求发现以 script 模式执行本文件，构建期命令必须整段跳过。
if(NOT CMAKE_SCRIPT_MODE_FILE)
file(GLOB_RECURSE PET_PART_PNGS "${PET_ASSET_DIR}/*.png")
file(MAKE_DIRECTORY "${PET_GEN_DIR}")
add_custom_command(
    OUTPUT "${PET_GEN_DIR}/pet_parts_data.c" "${PET_GEN_DIR}/pet_parts_data.h"
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/pet_assets/build_parts.py"
            --root "${PET_ASSET_DIR}" --out "${PET_GEN_DIR}"
    DEPENDS "${PET_ASSET_DIR}/manifest.json"
            ${PET_PART_PNGS}
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/pet_assets/build_parts.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/pet_assets/png_io.py"
    COMMENT "Generating PasPet part table"
    VERBATIM)
add_custom_target(pet_parts_gen DEPENDS "${PET_GEN_DIR}/pet_parts_data.c")
add_dependencies(${COMPONENT_LIB} pet_parts_gen)
target_sources(${COMPONENT_LIB} PRIVATE "${PET_GEN_DIR}/pet_parts_data.c")
target_include_directories(${COMPONENT_LIB} PRIVATE "${PET_GEN_DIR}")

# PV1 UI 图标：RGBA PNG + manifest → ARGB8888 C 表（零三方依赖）。
file(GLOB_RECURSE PET_UI_ICON_PNGS "${PET_ASSET_DIR}/ui-icons/*.png")
add_custom_command(
    OUTPUT "${PET_GEN_DIR}/pet_ui_icons_data.c" "${PET_GEN_DIR}/pet_ui_icons_data.h"
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/pet_assets/build_icons.py"
            --root "${PET_ASSET_DIR}" --out "${PET_GEN_DIR}"
    DEPENDS "${PET_ASSET_DIR}/ui-icons/manifest.json"
            ${PET_UI_ICON_PNGS}
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/pet_assets/build_icons.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/pet_assets/png_rgba.py"
    COMMENT "Generating PasPet UI icon tables"
    VERBATIM)
add_custom_target(pet_ui_icons_gen DEPENDS "${PET_GEN_DIR}/pet_ui_icons_data.c")
add_dependencies(${COMPONENT_LIB} pet_ui_icons_gen)
target_sources(${COMPONENT_LIB} PRIVATE "${PET_GEN_DIR}/pet_ui_icons_data.c")
endif()
