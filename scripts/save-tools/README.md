# Existing-city station replacement

Install the pinned parser with `npm ci --prefix scripts/save-tools`. Run:

```sh
node scripts/save-tools/replace-stations.cjs INPUT.sav NEW.sav manifest.json
node scripts/save-tools/replace-stations.cjs INPUT.sav NEW.sav manifest.json --write
```

The first command previews. The second writes a **new** save; it rejects an existing
output, the input path as output, a changed input hash, duplicate selections and
anything other than an ordinary truck station. The game should be closed before
loading the output. Keep the original save. No external service is used.

Manifest example:

```json
{
  "sha256": "exact input save SHA256",
  "stations": [{
    "path": "Persistent_Level:PersistentLevel.Build_TruckStation_C_...",
    "item": "/Game/FactoryGame/Resource/Parts/IronIngot/Desc_IronIngot.Desc_IronIngot_C",
    "recipe": "/KDataForge/Gen/FILNStations.Gen_FILNStations_Recipe_LogisticsStation"
  }]
}
```

The optional recipe defaults to the original programmable recipe. The FILN recipe
requires the installed FILNStations content extension. Exactly three fields change:
actor class, built-with recipe, and explicit filter. All component IDs, station
identifier, node GUID, node/road connections, transforms, names, inventory objects,
power/belt references and truck-route references remain in place. The complete
parsed body must survive serialization/reparse unchanged except for these edits.

This is an **offline copy conversion**, not a Build Gun upgrade or an in-game
hot replacement. Runtime validation is mandatory for each supported game/mod
version. A successful parse alone does not prove native game migration.
Parser: @etothepii/satisfactory-file-parser 4.1.1, MIT, locked in package-lock.json.

## Upgrade FILN blueprint conveyors

`node upgrade-filn-conveyors.cjs INPUT_DIR STAGING_DIR` stages FILN-prefixed copies only.
Upgrades vanilla Mk.1–3 belts/lifts to Mk.4, preserves Mk.4+ and stable actor references, updates
recipe requirements and geometry-scaled material deltas, and checks complete binary readback.
Keep .sbpcfg bytes unchanged. Back up and hash-check source files before atomic installation;
never call live category refresh or automate build-gun previews. Native material quotes remain
a manual validation item. Regression: `node upgrade-filn-conveyors.test.cjs`.
