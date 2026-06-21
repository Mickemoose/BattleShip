/**
 * fighter_registry.h - port-side per-fighter dispatch table.
 *
 * Decomp tables like dFTManagerDataFiles[] / dFTMainSpecialStatusDescs[] /
 * dFTCommonSpecialNStatusList[] are sized for the 27 vanilla FTKind values.
 * Synth fighters added by character mods have fkind
 * values past nFTKindEnumCount, so any unredirected `dFooArr[fkind]` site
 * OOBs and either crashes or pulls in adjacent data.
 *
 * This registry is the single redirection point. Every per-fkind dispatch
 * site in the decomp gets PORT-gated to read through one of the accessors
 * below. Vanilla rows are seeded once at boot from the decomp arrays; mods
 * call port_fighter_register() at MOD_INIT to add synth rows.
 */

#ifndef PORT_FIGHTER_REGISTRY_H
#define PORT_FIGHTER_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations of decomp types. Opaque from the registry's
 * perspective; full defs live under decomp/include/. */
struct FTData;
struct FTStatusDesc;
struct FTOpeningDesc;
struct FTCostume;
struct GObj;
struct FTStruct;

typedef void (*PortFTSpecialEnterFn)(struct GObj *);
typedef void (*PortFTEntryMakeEffectFn)(struct FTStruct *);
/* Per-fighter shield-hitlag override hook. Returns nonzero to zero the
 * fighter's hitlag_tics when a shielded hit lands (a custom fighter's
 * skip-hitlag-on-blocked-neutral-special pattern).
 * status_id is the fighter's current action; handlers match against
 * their character-specific blocked-NSP action ids. */
typedef int  (*PortFTShieldHitlagSkipFn)(struct GObj *, int status_id);
/* Per-fighter ECB resize hook. Engine calls this from ftMainSetStatus
 * just before the new action's ECB upper/middle radii get loaded so the
 * mod can substitute custom sizes (e.g. a custom fighter shrinking its
 * collision box while in a special state). Returns 1 if the descriptor wrote
 * new sizes via the two output args, 0 to leave the engine's defaults
 * alone. */
typedef int  (*PortFTECBOverrideFn)(struct FTStruct *fp, int next_status_id,
                                     float *out_upper, float *out_middle);
/* Per-grabber override for the ThrownDK mash/break-out interrupt. Called from
 * ftCommonCaptureShoulderedProcInterrupt with the GRABBING fighter's struct
 * (a2 = grabbing player struct). Return nonzero to skip the whole break-out
 * routine (the grabbed opponent can't mash free), 0 to run vanilla. One custom
 * fighter always returns 1; another (a Captain-clone) gates on the grabber's
 * ThrowF action. */
typedef int  (*PortFTCaptureDkInterruptFn)(struct FTStruct *grabber_fp);

enum {
    PORT_FIGHTER_SPECIAL_N      = 0,
    PORT_FIGHTER_SPECIAL_HI     = 1,
    PORT_FIGHTER_SPECIAL_LW     = 2,
    PORT_FIGHTER_SPECIAL_AIR_N  = 3,
    PORT_FIGHTER_SPECIAL_AIR_HI = 4,
    PORT_FIGHTER_SPECIAL_AIR_LW = 5,
    PORT_FIGHTER_SPECIAL_COUNT  = 6
};

enum {
    PORT_FIGHTER_ENTRY_APPEAR_R = 0,
    PORT_FIGHTER_ENTRY_APPEAR_L = 1
};

typedef struct FighterDescriptor {
    /* Files */
    struct FTData               *ft_data;

    /* Specials: per-fkind status descs (read at action_id >= 0xDC),
     * plus per-fkind enter handlers (N / Hi / Lw, ground + air). */
    struct FTStatusDesc         *special_descs;
    int                          special_descs_count;
    PortFTSpecialEnterFn         special_handler[PORT_FIGHTER_SPECIAL_COUNT];

    /* Match entry (status id picked at start of round + per-fkind effect). */
    int                          entry_appear_status[2];   /* [R, L] */
    PortFTEntryMakeEffectFn      entry_make_effect;        /* NULL = no effect */

    /* Opening / cutscene descs (per-fkind row pointer from the ovl1
     * D_ovl1_80390D20 table). */
    struct FTOpeningDesc        *opening_descs;

    /* Costumes (struct holds royal[4] / team[3] / develop). */
    struct FTCostume            *costume_row;

    /* Combat / damage */
    float                        scale;
    int                          skeleton_col_anim_base;

    /* Specific eats (Yoshi egg damage collision) */
    void                        *yoshi_egg_damage_coll;

    /* SFX */
    int                          down_bounce_fgm;
    int                          public_call_fgm;

    /* CPU */
    void                        *computer_attack_list;

    /* CSS presentation */
    int                          css_motion_special;
    void                        *css_attack1_motion_descs;

    /* CSS flashing-spotlight scale. The mnPlayers*SpotlightProcUpdate sizes[]
     * tables are 12-wide, so synth fkinds read this instead. 0 = unset; the
     * accessor falls back to 1.5 (every vanilla fighter except DK). */
    float                        css_spotlight_scale;

    /* Per-fighter engine-extension callbacks. NULL = no override. */
    PortFTShieldHitlagSkipFn     shield_hitlag_skip;
    PortFTECBOverrideFn          ecb_override;

    /* Kirby inhale-copy hat id. 0 = no special id
     * (fighter doesn't grant a special Kirby hat). */
    int                          kirby_hat_id;

    /* crowd_chant FGM id (announcer chants character
     * name on rally hit). 0 = use parent's default chant. */
    int                          crowd_chant_fgm;

    /* Per-action display-name string table. NULL =
     * no table (training-mode action overlay falls back to parent). */
    const char *const           *action_string_table;
    int                          action_string_table_count;
    int                          action_string_base_action_id;

    /* AI attack-prevent routine ptr. NULL = use
     * parent's. Signature: int(*)(struct FTStruct*, int input_kind) -
     * returns nonzero to skip committing that attack. */
    void                        *ai_attack_prevent_routine;

    /* Per-fkind CPU recovery hook
     * fired right after ftComputerFollowObjectiveWalk in the Recover
     * objective. NULL = parent's.
     * Signature: void(*)(struct FTStruct*). */
    void                        *ai_recovery_routine;

    /* Per-fkind hook to adjust
     * a candidate attack's selection weight (f2 =
     * current weight in/out). NULL = parent's. Signature:
     * float(*)(struct FTStruct*, int input_kind, float weight) - returns
     * the (possibly overridden) weight. */
    void                        *ai_attack_weight_routine;

    /* 1P-end BGM id. 0 = use parent's. */
    int                          remix_1p_end_bgm;

    /* Single-player ending-image file_id. 0 =
     * use parent's. */
    int                          remix_1p_ending_image_file_id;

    /* Default costumes table (per-fighter 8-byte
     * indices into the costume tex array, indexed by player port).
     * NULL = use parent's. */
    const unsigned char         *default_costumes;
    int                          default_costumes_count;

    /* Number of selectable costumes for the CSS
     * color cycle. Vanilla is capped at 4 (the 4 C-buttons map to royal[0..3]);
     * synths can have more (a custom fighter, e.g. 6). 0 = fall back to the
     * vanilla 4-button mapping. The costume value is the in-match mat-anim
     * frame index, so the model must carry this many costume frames. */
    int                          costume_count;

    /* Team costume index per team (3 entries: red,
     * green, blue / red, blue, yellow). 0xFF = unset. */
    unsigned char                team_costume[4];

    /* Charge-smash hold frame counts (3 entries:
     * forward, up, down). NULL = use parent's. */
    const unsigned char         *charge_smash_frames;

    /* When THIS fighter grabs an
     * opponent, the grabbed opponent plays this action instead of the usual
     * CapturePulled (0xAB). A custom fighter, e.g. ThrownDK (0xB8: opponent on
     * their back, struggling). 0 = no override (vanilla CapturePulled). Keyed
     * by the GRABBING fighter's fkind. */
    int                          custom_capture_action;

    /* Companion to custom_capture_action when the action is ThrownDK. Routine
     * deciding whether the grabbed opponent's mash/break-out is suppressed.
     * NULL = vanilla break-out. Keyed by the GRABBING fighter's fkind. */
    PortFTCaptureDkInterruptFn   custom_capture_dk_interrupt;

    /* This fighter (a Captain-clone custom fighter)
     * runs DK's grab->cargo ThrowF carry. ORed into the fkind==Donkey gate in
     * ftCommonThrowProcUpdate / ftCommonThrownProcUpdate so a normal grab's ThrowF
     * tail forks into the Cargo carry (status 0xEB). 0 = vanilla (only the true DK
     * family carries). Keyed by the GRABBING fighter's fkind. */
    int                          is_cargo_grabber;

    /* Per-fighter VS-results data.
     * A synth winner must use ITS OWN values here, never the parent's. The
     * decomp results consumers (announce / winner name / "WINS!" x) index
     * 12-entry vanilla tables that OOB on a synth fkind, so they read these
     * for fkind >= nFTKindEnumCount. results_name == NULL means a synth never
     * registered results data (a misconfiguration, not a Mario fallback). */
    int                          results_announce_fgm;   /* winner announce voice FGM */
    const char                  *results_name;           /* on-screen winner name string */
    float                        results_name_lx;         /* name string left-x */
    float                        results_name_scale;      /* name string x-scale */
    float                        results_wins_lx;         /* "WINS!" string left-x */

    /* The spinning series-emblem model rendered behind the winner. The three
     * per-character tables that the vanilla mnVSResultsMakeEmblem reads are
     * extended to swap the DObjDesc / MObjSub / MatAnimJoint offsets into the
     * grown FTEmblemModels file (reloc 0x23). These are byte offsets into that
     * file; CE owns and reloads the grown file, and publishes its current base
     * via port_set_results_emblem_base. results_emblem_valid == 0 means the
     * synth registered no emblem, so the results screen draws no emblem (never
     * the parent's). The swapped targets are MObjSub (zoom table) and
     * MatAnimJoint (color table). */
    int                          results_emblem_valid;    /* 1 if emblem offsets registered */
    unsigned int                 results_emblem_dobjdesc; /* series_logo offset */
    unsigned int                 results_emblem_mobjsub;  /* series_logo_zoom offset */
    unsigned int                 results_emblem_matanim;  /* series_logo_color offset */
} FighterDescriptor;

/* Register or replace a fighter's row. Resizes if fkind exceeds current
 * capacity. Caller-owned descriptor is shallow-copied. Registration is
 * MOD_INIT only -- not thread-safe for concurrent reads. */
void  port_fighter_register(int fkind, const FighterDescriptor *src);

/* Returns NULL if fkind out of range / unregistered. Field-specific
 * accessors below return safe defaults instead. */
const FighterDescriptor *port_fighter_descriptor(int fkind);

struct FTData              *port_fighter_data(int fkind);
struct FTStatusDesc        *port_fighter_special_descs(int fkind);
int                         port_fighter_special_descs_count(int fkind);
PortFTSpecialEnterFn        port_fighter_special_handler(int fkind, int kind);
int                         port_fighter_entry_appear(int fkind, int entry_id);
void                        port_fighter_entry_make_effect(int fkind, struct FTStruct *fp);
struct FTOpeningDesc       *port_fighter_opening_descs(int fkind);
struct FTCostume           *port_fighter_costume_row(int fkind);
float                       port_fighter_scale(int fkind);
int                         port_fighter_skeleton_col_anim_base(int fkind);
int                         port_fighter_down_bounce_fgm(int fkind);
int                         port_fighter_public_call_fgm(int fkind);
void                       *port_fighter_yoshi_egg_damage_coll(int fkind);
void                       *port_fighter_computer_attack_list(int fkind);

/* Engine-extension accessors. */
int                         port_fighter_shield_hitlag_skip(int fkind,
                                                             struct GObj *fighter_gobj,
                                                             int status_id);
int                         port_fighter_ecb_override(struct FTStruct *fp,
                                                       int next_status_id,
                                                       float *out_upper,
                                                       float *out_middle);
int                         port_fighter_kirby_hat_id(int fkind);

/* Per-player "pending custom hat id" carried from the inhale-eat path to the
 * copy-apply paths. The decomp stores only the copy POWER fkind in
 * status_vars.kirby.specialn.copy_id, then re-derives the hat at apply time as
 * copy[copy_id].copy_modelpart_id. That re-derivation collides for a synth: a
 * synth's power fkind is its parent (e.g. a Mario-parented synth), so
 * copy[Mario] yields Mario's vanilla hat (0x0C) instead of the synth's custom
 * hat (0x2A). When
 * Kirby eats a synth, the eat path records the synth's hat id here; the apply
 * sites (ftKirbySpecialNCopyInitCopyVars, ftManagerMakeFighter) read it back
 * and use the custom hat id when it is set (>= 0x0F). hat_id 0 clears it.
 *
 * Vanilla copies never set this (eat path only records for a mod-resolved
 * synth), so vanilla behavior is unchanged. Indexed by FTStruct.player.
 *
 * The slot itself lives in the Kirby-copy mod; these forward through handlers the
 * mod installs at MOD_INIT via port_kirby_register_pending_hat_handlers. Before
 * the mod registers (or after it exits) set is a no-op and get returns 0. */
typedef void (*PortKirbySetPendingHatFn)(int player, int hat_id);
typedef int  (*PortKirbyGetPendingHatFn)(int player);
void                        port_kirby_register_pending_hat_handlers(PortKirbySetPendingHatFn set,
                                                                     PortKirbyGetPendingHatFn get);
void                        port_kirby_set_pending_hat(int player, int hat_id);
int                         port_kirby_get_pending_hat(int player);

/* "Active copied-special fkind" override. When Kirby copies a SYNTH fighter
 * (copy_id >= nFTKindEnumCount), the synth's own neutral-B routine runs on
 * Kirby, which puts Kirby into the SYNTH's
 * special action id (>= 0xDC). That action's status descriptor lives in the
 * SYNTH's special_descs table, not Kirby's, so ftMainSetStatus must key its
 * special-desc lookup off the synth fkind for the duration of that call. The
 * copy-special dispatch (ftKirbySpecialN/AirNSetStatusSelect) sets this to the
 * copy_id right before invoking the synth handler and clears it (0/Null) after.
 * ftMainSetStatus consults it; when unset (default) the lookup uses fp->fkind
 * so Kirby's own specials and vanilla copies are unaffected. */
void                        port_kirby_set_copy_special_fkind(int fkind);
int                         port_kirby_get_copy_special_fkind(void);

/* Fighting-game-controls (FGC) engine bridge. The FightingGameEngine mod adds
 * a per-frame special-cancel / jab-rekka / tap-hold / proximity handler for
 * fighting-game-style custom fighters (patched into ftMainProcUpdateInterrupt)
 * plus a cancel-window opener in the hitlag-just-ended block. The two
 * forwarders below are called from those decomp sites under #ifdef PORT; they
 * no-op until the mod installs its handlers via port_fgc_register_handlers at
 * MOD_INIT, so vanilla fighters - and a build without the mod - are
 * unaffected. */
typedef void (*PortFgcHookFn)(struct GObj *fighter_gobj);
void                        port_fgc_register_handlers(PortFgcHookFn proc_interrupt,
                                                       PortFgcHookFn hitlag_end);
void                        port_fgc_proc_interrupt_hook(struct GObj *fighter_gobj);
void                        port_fgc_hitlag_end_hook(struct GObj *fighter_gobj);

int                         port_fighter_crowd_chant_fgm(int fkind);
const char                 *port_fighter_action_string(int fkind, int action_id);
void                       *port_fighter_ai_attack_prevent_routine(int fkind);
void                       *port_fighter_ai_recovery_routine(int fkind);
void                       *port_fighter_ai_attack_weight_routine(int fkind);
int                         port_fighter_remix_1p_end_bgm(int fkind);
int                         port_fighter_remix_1p_ending_image_file_id(int fkind);
const unsigned char        *port_fighter_default_costumes(int fkind, int *out_count);
int                         port_fighter_team_costume(int fkind, int team);
const unsigned char        *port_fighter_charge_smash_frames(int fkind);
int                         port_fighter_costume_count(int fkind);
float                       port_fighter_css_spotlight_scale(int fkind);

/* Custom capture-action accessors. _action returns the grabbed-opponent
 * action override for a grabber fkind (0 = none, use vanilla CapturePulled).
 * _dk_interrupt runs the grabber's break-out-skip routine and returns nonzero
 * to suppress the grabbed opponent's mash-out (0 if unregistered = vanilla). */
int                         port_fighter_custom_capture_action(int fkind);
int                         port_fighter_custom_capture_dk_interrupt(int fkind,
                                                                     struct FTStruct *grabber_fp);

/* Nonzero if this grabber fkind runs DK's grab->cargo ThrowF carry
 * (a Captain-clone custom fighter). 0 if unregistered = vanilla (DK-family only). */
int                         port_fighter_is_cargo_grabber(int fkind);

/* VS-results accessors. For a synth fkind the decomp results consumers read
 * these instead of indexing the 12-entry vanilla tables. _name returns NULL
 * if the synth registered no results data (caller skips / surfaces the gap;
 * it never falls back to the parent). */
int                         port_fighter_results_announce_fgm(int fkind);
const char                 *port_fighter_results_name(int fkind);
float                       port_fighter_results_name_lx(int fkind);
float                       port_fighter_results_name_scale(int fkind);
float                       port_fighter_results_wins_lx(int fkind);

/* VS-results spinning emblem (winner-logo). CE publishes the current base
 * of the loaded FTEmblemModels blob via port_set_results_emblem_base after
 * every scene reset (the blob's internal pointer tokens are generation-scoped,
 * so the base must be refreshed each time). For a synth that registered emblem
 * offsets, port_fighter_results_emblem resolves the three model pointers from
 * (base + offset) and returns 1; it returns 0 (drawing nothing) if the synth
 * registered no emblem or the base has not been published. */
void                        port_set_results_emblem_base(void *base);
int                         port_fighter_results_emblem(int fkind,
                                                        void **out_dobjdesc,
                                                        void **out_mobjsub,
                                                        void **out_matanim);

/* Walk every registered fkind in ascending order. Used by figatree-heap
 * sizing and other "iterate every fighter" loops that previously walked
 * the vanilla array length. */
typedef void (*PortFighterForEachFn)(int fkind, const FighterDescriptor *desc, void *user);
void  port_fighter_for_each(PortFighterForEachFn cb, void *user);

/* Seeds slots [0, 27) from the vanilla decomp arrays. Called once at
 * PortInit. Safe to call again -- existing synth rows past 27 are kept,
 * vanilla rows are overwritten. */
void  port_fighter_seed_vanilla(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_FIGHTER_REGISTRY_H */
