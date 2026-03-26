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
