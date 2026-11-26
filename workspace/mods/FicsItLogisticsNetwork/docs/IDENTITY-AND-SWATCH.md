# FILN identity and optional shared native palette

Default names are numbered on the authoritative server during bounded discovery. Named members and per-category counters are saved. A player name equal to the native factory default is indistinguishable from an untouched default at first encounter. Custom names, including [S], are preserved. Explicitly configured stations, depots and actual committed providers are included; ordinary speculative providers are not.

## Shared palette (opt-in existing slot; NOT a dedicated new swatch)

The current implementation deliberately does not allocate or modify a global game palette slot by itself. The authoritative host may use the developer console:

- `filn.swatch list`: list native unlocked swatch slot IDs.
- `filn.swatch N`: bind FILN to the existing slot N without changing that slot's colors.
- `filn.swatch N taxi`: explicitly set that existing slot to taxi yellow #FFC107 and charcoal #202124, preserving its paint finish, then bind FILN.
- `filn.swatch off`: stop assignment and restore the previous paint on objects still using the assigned swatch; unloaded actors retry on discovery.

Choose an unused regular swatch: changing a native swatch also affects any non-FILN objects already using it. No existing slot is chosen or edited automatically. Use the normal in-game Customizer to edit the shared slot afterward. Member assignment is bounded, not per frame, and preserves skin, pattern and material. A later individual repaint is respected; selecting another network swatch reapplies it. Leaving FILN restores original paint where readback confirms the assigned swatch. Objects already using the selected native swatch before enrollment are not treated as FILN-owned paint.

Dedicated new swatch remains T137: allocate a collision-free persistent slot, register a descriptor and customization collection/recipe, replicate ID and palette to remote clients, prove native Customizer edit and actual vehicle/station rendering in cooked game. No dependency on KBFL or other mod. The opt-in path is a distinct interim feature, not dedicated-slot completion.

All native behavior requires game verification, including saved names, palette readback, live propagation and restore. No original save modification by development tools.
