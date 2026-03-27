# Mojave

Mojave is a pure C99 project focused on building a generic top-down 2D runtime for data-driven RPGs. Its purpose is to provide a clean, efficient, and reusable foundation capable of supporting games with the systemic depth, reactivity, and world complexity associated with titles like Fallout: New Vegas, while remaining entirely independent from the original engine and its 3D rendering model.

The core philosophy of Mojave is strict, data-oriented design. Game content should be authored as data, not scripts. During development, content is represented in JSON so it remains easy to inspect, edit, validate, and generate. For shipping, that data will be compiled into a compact binary format so the final runtime stays fast, lightweight, and practical to distribute. The project should avoid unnecessary complexity and should not depend on Lua or any other embedded scripting layer. The intention is to make it possible to build rich Bethesda-like RPGs through structured data and well-defined engine systems alone.

Mojave is not trying to recreate Gamebryo or preserve legacy behavior one-to-one. It is trying to define a simpler, more coherent, and more maintainable runtime that captures the important parts of this style of game in a top-down 2D form. This includes world state, map traversal, entities, items, dialogue, quests, combat, interaction, progression, UI, audio, and save/load behavior. The systems should be strongly inspired by the Fallout: New Vegas model wherever that helps preserve the depth and feel of that kind of RPG.

The immediate goal is to establish a solid pure C core with an initial Raylib backend. The architecture should remain modular so the core runtime is cleanly separated from backend details and can support future rendering or platform layers without rewriting core systems. The broader goal is to create a runtime that feels disciplined, understandable, and extensible, without collapsing into engine bloat, excessive abstraction, or avoidable complexity.

The long-term objective of Mojave is to make it easier for people to create games in this style without needing to learn scripting. The final target is a 2D re-implementation of Fallout: New Vegas, but the project should also stand on its own as a generic runtime that others can use for their own creations.

The project currently depends on three core libraries only. Flecs is used as the main ECS and is cloned locally in the repository root as a gitignored dependency. yyjson is used for JSON parsing and writing and can be consumed from the system package. Raylib is used as the initial backend and can also be consumed from the system package.

When working on Mojave, prioritize simplicity, determinism, modularity, and strong data modeling. Prefer explicit systems over clever abstractions. Keep the runtime generic, keep the codebase tight, and make choices that support the broader goal of a reusable, data-driven top-down RPG engine.

For Mojave, "data-driven" means the Fallout: New Vegas model: engine systems are hardcoded in C, while game content is authored as structured data. Do not try to make the core engine itself soft, scriptable, or endlessly configurable just for flexibility. The goal is a fixed set of strong runtime systems that can be combined through data files and, later, GUI authoring tools.

Treat JSON as a development-time authoring format. Data schemas should be explicit, stable, editor-friendly, and built around strong IDs and references so future tools can safely create, validate, and modify content. Prefer reusable definitions and archetypes over ad hoc one-off inline data when that improves authoring clarity.

The current project phase is still prototyping. Use simple placeholder shapes for visuals and do not introduce audio systems or real asset pipeline work unless explicitly requested. However, prototype UI must still be robust: always measure text, avoid overlap, clamp or scroll overflowing content, and keep layouts readable at runtime.

Keep system boundaries clean. Runtime owns gameplay rules, simulation state, quests, dialogue logic, inventory logic, and save/load behavior. The backend should mainly handle rendering, input, and presentation. Avoid leaking gameplay decisions into backend code.

---

## Dependencies

- **Flecs ECS** - `flecs/distr/` (single-header, compiled separately)
- **yyjson** - system package for JSON parsing/writing
- **Raylib** - system package for rendering/input (initial backend)

---

## Build System

Uses a simple Makefile. Must define Flecs addons **before** including `flecs.h`:

```makefile
DFLECS_CUSTOM_BUILD -DFLECS_SYSTEM -DFLECS_PIPELINE -DFLECS_OS_API_IMPL
```

Key targets:
- `make` - build
- `make clean` - clean build artifacts
- `./mojave` - run

---

## ECS Architecture

### Flecs Custom Build

Flecs uses a custom build system where addons must be opted-in via `FLECS_*` defines **before** including `flecs.h`. The systems addon is NOT enabled by default.

```c
#define FLECS_CUSTOM_BUILD
#define FLECS_SYSTEM          // ECS_SYSTEM macro, ecs_system_init
#define FLECS_PIPELINE        // ecs_progress(), system scheduling
#define FLECS_OS_API_IMPL     // OS API implementations
#include "flecs.h"
```

These defines must be in the Makefile and passed when compiling `flecs.c` too.

### Components

Defined in `src/runtime.h` and declared/defined in `src/runtime.c`:

```c
// Core components
Position { float x, y }
Velocity { float x, y }
CollisionBox { float w, h }
Renderable { unsigned char r, g, b, a }

// Entity type tags (marker components)
Npc { const char *name }
Item { int map_index }

// Gameplay components
Team { int team_id }              // MOJAVE_TEAM_PLAYER, FRIENDLY, HOSTILE
Hp { float current, max }
ItemRef { const MojaveItemDefinition *definition }
DialoguRef { const MojaveDialogue *dialogue, const char *start_id }
ActiveDialogue { const char *node_id }
```

### Systems

Registered in `mojave_game_init()` and run via `ecs_progress()`:

```c
ECS_SYSTEM(world, MovementSystem, EcsOnUpdate, Position, Velocity);
ECS_SYSTEM(world, TilemapCollisionSystem, EcsOnUpdate, Position, Velocity, CollisionBox);
ECS_SYSTEM(world, CombatSystem, EcsOnUpdate, Hp);
ECS_SYSTEM(world, PickupSystem, EcsOnUpdate, Position, Item);
```

System behavior:
- **MovementSystem** - applies velocity to position
- **TilemapCollisionSystem** - resolves tilemap collision for entities with CollisionBox
- **CombatSystem** - deletes entities when HP <= 0
- **PickupSystem** - reserved for future auto-pickup

### Global State

For ECS systems that need map/world access:

```c
static const MojaveMap *g_map = NULL;
static MojaveGame *g_game = NULL;
```

---

## Project Structure

### Source Files (`src/`)

| File | Purpose |
|------|---------|
| `main.c` | Entry point, initializes game and backend |
| `runtime.h/c` | ECS components, systems, game logic, API |
| `backend_raylib.h/c` | Raylib rendering, input (6 functions) |
| `content.h/c` | JSON loading for maps, items, quests, dialogue |
| `collision.h/c` | Tilemap collision resolution |

### Header-Only Modules (`src/`)

| File | Purpose |
|------|---------|
| `backend.h` | Backend interface (6 functions) |

### Backend Interface

Thin backend with exactly 6 functions:

```c
bool mojave_backend_init(const MojaveBackendConfig *config);
void mojave_backend_shutdown(void);
bool mojave_backend_should_close(void);
float mojave_backend_frame_time(void);
MojaveInput mojave_backend_poll_input(void);
void mojave_backend_draw(const MojaveGame *game);
```

Backend accesses game data ONLY through runtime API functions:
- `mojave_game_get_player()`, `mojave_game_get_entity_render_data()`
- `mojave_game_get_npc_render_data()`, `mojave_game_get_item_render_data()`
- `mojave_map_get_width/height/tile_size/tile/name()`

---

## Runtime API Key Functions

### Entity Management
```c
ecs_entity_t mojave_game_spawn_player_ecs(MojaveGame *game, float x, float y);
ecs_entity_t mojave_game_spawn_npc_ecs(MojaveGame *game, const MojaveNpc *npc_def, const MojaveDialogue *dialogue);
ecs_entity_t mojave_game_spawn_item_ecs(MojaveGame *game, const MojaveItemDefinition *item_def, float x, float y, int map_index);
ecs_entity_t mojave_game_get_player(const MojaveGame *game);
```

### Combat
```c
void mojave_game_damage_entity(MojaveGame *game, ecs_entity_t entity, float damage);
float mojave_game_get_entity_hp(MojaveGame *game, ecs_entity_t entity);
bool mojave_game_entity_is_alive(MojaveGame *game, ecs_entity_t entity);
```

### Render Data (for backend)
```c
bool mojave_game_get_entity_render_data(const MojaveGame *game, ecs_entity_t entity, MojaveRenderData *out);
bool mojave_game_get_npc_render_data(const MojaveGame *game, int index, MojaveNpcRenderData *out);
bool mojave_game_get_item_render_data(const MojaveGame *game, int index, MojaveItemRenderData *out);
```

### Map Accessors (for backend)
```c
int mojave_map_get_width(const MojaveMap *map);
int mojave_map_get_height(const MojaveMap *map);
int mojave_map_get_tile_size(const MojaveMap *map);
int mojave_map_get_tile(const MojaveMap *map, int x, int y);
const char *mojave_map_get_name(const MojaveMap *map);
```

---

## Current Architecture State

### Completed (100% ECS)
- ✅ Player entity with Position, Velocity, CollisionBox, Team, Hp
- ✅ NPC entities with Position, Velocity, CollisionBox, Renderable, Team, DialoguRef, ActiveDialogue, Npc
- ✅ Item entities with Position, CollisionBox, Renderable, ItemRef, Item
- ✅ MovementSystem for all entities with Position + Velocity
- ✅ TilemapCollisionSystem for all entities with CollisionBox
- ✅ CombatSystem for HP/death
- ✅ NPC proximity detection via ECS queries
- ✅ Item rendering via ECS queries
- ✅ Backend only calls runtime API (no direct map/entity access)

### Implemented but Not Fully ECS
- ⚠️ Pickup requires E keypress (dialogue system), not yet auto-pickup
- ⚠️ Dialogue/Quest state still in MojaveGame struct, not ECS components
- ⚠️ QuestSystem and DialogueSystem not yet implemented as ECS systems

### Not Yet Implemented
- ❌ NPC AI systems (patrol, chase, flee)
- ❌ Entity-entity collision (not just tilemap)
- ❌ Combat system integration with NPC AI
- ❌ Sound/audio systems
- ❌ Multiple maps/world traversal
- ❌ Binary asset format for shipping

---

## Data Files (`data/`)

| File | Purpose |
|------|---------|
| `first_map.json` | Test map with NPCs and items |
| `items.json` | Item definitions |
| `quests.json` | Quest definitions |
| `guide_dialogue.json` | Test dialogue |

---

## Key Constants

```c
MOJAVE_PLAYER_SIZE = 18.0f
MOJAVE_PLAYER_SPEED = 180.0f
MOJAVE_NPC_SIZE = 18.0f
MOJAVE_ITEM_SIZE = 12.0f
MOJAVE_NPC_INTERACT_RANGE = 42.0f
MOJAVE_ITEM_PICKUP_RANGE = 30.0f
```

---

## Git History

| Commit | Description |
|--------|-------------|
| `3756027` | Full ECS migration: items, collision, combat, pickup systems |
| `4907fda` | Migrate NPCs to ECS: spawn into world during init, read positions from ECS |
| `eb7f9bd` | Enable Flecs systems addon and add proper ECS system |
| `75a9e95` | Thin backend API: runtime provides clean rendering interface |
| `7232bba` | Add ECS foundation for proper Flecs usage |
| `de1d669` | Fix critical memory leaks and code audit improvements |

---

## Working on Mojave

### Starting Fresh
1. Read this AGENTS.md for current architecture
2. Check git log for recent changes
3. Run `make` to verify build works
4. Run `./mojave` to test

### Code Conventions
- Pure C99, no C++ features
- No comments in code unless explicitly requested
- Use existing code style and patterns
- Flecs components: declaration in `.h`, definition in `.c`
- ECS_SYSTEM macro for system registration
- Backend is thin - game logic stays in runtime.c

### Before Committing
- Run `make clean && make` - must build without warnings
- Check for memory leaks if adding allocation
- Keep runtime/backend separation clean
- Update this AGENTS.md if architecture changes
