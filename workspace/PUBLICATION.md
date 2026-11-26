# FILN publication / SMR disclosure

Status: preparation only; no release approval. See RELEASE.md and native acceptance matrix.

## Mod page
Name: FicsIt Logistics Network
Reference: FicsItLogisticsNetwork
Source: https://github.com/ninber/FicsItLogisticsNetwork
Support: https://github.com/ninber/FicsItLogisticsNetwork/issues

Unofficial pioneer* modification. Corporate approval pending for <<ERROR>> days.

*(to pioneer) unofficial community of engineers.

Unofficial community modification; not affiliated with or endorsed by Coffee Stain Studios. Original assets and trademarks belong to their respective owners.

## AI Usage Transparency
Extensive AI-assisted programming (vibe coding), primarily ChatGPT and OpenAI Codex, including architecture, code, debugging, testing, refactoring, documentation and release text. No generative AI-created visual or audio assets are included; no runtime AI. Confirm against the final release.

## Network Activity Transparency
Optional read-only local logistics dashboard, 127.0.0.1:8765. Native gameplay snapshots are stored locally. Item artwork is generated on demand from the installed base game into a runtime-only cache. No external icon requests, external analytics, advertising, tracking or gameplay telemetry uploads. Native dashboard POST is disabled. Historical Lua development tools are not part of the public runtime. Verify actual release behavior and installation before publication.

## Rights and asset provenance
No new general-purpose source license is granted for FILN-owned code in this revision. Previous GPL grants remain effective for previously distributed versions; do not remove applicable third-party terms or claim that existing licensed material was retroactively relicensed. Verify contributor rights and any SML/engine obligations before distribution. Smarter Truck Stations is a technical reference, not a licensed source/asset dependency; independently review potentially copied implementation before release.
Base game art belongs to Coffee Stain Studios. Former extracted PNGs came from SatisfactoryTools revision fb43f813c03eb306b0e8d0047a8c53ae010044e0; removed from the current tree and handoff. Prior history/forks may still hold them. Do not force-rewrite history without separate coordination. Runtime cache must never be distributed. Reuse vanilla Blueprint references rather than package original game assets. Local extraction is not a blanket permission to redistribute.
Check https://ficsit.app/content-policy and https://ficsit.app/tos when submitting. No external download links to binary mods on SMR.

## Release gates
- Run source compliance check, full portable tests and native acceptance report.
- Inspect actual cooked package and source provenance; no distributed game art/cache/legacy runtime.
- Compile, test real game, prove dispatch and migration behavior, test dashboard offline, missing icons, transparency and first-load frame-time, warm cache and game build invalidation.
- Validate dashboard install for SMM users, dependency metadata, platform and game compatibility; never advertise untested features.
- No publish until the release gates pass and owner authorizes upload.
