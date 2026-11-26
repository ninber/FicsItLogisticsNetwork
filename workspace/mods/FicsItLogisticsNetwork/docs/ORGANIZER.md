# In-game FILN Organizer

Open in normal gameplay with **Ctrl+Shift+L**. The native interaction window defaults to Vehicles, then Stations, then Diagnostics. Close with Esc or CLOSE. It does not assign or modify routes, cargo, stations or dispatcher state.

The initial keyboard chord uses the Satisfactory character input initialization delegate. It is configurable on restart in the game's Saved/Config/<platform>/Game.ini:

```ini
[FILN.Organizer]
Key=L
Ctrl=True
Shift=True
Alt=False
```

Standard player-mappable Enhanced Input assets and the in-game key settings menu are not yet implemented. This binding is a source implementation, not proof it is processed by the shipping Enhanced Input stack. The developer console command `filn.organizer` is an alternate local invocation; existing menus block opening.

The native HUD interaction stack handles input/focus. Custom UMG palette uses compact rows and native item descriptor names/icons. Actual UI layout, fonts, focus, icon size and DPI must be inspected in the cooked game; no claim that custom styling is identical to the base game. Remote clients explicitly show snapshot unavailable; no server payload RPC is implemented. The host/standalone uses the completed in-memory snapshot also serialized for the existing web dashboard. Open-only demand expires two seconds after the last widget refresh; an accepted scan may finish after close.

Verification: test all three tabs in a disposable world including the blocked-registry autosave, actual parked mixed cargo, long routes, target highlighting, grouped requests and supply, Esc, close and repeated opens, 1080p/4K screenshots, no per-frame world scan or inventory mutations. Record actual results in acceptance.json. No original save mutation or automatic game UI interaction.
