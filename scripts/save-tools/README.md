# Offline blueprint utilities

These are optional blueprint editing tools, not part of the mod or its CI. The one-time City save repair is not a repository feature. Obsolete city conversion tools were retired; do not use them from history to repair current worlds.

Install the pinned parser with `npm ci --prefix scripts/save-tools`.

## Upgrade FILN blueprint conveyors

`node upgrade-filn-conveyors.cjs INPUT_DIR STAGING_DIR` stages FILN-prefixed copies only.
Upgrades vanilla Mk.1–3 belts/lifts to Mk.4, preserves Mk.4+ and stable actor references, updates
recipe requirements and geometry-scaled material deltas, and checks complete binary readback.
Keep .sbpcfg bytes unchanged. Back up and hash-check source files before atomic installation;
never call live category refresh or automate build-gun previews. Native material quotes remain
a manual validation item. Regression: `node upgrade-filn-conveyors.test.cjs`.
