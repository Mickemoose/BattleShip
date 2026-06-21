/**
 * stage_registry.cpp - storage + accessors for the per-gkind dispatch
 * registry declared in stage_registry.h.
 *
 * Vector-backed, one row per gkind. Vanilla rows seeded at boot from the
 * decomp arrays via port_stage_seed_vanilla (implemented decomp-side in
 * decomp/src/gr/grport.c, which has access to the source arrays + types).
 * Synth rows registered via port_stage_register. Mirrors the structure of
 * fighter_registry.cpp.
 */

#include "stage_registry.h"

#include <cstring>
#include <memory>
#include <vector>

namespace {

using SDPtr = std::unique_ptr<StageDescriptor>;
std::vector<SDPtr> sRegistry;

/* Returns the row for gkind, or nullptr if out of range / unregistered. */
const StageDescriptor *get(int gkind)
{
    if (gkind < 0) return nullptr;
    if (static_cast<size_t>(gkind) >= sRegistry.size()) return nullptr;
    return sRegistry[static_cast<size_t>(gkind)].get();
}

} /* namespace */

extern "C" {

void port_stage_register(int gkind, const StageDescriptor *src)
{
    if (gkind < 0 || src == nullptr) return;

    if (static_cast<size_t>(gkind) >= sRegistry.size()) {
        sRegistry.resize(static_cast<size_t>(gkind) + 1);
    }

    auto row = std::make_unique<StageDescriptor>();
    std::memcpy(row.get(), src, sizeof(StageDescriptor));
    sRegistry[static_cast<size_t>(gkind)] = std::move(row);
}

const StageDescriptor *port_stage_descriptor(int gkind)
{
    return get(gkind);
}

intptr_t port_stage_map_file_id(int gkind)
{
    if (const auto *r = get(gkind)) return r->map_file_id;
    return -1;
}

intptr_t port_stage_map_header_offset(int gkind)
{
    if (const auto *r = get(gkind)) return r->map_header_offset;
    return 0;
}

PortStageMakeGroundFn port_stage_make_ground_proc(int gkind)
{
    if (const auto *r = get(gkind)) return r->make_ground_proc;
    return nullptr;
}

const char *port_stage_name(int gkind)
{
    if (const auto *r = get(gkind)) return r->name;
    return nullptr;
}

const char *port_stage_emblem_franchise(int gkind)
{
    if (const auto *r = get(gkind)) return r->emblem_franchise;
    return nullptr;
}

int port_stage_page(int gkind)
{
    if (const auto *r = get(gkind)) return r->page;
    return -1;
}

int port_stage_cell(int gkind)
{
    if (const auto *r = get(gkind)) return r->cell;
    return -1;
}

int port_stage_gkind_at(int page, int cell)
{
    for (size_t i = 0; i < sRegistry.size(); ++i) {
        const StageDescriptor *r = sRegistry[i].get();
        if (r != nullptr && r->page == page && r->cell == cell) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void port_stage_for_each(PortStageForEachFn cb, void *user)
{
    if (cb == nullptr) return;
    for (size_t i = 0; i < sRegistry.size(); ++i) {
        if (sRegistry[i] != nullptr) {
            cb(static_cast<int>(i), sRegistry[i].get(), user);
        }
    }
}

} /* extern "C" */
