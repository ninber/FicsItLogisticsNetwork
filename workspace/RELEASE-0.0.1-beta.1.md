# FILN 0.01b — Early Beta

Version: 0.0.1-beta.1
Tag: v0.0.1-beta.1
Status: release candidate preparation, not yet approved for public distribution.

*Unofficial pioneer\** modification. Corporate approval pending for &lt;&lt;ERROR&gt;&gt; days.

*\*(to pioneer)* unofficial community of engineers.

FILN is an experimental native truck logistics mod intended to automate resource delivery between provider stations, requester stations and depots. This first beta is intended to gather gameplay feedback, not to promise a stable logistics network.

## Important
- Back up your saves and test on a disposable copy. Existing save migration, dispatch, fluid handling, station controls, multiplayer, and safe removal still require documented acceptance.
- Target game build CL 520140 and SML 3.12.1 (exact version; other SML versions have not been validated) as declared in the plugin. Compatibility with newer game builds is not established.
- Only the freshly built, verified Alpakit archive for this exact version may be attached. Windows client is the current packaging target; Proton, dedicated servers and other platforms must not be claimed compatible without evidence.
- The optional local read-only web dashboard currently requires a separately started companion on the host; it is not automatically installed by SMM. No externally hosted item icons. Native local icon cache requires runtime evidence.
- Report bugs with game and SML version, mod version, reproduction steps, and relevant redacted logs: https://github.com/ninber/FicsItLogisticsNetwork/issues

## AI Usage Transparency
Extensive AI-assisted programming (vibe coding) using ChatGPT and OpenAI Codex for architecture, code, debugging, tests, documentation and release text. No generative AI-created visual or audio assets are included; no runtime AI.

## Network Activity Transparency
Optional local dashboard on 127.0.0.1:8765, read-only in native mode. Telemetry and generated base-game item icon cache remain local. No intended external telemetry uploads, advertising, tracking or external icon requests.

Unofficial community modification. Not affiliated with or endorsed by Coffee Stain Studios. Original game assets and trademarks belong to their respective owners.

## Maintainer checklist (remove from public notes when ready)
- [ ] Record exact source commit and successful versioned build; inspect packaged descriptor and archive contents.
- [ ] Verify gameplay on disposable saves; document remaining defects and confirm no unacceptable save/cargo corruption.
- [ ] Complete third-party provenance and publication compliance review; verify actual cooked content.
- [ ] Verify clean SMM installation and supported targets; confirm local dashboard setup or explicitly omit companion distribution.
- [ ] Attach actual Alpakit release ZIP (never the source handoff or runtime cache), verify checksum, keep GitHub release marked pre-release.
- [ ] Submit the same verified package to SMR only after release gates pass and owner approves.
