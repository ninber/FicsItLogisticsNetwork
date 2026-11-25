# Architecture and risk boundaries

Current target: [standalone native requirements](../../../specs/014-native-completion/standalone-station-implementation.md). Source gaps are listed in the [consistency audit](../../../specs/014-native-completion/consistency-audit.md).

Authoritative gameplay belongs to the native saved ALogisticsSubsystem, registered via SML. Stable station IDs, home ownership, job reservations and intent are saved; native path GUIDs resolve current road endpoints. Never save volatile network IDs. Atomic reservation precedes route mutation, readback is mandatory, failure conservatively retains leases and cargo.

The medium-aware portable policy is shared by native dispatch and host tests. Both native adapters must supply actual resource form, capacity, quantity, docking compatibility and explicit requester selection; ordinary LOAD supply is observational. Empty demand, full fresh load, retained cargo priority, warehouse rules and directed nearest-provider ranking are preserved. No stock-derived requester identity.

Source implements bounded scheduling/cache, automatic enrollment/overflow for recorded or exact recipe-identified depots, recovery, fuel service and synchronized native operation receipts. Receipts aggregate across factory threads; authoritative reconciliation validates actual inventory and job identity. No Lua dispatcher, third-party station dependency or custom inventory writes in release design. Third-party filter adapter is removed; old saved unowned filter records are ineligible until explicitly removed/reconfigured. Solid-only gates remain until verified fluid support; portable tests cannot establish native transfer semantics.

Use original game UI components for one FILN-owned resource selector; keep ordinary station actors and controls. No management panel, manual player registration or global widget-tree guessing. Author dedicated depot assets and verify native SML root discovery, availability, placement, compatibility and save behavior. Native web telemetry is bounded and read-only; report unknown quantities as unknown.

Runtime, asset authoring and save/uninstall semantics remain unverified. Native tests must use isolated saves, never original City or automated UI. No successful route write proves delivery.
