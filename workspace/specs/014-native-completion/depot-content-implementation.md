> **Scope/evidence notice (2026-11-25):** Historical implementation/research below may describe superseded dependency, solid-only, UI or Blueprint-root decisions. Current release requires SML-only standalone solid/fluid logistics and FILN-owned selector. See [current requirements](standalone-station-implementation.md). Local Codex reports Linux native compile/link after unpublished unity fixes, but depot Blueprint authoring and fresh reload failed; no current package/game acceptance. Do not treat old successes as current runtime proof.

# Depot authored content — implementation and acceptance

The native depot is identified by the exact FILN recipe path, never by a station name, legacy KDataForge recipe or generic LOAD station. The existing vanilla actor class/CDO is reused, following the Lua crash workaround. Its ordinary UI stays intact. SML's root module registers the recipe during Initialization; the authoritative subsystem makes the recipe available after vanilla Truck Station unlock and confirms recipe-manager readback. No global class mutation or runtime-created actor templates.

The editor-only script scripts/author-native-depot.py creates three original Blueprint assets (descriptor, recipe, root world module) without copying Smarter Truck Stations assets. Existing assets are checked rather than overwritten. It requires the exact matching StarterProject, FILN native module built for editor, PythonScriptPlugin, and the installed required dependency. These prerequisites are not available in the remote connector environment. The script and C++ are source-only until the authoring commandlet and cook complete.

Example on the development machine (not the running game):
1. Install canonical sources and build the Editor target.
2. Enable PythonScriptPlugin in the development project; use the existing CSS editor environment. Ensure no other root GameWorldModule exists for this plugin (if it does, integrate the recipe property into that root instead).
3. Set FILN_DEPOT_AUTHOR_RECEIPT to a writable fresh absolute path and run UnrealEditor-Cmd FactoryGame.uproject -run=pythonscript -script=/absolute/path/scripts/author-native-depot.py -unattended -NullRHI.
4. Require process exit success AND the new receipt and three real .uasset files in the StarterProject FILN Content/Depot directory. Verify parent classes/recipe cost/category/BuildGun producer in editor. Cook using Alpakit and inspect actual packaged content. Preserve editor-authored assets on source install.
5. On a disposable world, place depot from standard build menu, check GetBuiltWithRecipe identity, normal docking, power/fuel, automatic registration, exclusive overflow, save reload. Ordinary LOAD stations must not auto-become depots.

No asset, cook, Build Gun listing or real gameplay proof exists from this source commit. Dedicated content still fails release acceptance. The new recipe is a mod save reference; independent uninstall requires a deliberate external conversion or dismantling before removing FILN. Do not call vanilla-actor reuse proof of uninstall compatibility.

Build entry point is now `bash scripts/headless.sh author-depot` after the matching editor build. The development prerequisite validator checks the upstream plugin source and required binary Blueprint package headers; `headless.sh package` refuses missing depot assets. The ZIP verifier refuses archives with no FILN cooked content container. These are negative guards, not proof of successful cooking. The commandlet has not been executed remotely.
