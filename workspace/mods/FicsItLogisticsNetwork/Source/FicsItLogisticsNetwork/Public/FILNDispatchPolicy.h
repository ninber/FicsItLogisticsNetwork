// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Pure C++ policy for native FILN automatic dispatch. This has no Unreal/UE4SS dependencies.
// The server adapter MUST supply fresh, authoritative snapshots and verified directed road costs.
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace filn::dispatch {

enum class Kind { Pickup, RetainedCargo };

inline bool IsStorage(std::string_view name) {
    for (std::size_t i = 0; i + 2 < name.size(); ++i) {
        if (name[i] == '[' && (name[i + 1] == 'S' || name[i + 1] == 's') && name[i + 2] == ']') {
            return true;
        }
    }
    return false;
}

inline bool PairAllowed(std::string_view provider, std::string_view requester) {
    return !(IsStorage(provider) && IsStorage(requester));
}

// Station.resource must be the actual descriptor identity, not its localized display name.
// A requester resource must originate from an explicit in-game filter, never from stock.
// A provider may instead be identified from exactly one observed cargo type.
struct Station {
    std::string id;
    std::string label;
    std::string resource;
    bool loadMode = false;
    bool powered = false;
    bool paused = false;
    bool occupied = false;
    bool depot = false;
    bool explicitFilter = false;
    int kinds = 0;
    int quantity = 0;
    int capacity = 0;
};

struct Vehicle {
    std::string id;
    std::string home;
    std::string cargoResource;
    bool enrolled = false;
    bool homeReady = false;
    bool idle = false;
    bool autopilot = false;
    bool driver = false;
    bool fuelReady = false;
    bool reserved = false;
    int cargoKinds = 0;
    int cargoQuantity = 0;
    int cargoCapacity = 0;
};

// Costs are directed path lengths, in the same units, for a vehicle-compatible route.
// Missing/failed path queries must be represented by negative (default) costs.
struct RouteCosts {
    double homeToProvider = -1.0;
    double providerToRequester = -1.0;
    double homeToRequester = -1.0;
    double requesterToHome = -1.0;
};

struct Offer {
    Vehicle vehicle;
    Station requester;
    std::optional<Station> provider; // absent only for retained-cargo delivery
    RouteCosts roads;
};

struct Choice {
    Kind kind;
    std::string home;
    std::string truck;
    std::string provider;
    std::string requester;
    std::string resource;
    int amount;
    double roadDistance;
};

inline bool ValidCost(double cost) { return std::isfinite(cost) && cost >= 0.0; }

inline std::optional<Choice> Evaluate(const Offer& offer) {
    const Vehicle& v = offer.vehicle;
    const Station& q = offer.requester;
    if (v.id.empty() || v.home.empty() || !v.enrolled || !v.homeReady || !v.idle ||
        !v.autopilot || v.driver || !v.fuelReady || v.reserved || v.cargoCapacity < 1) {
        return std::nullopt;
    }
    if (q.id.empty() || q.id == v.home || q.resource.empty() || !q.explicitFilter || q.loadMode ||
        !q.powered || q.paused || q.occupied || q.depot || q.kinds != 0 || q.quantity != 0 || q.capacity < 1) {
        return std::nullopt;
    }

    Kind kind;
    int amount;
    double distance;
    std::string provider;
    if (offer.provider) {
        const Station& p = *offer.provider;
        if (v.cargoKinds != 0 || v.cargoQuantity != 0 || p.id.empty() || p.id == v.home || p.id == q.id ||
            !p.loadMode || !p.powered || p.paused || p.occupied || p.depot || p.kinds != 1 ||
            p.resource != q.resource || p.quantity < v.cargoCapacity || p.capacity < p.quantity ||
            q.capacity < v.cargoCapacity || !PairAllowed(p.label, q.label) ||
            !ValidCost(offer.roads.homeToProvider) || !ValidCost(offer.roads.providerToRequester) ||
            !ValidCost(offer.roads.requesterToHome)) {
            return std::nullopt;
        }
        kind = Kind::Pickup;
        amount = v.cargoCapacity;
        provider = p.id;
        distance = offer.roads.homeToProvider + offer.roads.providerToRequester + offer.roads.requesterToHome;
    } else {
        // A recovered truck with a single item type may make a partial direct delivery.
        // Never send retained cargo to a tagged warehouse because its origin is unproven.
        if (v.cargoKinds != 1 || v.cargoQuantity < 1 || v.cargoQuantity > v.cargoCapacity ||
            v.cargoResource != q.resource || q.capacity < v.cargoQuantity || IsStorage(q.label) ||
            !ValidCost(offer.roads.homeToRequester) || !ValidCost(offer.roads.requesterToHome)) {
            return std::nullopt;
        }
        kind = Kind::RetainedCargo;
        amount = v.cargoQuantity;
        distance = offer.roads.homeToRequester + offer.roads.requesterToHome;
    }
    if (!std::isfinite(distance)) return std::nullopt; // even finite legs can overflow
    return Choice{kind, v.home, v.id, provider, q.id, q.resource, amount, distance};
}

inline bool Better(const Choice& a, const Choice& b) {
    if (a.kind != b.kind) return a.kind == Kind::RetainedCargo;
    if (a.roadDistance != b.roadDistance) return a.roadDistance < b.roadDistance;
    if (a.home != b.home) return a.home < b.home;
    if (a.requester != b.requester) return a.requester < b.requester;
    if (a.provider != b.provider) return a.provider < b.provider;
    return a.truck < b.truck;
}

inline std::optional<Choice> Select(const std::vector<Offer>& offers) {
    std::optional<Choice> best;
    for (const Offer& offer : offers) {
        auto candidate = Evaluate(offer);
        if (candidate && (!best || Better(*candidate, *best))) best = std::move(candidate);
    }
    return best;
}

} // namespace filn::dispatch
