/**
 * stage_registry.h - port-side per-stage (gkind) dispatch table.
 *
 * Decomp tables like dMPCollisionGroundFileInfos[] and
 * dGRMainSetupProcMakeList[] are sized for the fixed GRKind enum
 * (VS stages 0..8, plus the bonus/1P stages up to nGRKindBonus2End=40).
 * A synth stage registered past that range takes a gkind value past it,
 * so any unredirected `dFooArr[gkind]` site OOBs and either crashes or
 * pulls in adjacent data -- the exact analog of the synth-fkind OOB
 * problem fighter_registry.{h,cpp} solves.
 *
 * This registry is the single redirection point for the gameplay-side
 * per-gkind dispatch. Every per-gkind site in the decomp gets PORT-gated
 * to read through one of the accessors below. Vanilla rows are seeded once
 * at boot from the decomp arrays (port_stage_seed_vanilla, implemented
 * decomp-side in decomp/src/gr/grport.c where the source arrays + types
 * are visible); synth rows are added via port_stage_register().
 *
 * SCOPE: covers the gameplay-load dispatch (map-file lookup + per-stage
 * make-ground proc). The stage-select-screen tables (mnmaps.c
 * icon/name/emblem/preview arrays + cursor nav + random rollers) are a
 * separate set of fields added later; this struct grows additively, so
 * seeding/registration stay forward-compatible.
 */

#ifndef PORT_STAGE_REGISTRY_H
#define PORT_STAGE_REGISTRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of the decomp GObj type. Opaque here; the make-ground
 * procs (grCastleMakeGround etc.) are GObj*(*)(void). */
struct GObj;

/* Per-stage make-ground proc. Mirrors dGRMainSetupProcMakeList[]'s element
 * type (GObj*(*)(void)). NULL = the gkind has no proc-list entry (a vanilla
 * bonus/1P stage routed through grMainSetupMakeGround's else-branches). */
typedef struct GObj *(*PortStageMakeGroundFn)(void);

typedef struct StageDescriptor {
    /* Gameplay map file: dMPCollisionGroundFileInfos[gkind] = {file_id, offset}.
     * file_id is the reloc file id of the stage's GR*Map header file; offset is
     * the byte offset of the MPGroundData header inside it. */
    intptr_t                map_file_id;
    intptr_t                map_header_offset;

    /* Per-stage setup proc: dGRMainSetupProcMakeList[gkind]. NULL = none. */
    PortStageMakeGroundFn   make_ground_proc;

    /* --- Stage-select (mnmaps) fields. Default-safe: vanilla rows leave these
     * zero/NULL/-1 and keep using their own ROM tables; a SYNTH stage sets them
     * so its select-screen cell is fully data-driven, with no per-stage
     * hardcoding in mnmaps.c. A loose stage mod fills these from
     * stage_info.yaml via port_stage_register. --- */
    const char             *name;             /* CSS nameplate text; NULL = none */
    const char             *emblem_franchise; /* FranchiseEngine id for the cell emblem; NULL = blank */
    int32_t                 page;             /* stage-select page index; -1 = not placed on the grid */
    int32_t                 cell;             /* slot index on that page; -1 = not placed */
} StageDescriptor;

/* Register or replace a stage's row. Resizes if gkind exceeds current
 * capacity. Caller-owned descriptor is shallow-copied. Registration is
 * startup-only -- not thread-safe for concurrent reads. */
void  port_stage_register(int gkind, const StageDescriptor *src);

/* Returns NULL if gkind out of range / unregistered. Field accessors below
 * return safe defaults instead. */
const StageDescriptor *port_stage_descriptor(int gkind);

/* Gameplay-load accessors. Vanilla gkinds return the seeded values
 * (identical to the original array reads); synth gkinds return their
 * registered values; unregistered gkinds return safe sentinels. */
intptr_t              port_stage_map_file_id(int gkind);
intptr_t              port_stage_map_header_offset(int gkind);
PortStageMakeGroundFn port_stage_make_ground_proc(int gkind);

/* Stage-select (mnmaps) accessors. Return NULL/-1 for vanilla or unset rows;
 * mnmaps.c falls back to its own ROM tables in that case, so the vanilla
 * select-screen path is unchanged. A synth stage that set these drives its
 * cell entirely from the registry. */
const char *port_stage_name(int gkind);             /* NULL = use ROM/none */
const char *port_stage_emblem_franchise(int gkind); /* NULL = blank emblem */
int         port_stage_page(int gkind);             /* -1 = not placed */
int         port_stage_cell(int gkind);             /* -1 = not placed */

/* Reverse grid lookup: the registered synth gkind placed at (page, cell), or
 * -1 if no synth row occupies that slot. Lets mnmaps overlay synth cells onto
 * the vanilla page grid without a per-stage table edit. */
int         port_stage_gkind_at(int page, int cell);

/* Walk every registered gkind in ascending order. */
typedef void (*PortStageForEachFn)(int gkind, const StageDescriptor *desc, void *user);
void  port_stage_for_each(PortStageForEachFn cb, void *user);

/* Seeds the vanilla gkind rows from the decomp arrays. Called once at
 * PortInit. Safe to call again -- registered rows past the vanilla range
 * are kept, vanilla rows are overwritten. */
void  port_stage_seed_vanilla(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_STAGE_REGISTRY_H */
