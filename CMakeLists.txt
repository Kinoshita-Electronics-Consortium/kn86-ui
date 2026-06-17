cmake_minimum_required(VERSION 3.16)
project(kn86_ui C)

#
# kn86-ui — the KN-86 Fe component kit (source of truth).
#
# This repo owns the ui/ component library (frames, glyphs, compositing,
# lists, data, input) authored in KEC Lisp against the nOSh render + cell
# API contract. It is vendored into nOSh / kn86-sdk / kn86-carts via each
# consumer's own sync.sh.
#
# Standalone CI builds the components against a FROZEN render substrate
# shim (vendor/nosh-substrate/) so this repo can prove the kit green
# without the full nOSh runtime existing yet. The shim's source flips from
# the kn86-deckline monorepo to the standalone nOSh repo in P4 — see
# vendor/nosh-substrate/sync.sh.
#
# The component sources themselves (ui/*.lsp) are the deliverable; the
# substrate is build scaffolding only.
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

set(SUBSTRATE ${CMAKE_SOURCE_DIR}/vendor/nosh-substrate)
set(KEC_DIR ${SUBSTRATE}/kec-lisp)

# ---------------------------------------------------------------------------
# Vendored kec-lisp (the Fe VM the components run on). Same codegen the
# kn86-deckline monorepo uses: mkembed bakes KEC Core (.lsp) into a C string
# literal that runtime/kec.c #includes (kec_core_embed.h).
# ---------------------------------------------------------------------------
set(KEC_GEN_DIR ${CMAKE_BINARY_DIR}/kec_generated)
file(MAKE_DIRECTORY ${KEC_GEN_DIR})

add_executable(kec_mkembed ${KEC_DIR}/tools/mkembed.c)
set_target_properties(kec_mkembed PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

# Core modules — dependency order (def first, str last).
set(KEC_CORE_SRCS
    ${KEC_DIR}/core/00-def.lsp
    ${KEC_DIR}/core/10-list.lsp
    ${KEC_DIR}/core/20-cmp.lsp
    ${KEC_DIR}/core/30-pred.lsp
    ${KEC_DIR}/core/40-ctrl.lsp
    ${KEC_DIR}/core/50-hof.lsp
    ${KEC_DIR}/core/60-str.lsp)

add_custom_command(
    OUTPUT ${KEC_GEN_DIR}/kec_core_embed.h
    COMMAND kec_mkembed ${KEC_GEN_DIR}/kec_core_embed.h KEC_CORE_SRC ${KEC_CORE_SRCS}
    DEPENDS kec_mkembed ${KEC_CORE_SRCS}
    COMMENT "Embedding KEC Core")

# The linkable Fe stack (mirrors the monorepo's FE_SRC).
set(FE_SRC
    ${KEC_DIR}/kernel/fe.c
    ${KEC_DIR}/host/host.c
    ${KEC_DIR}/runtime/kec.c
    ${KEC_GEN_DIR}/kec_core_embed.h)

# ---------------------------------------------------------------------------
# The render substrate the headless component tests link (no SDL3). These
# are the C files sys_context.c (System context lifecycle) +
# sys_render.c (binds render/* primitives) + render.c / phosphor.c /
# font.c / cell_api.c (the in-memory render surface the golden buffer reads).
# ---------------------------------------------------------------------------
set(SUBSTRATE_SRCS
    ${SUBSTRATE}/sys_render.c
    ${SUBSTRATE}/sys_context.c
    ${SUBSTRATE}/render.c
    ${SUBSTRATE}/phosphor.c
    ${SUBSTRATE}/font.c
    ${SUBSTRATE}/cell_api.c
    ${FE_SRC})

# ---------------------------------------------------------------------------
# Stage the lib tree the golden-buffer harness loads at runtime. ui_golden.h
# reads "UI_LIB_DIR/render.lsp" + "UI_LIB_DIR/ui/<file>.lsp", so we assemble
# a build-tree lib dir holding render.lsp (from the substrate) + the ui/
# component sources, and bake UI_LIB_DIR to it. Keeps the harness header
# byte-identical to the monorepo's.
# ---------------------------------------------------------------------------
set(UI_LIB_STAGE ${CMAKE_BINARY_DIR}/lib)
add_custom_target(ui_lib_stage ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${UI_LIB_STAGE}/ui
    COMMAND ${CMAKE_COMMAND} -E copy ${SUBSTRATE}/render.lsp ${UI_LIB_STAGE}/render.lsp
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/ui ${UI_LIB_STAGE}/ui
    COMMENT "Staging render.lsp + ui/*.lsp for the golden-buffer harness")

# ---------------------------------------------------------------------------
# Component tests — each loads render/ + ui/ into a System context, evals a
# component form, and asserts the in-memory render surface glyph-by-glyph
# (tests/ui_golden.h). Split by concern: frames+chrome+compositing,
# lists+data, nav+input.
# ---------------------------------------------------------------------------
enable_testing()
foreach(ui_test test_ui_frames test_ui_lists_data test_ui_input)
    add_executable(${ui_test} tests/${ui_test}.c ${SUBSTRATE_SRCS})
    add_dependencies(${ui_test} ui_lib_stage)
    target_include_directories(${ui_test} PRIVATE
        tests/
        ${SUBSTRATE}
        ${KEC_DIR}/kernel
        ${KEC_DIR}/host
        ${KEC_DIR}/runtime
        ${KEC_GEN_DIR})
    target_compile_definitions(${ui_test} PRIVATE
        UI_LIB_DIR="${UI_LIB_STAGE}")
    # host.c (the Fe VM's C primitives) uses libm (pow/sqrtf/fmod). macOS
    # folds libm into libc, but GNU ld needs it explicitly — link it so the
    # Linux CI leg links.
    target_link_libraries(${ui_test} m)
    set_target_properties(${ui_test} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME ${ui_test} COMMAND ${ui_test})
endforeach()

# ---------------------------------------------------------------------------
# render_screen — DSL preview tool. Loads render/ + ui/ + ui/screen.lsp,
# evals a screen .lsp (which calls ui/draw), and dumps the real RGB565
# surface to a PPM. Same renderer + font the panel uses; no SDL.
# ---------------------------------------------------------------------------
add_executable(render_screen tools/render_screen.c ${SUBSTRATE_SRCS})
add_dependencies(render_screen ui_lib_stage)
target_include_directories(render_screen PRIVATE
    tests/
    ${SUBSTRATE}
    ${KEC_DIR}/kernel
    ${KEC_DIR}/host
    ${KEC_DIR}/runtime
    ${KEC_GEN_DIR})
target_compile_definitions(render_screen PRIVATE UI_LIB_DIR="${UI_LIB_STAGE}")
target_link_libraries(render_screen m)
set_target_properties(render_screen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
