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

## Copy-only conversion from legacy classes to native FILN

Use an isolated copy of a quiescent save. Select exact actor paths explicitly:
`{"sha256":"<input SHA256>","stations":["<actor path>"]}`.
Run `node scripts/save-tools/convert-native-stations.cjs INPUT OUTPUT SELECTION.json`
for preview; append `--write` to write a new copy outside SaveGames.
The converter changes only known solid station classes/recipes and removes the
explicit programmable filter after exporting it. It writes a .native.tsv resource
and depot manifest plus a .report.json receipt. The original save is never edited.
Remaining references to legacy mods are reported, not removed. Do not disable
those mods until a disposable independent load proves the save no longer needs them.

The manifest is external migration data for Codex/Lua scripts. The native mod
does not read it, provide legacy import commands, or recognize legacy recipes.
Use ordinary native configuration/enrollment interfaces after external preparation.
No game process is started or stopped by these tools. Test with
`node --test scripts/save-tools/native-station-conversion.test.cjs`.

### City-scale conversion and unconfigured templates

The external manifest accepts up to 512 explicitly selected stations and 256 KiB.
A missing or empty legacy FilterItem is converted to an ordinary vanilla station
and recorded as an `ordinary` row, not a FILN requester. No resource is inferred.
Malformed, multi-resource or contradictory explicit filters still fail closed.
Native `mForceVehicleFuelType` is a vanilla docking-station property and is preserved.
Actual legacy recipe/schematic dependencies must be audited separately; do not
remove KDataForge or TruckStationSettings based solely on station conversion.

### Optional legacy unlock cleanup

Append `--cleanup-unlocks` to preview or write. This removes only the six exact
FILN/Smarter Trucks unlock asset references from the observed native recipe and
schematic manager arrays. It rejects remaining programmable actors or uses of
those six asset paths elsewhere. Other references, including construction
statistics and header mod metadata, remain unchanged and are reported where
visible in parsed values. This is not a dependency-free certification.
Run `node --test scripts/save-tools/legacy-unlock-cleanup.test.cjs`.

The dependency scanner walks typed values and map keys rather than serializing
whole objects to JSON (64-bit counters can be BigInt). Native construction
statistics in `mActorsBuiltCount` are intentionally preserved and reported;
merging historical maxima for different classes would be an approximation.

## Current standalone scope (2026-11-25)

Native conversion is preparation only: the manifest is not native configured requesters or depots. Do not convert requesters back to TruckStationSettings for native release. Preserve original explicit resource, actor/identifier/road identity, fuel/cargo and connections; apply FILN-owned selection and depot recipe only after assets/APIs are proven. Legacy tools may read old references for external copy-only conversion. Never rewrite original City or mark missing/skipped configuration successful.

## Correction after failed user migration (2026-11-25)

The old converter incorrectly assigned the vanilla actor AND recipe even to legacy depots. The corrected conversion uses Build_FILNDepot and Recipe_FILNDepot for an explicitly identified solid depot; identity, components, cargo/fuel, road node and connections remain unchanged. Writing such a copy requires the actual schema-4 reload receipt (`--asset-receipt=/path/to/reload.json`). The normal `--write` path rejects incomplete explicit resource configuration; `--write-preparation` deliberately creates an incomplete staging copy with a pending report. A real external migration still must apply and verify all explicit resources and vehicle enrollment, including safe handling of previously docked/occupied objects. Do not derive resource or depot identity from the damaged converted save alone. Use the preserved original legacy save or exact original conversion evidence, and write a separate copy outside live SaveGames. No native importer or external mod dependency added. No actual save load or migration success established.
