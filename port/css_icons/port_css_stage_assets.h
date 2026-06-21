#ifndef PORT_CSS_STAGE_ASSETS_H
#define PORT_CSS_STAGE_ASSETS_H

/**
 * port_css_stage_assets.h — Generic CSS stage asset loader (port-only).
 *
 * Replaces the old per-stage port_css_fd_icon.h / port_css_fd_background.h.
 * Assets are loaded at runtime from PNG files extracted from baserom.us.z64 by
 * tools/derive_stage_assets.py (run at CMake build time). No Nintendo-derived
 * bytes are baked into the binary.
 *
 * Runtime file layout (under <app-data>/assets/css_icons/):
 *   final_destination_background.png  — 300x220 RGBA, 44-bitmap RGBA16 sprite
 *   final_destination_small.png       — 48x36  RGBA, 1-bitmap  RGBA16 sprite
 *   final_destination_name.png        — 96x10  RGBA, 1-bitmap  RGBA16 sprite
 *                                        (synthesized — matches ROM IA4 nameplate dims)
 *   final_destination_emblem.png      — 64x48  RGBA, 1-bitmap  RGBA16 sprite
 *                                        (upscaled from 25x25 IA4 MasterHand icon)
 *
 * If a PNG is absent (user hasn't run a ROM-extract build), the getter returns
 * NULL and the caller should fall back to the ROM-resident sprite.
 *
 * Adding a new port-introduced stage:
 *   1. Add an entry to STAGE_TABLE in port_css_stage_assets.cpp.
 *   2. Add the matching entry to STAGES in tools/derive_stage_assets.py.
 *   3. Add the CMake OUTPUT + DEPENDS lines in CMakeLists.txt (copy the FD block).
 *   That is the entire change — no other C++ files need editing.
 *
 * Nameplate sprite (portCSSGetStageNameSprite):
 *   Dimensions: 96x10 — matches ROM IA4 nameplate sprites (file 30/MNMaps).
 *   Format: RGBA16 (same as background/icon — convertToRGBA16BE applied at load).
 *   Only stages with a synthesized nameplate PNG expose a non-NULL return.
 *   Stages shipping a ROM nameplate (the 9 starter stages) return NULL; the
 *   caller falls back to the ROM-resident sprite via lbRelocGetFileData.
 *   The SObj caller must set red=green=blue=0x00 (as done for ROM nameplates)
 *   so the sprite renders as black text on transparent.
 *
 * Usage in mnmaps.c (after the main thread wires up call sites):
 *
 *   #ifdef PORT
 *   #include <port/css_icons/port_css_stage_assets.h>
 *   ...
 *   Sprite *bg     = portCSSGetStageBackgroundSprite(nGRKindLast);
 *   Sprite *icon   = portCSSGetStageIconSprite(nGRKindLast);
 *   Sprite *emblem = portCSSGetStageEmblemSprite(nGRKindLast);
 *   #endif
 *
 * Both getters are thread-safe.  The first call loads + converts the PNG;
 * subsequent calls re-register fixup tracking (scene-boundary safety) and
 * return the cached pointer.  Returns NULL on disk-miss or alloc failure.
 *
 * Sprite field byte offsets (native LE, post-portMarkSyntheticSprite):
 *   see port_css_fd_icon.cpp header comment for the full table.
 */

#ifdef PORT

#ifdef __cplusplus
extern "C" {
#endif

struct sprite;
typedef struct sprite Sprite;

/**
 * Return the full-resolution CSS wallpaper Sprite for the given gkind.
 *
 * For nGRKindLast (16) this is a 300x220 RGBA16 sprite with 44 bitmaps of
 * 5 rendered rows each, matching dStageLastBackground_0x26c88.
 *
 * Thread-safe. Returns NULL if the PNG is missing or allocation fails.
 */
Sprite *portCSSGetStageBackgroundSprite(int gkind);

/**
 * Return the 48x36 CSS icon Sprite for the given gkind.
 *
 * Thread-safe. Returns NULL if the PNG is missing or allocation fails.
 */
Sprite *portCSSGetStageIconSprite(int gkind);

/**
 * Return the 96x10 nameplate Sprite for the given gkind.
 *
 * Only port-introduced stages that have a synthesized nameplate PNG return
 * non-NULL.  Stages that ship a ROM-resident IA4 nameplate (the 9 starter
 * stages in reloc file 30 / MNMaps) return NULL so the caller falls back to
 * the ROM sprite via lbRelocGetFileData.
 *
 * The caller must set sobj->sprite.red = sobj->sprite.green =
 * sobj->sprite.blue = 0x00 (matching the ROM nameplate draw style).
 *
 * Thread-safe. Returns NULL if the PNG is missing or allocation fails.
 */
Sprite *portCSSGetStageNameSprite(int gkind);

/**
 * Return the 64x48 emblem Sprite for the given gkind.
 *
 * For port-introduced stages (e.g. nGRKindLast/Final Destination) where the
 * ROM ships only a small source sprite, this returns a 64x48 LANCZOS-upscaled
 * version derived at build time by tools/derive_stage_assets.py.
 *
 * The caller should set sprite colour modulation (red/green/blue) to match the
 * franchise colour used by the other ROM emblems (0x5C, 0x22, 0x00).
 *
 * Thread-safe. Returns NULL if the PNG is missing or allocation fails; the
 * caller should fall back to the ROM-resident small sprite in that case.
 */
Sprite *portCSSGetStageEmblemSprite(int gkind);

/**
 * Runtime stage-asset registration — the runtime analog of appending a
 * STAGE_TABLE entry, so a mod (e.g. the stage mod) can add OR override a
 * stage's CSS preview assets at MOD_INIT with NO host rebuild.
 *
 * After registration the four getters above return this stage's sprites,
 * loaded from PNGs at:
 *   <app-data>/<base_dir>/<name>_{background,small,name,emblem}.png
 * base_dir lets a mod point at its own folder (e.g. "mods/MyStage") so its
 * converted/loose PNGs load directly; NULL or "" falls back to the
 * build-extracted "assets/css_icons". Strings are copied.
 *
 * Thread-safe; intended for MOD_INIT (register before the CSS screen first
 * renders). Re-registering a gkind replaces its descriptor and drops any
 * cached sprites so the new assets reload.
 *
 * NOTE: this supplies the stage's CSS TEXTURES only. A brand-new gkind also
 * needs a gameplay map file (port_stage_register) and a stage-select grid cell
 * (the mnmaps page-layout path) before it is actually selectable.
 */
typedef struct PortCSSStageAssetDesc {
    int         gkind;          /* GRKind value */
    const char *name;           /* filename stem, e.g. "customstage" */
    const char *base_dir;       /* PNG dir rel to app dir; NULL/"" => "assets/css_icons" */
    int         bg_w, bg_h;     /* background full dimensions (pixels) */
    int         bg_nbitmaps;    /* TMEM-slice bitmap count */
    int         bg_bm_h;        /* rendered rows per bitmap */
    int         bg_bm_hreal;    /* N64 bmHreal (TMEM-load rows incl. fringe) */
    int         bg_ndisplist;   /* ndisplist from the sprite */
    int         has_name_png;   /* nonzero if a <name>_name.png is shipped */
    int         has_emblem_png; /* nonzero if a <name>_emblem.png is shipped */
} PortCSSStageAssetDesc;

void portCSSRegisterStageAssets(const PortCSSStageAssetDesc *desc);

#ifdef __cplusplus
}
#endif

#endif /* PORT */

#endif /* PORT_CSS_STAGE_ASSETS_H */
