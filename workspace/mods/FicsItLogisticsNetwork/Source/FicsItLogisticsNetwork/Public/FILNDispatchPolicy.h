#pragma once
// Pure C++ policy for native FILN automatic dispatch. This is engine- and loader-independent.
// The server adapter MUST supply fresh, authoritative snapshots and verified directed road costs.
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace filn::dispatch {

enum class Kind { Pickup, RetainedCargo };
enum class Medium { Solid, Fluid };

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
    Medium medium = Medium::Solid;
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
    Medium medium = Medium::Solid;
    bool enrolled = false;
    bool homeReady = false;
    bool idle = false;
    bool autopilot = false;
    bool driver = false;
    bool fuelReady = false;
    bool reserved = false;
    bool cargoOriginKnown = false;
    bool cargoOriginStorage = false;
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
    std::optional<Station> provider; // required for every new delivery
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
    double providerDistance;
    double emptyDistance;
};

// Ordinary providers have no explicit filter. Their actual single cargo type
// must match the explicitly configured requester. Never infer requester identity.
inline bool ProviderResourceMatches(std::string_view requested,std::string_view observed,int kinds,std::string_view configured) {
 return !requested.empty() && kinds==1 && observed==requested && (configured.empty() || configured==requested);
}
// Ranking can stop only after all equal-cost providers have been considered.
inline bool WithinNearestGroup(double firstEligible,double candidate) {
 return std::isfinite(candidate) && candidate>=0 &&
  (firstEligible<0 || (std::isfinite(firstEligible) && candidate<=firstEligible));
}
inline bool HomePositionReady(double distanceSquared,double verticalDistance,double speed,bool foreignDock) {
 return !foreignDock && std::isfinite(distanceSquared) && distanceSquared>=0 && distanceSquared<=800.0*800.0 &&
  std::isfinite(verticalDistance) && std::abs(verticalDistance)<=250.0 &&
  std::isfinite(speed) && std::abs(speed)<=1.0;
}
inline bool ValidCost(double cost) { return std::isfinite(cost) && cost >= 0.0; }

inline bool EligibleWithoutRoads(const Offer& offer) {
    const Vehicle& v = offer.vehicle;
    const Station& q = offer.requester;
    if (v.id.empty() || v.home.empty() || !v.enrolled || !v.homeReady || !v.idle ||
        !v.autopilot || v.driver || !v.fuelReady || v.reserved || v.cargoCapacity < 1) {
        return false;
    }
    if (q.medium != v.medium || q.id.empty() || q.id == v.home || q.resource.empty() || !q.explicitFilter || q.loadMode ||
        !q.powered || q.paused || q.occupied || q.depot || q.kinds != 0 || q.quantity != 0 || q.capacity < 1) {
        return false;
    }

    if (!offer.provider) return false;
    const Station& p = *offer.provider;
    const bool retained = v.cargoQuantity > 0;
    if (v.cargoQuantity < 0 || v.cargoQuantity > v.cargoCapacity ||
        (retained ? (v.cargoKinds != 1 || v.cargoResource != q.resource) : v.cargoKinds != 0)) return false;
    // Same provider rules, but only the missing capacity needs to be loaded.
    if (p.id.empty() || p.id == v.home || p.id == q.id ||
        p.medium != q.medium || !p.loadMode || !p.powered || p.paused || p.occupied || p.depot || p.kinds != 1 ||
        p.resource != q.resource || p.quantity <= 0 || p.quantity < v.cargoCapacity - v.cargoQuantity ||
        p.capacity < p.quantity || q.capacity < v.cargoCapacity || !PairAllowed(p.label, q.label)) return false;
    if (retained && IsStorage(q.label) && (!v.cargoOriginKnown || v.cargoOriginStorage)) return false;
    return true;
}

// Dispatch choices require actual directed road evidence.
inline std::optional<Choice> Evaluate(const Offer& offer) {
    if (!EligibleWithoutRoads(offer)) return std::nullopt;
    const auto& v = offer.vehicle;
    const auto& q = offer.requester;
    const auto& roads = offer.roads;
    if (!ValidCost(roads.requesterToHome)) return std::nullopt;
    if (!ValidCost(roads.homeToProvider) || !ValidCost(roads.providerToRequester)) return std::nullopt;
    const double distance = roads.homeToProvider + roads.providerToRequester + roads.requesterToHome;
    if (!std::isfinite(distance)) return std::nullopt;
    return Choice{v.cargoQuantity>0 ? Kind::RetainedCargo : Kind::Pickup,
                  v.home, v.id, offer.provider->id, q.id, q.resource,
                  v.cargoCapacity, distance, roads.providerToRequester, roads.homeToProvider};
}

inline bool Better(const Choice& a, const Choice& b) {
    if (a.kind != b.kind) return a.kind == Kind::RetainedCargo;
    if (a.requester != b.requester) return a.requester < b.requester;
    // For one requester, nearest eligible provider is primary. Empty/depot travel
    // must never override provider-to-requester road distance (authoritative spec).
    if (a.requester == b.requester) {
        if (a.providerDistance != b.providerDistance) return a.providerDistance < b.providerDistance;
        if (a.emptyDistance != b.emptyDistance) return a.emptyDistance < b.emptyDistance;
    }
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
