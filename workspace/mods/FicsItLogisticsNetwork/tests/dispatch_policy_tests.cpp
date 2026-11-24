// SPDX-License-Identifier: GPL-3.0-only
#include "FILNDispatchPolicy.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#define CHECK(value) do { ++checks; if (!(value)) { std::cerr << __LINE__ << ": " #value "\n"; std::exit(1); } } while (false)
using namespace filn::dispatch;

static Offer Pickup() {
    Offer offer;
    offer.vehicle.id = "truck-a";
    offer.vehicle.home = "home-a";
    offer.vehicle.enrolled = true;
    offer.vehicle.homeReady = true;
    offer.vehicle.idle = true;
    offer.vehicle.autopilot = true;
    offer.vehicle.fuelReady = true;
    offer.vehicle.cargoCapacity = 48;
    offer.requester.id = "requester-a";
    offer.requester.label = "Factory";
    offer.requester.resource = "/Game/Desc_IronPlate";
    offer.requester.explicitFilter = true;
    offer.requester.powered = true;
    offer.requester.capacity = 200;
    Station provider;
    provider.id = "provider-a";
    provider.label = "Mine";
    provider.resource = offer.requester.resource;
    provider.powered = true;
    provider.loadMode = true;
    provider.kinds = 1;
    provider.quantity = 96;
    provider.capacity = 200;
    offer.provider = provider;
    offer.roads.homeToProvider = 120;
    offer.roads.providerToRequester = 220;
    offer.roads.requesterToHome = 170;
    return offer;
}

int main() {
    int checks = 0;
    Offer p = Pickup();
    auto candidate = Evaluate(p);
    CHECK(candidate.has_value());
    CHECK(candidate->kind == Kind::Pickup);
    CHECK(candidate->amount == 48);
    CHECK(candidate->roadDistance == 510);
    CHECK(candidate->resource == p.requester.resource);
    CHECK(!IsStorage("Plain Station"));
    CHECK(IsStorage("[S] Warehouse"));
    CHECK(IsStorage("Warehouse [s] North"));
    CHECK(!IsStorage("[SKIP] Factory"));
    CHECK(PairAllowed("Factory", "Factory [S]"));
    CHECK(!PairAllowed("Factory [s]", "Factory [S]"));

    Offer withOnlyOneStorage = p;
    withOnlyOneStorage.provider->label = "[S] Warehouse";
    CHECK(Evaluate(withOnlyOneStorage).has_value());
    withOnlyOneStorage.requester.label = "Store [S]";
    CHECK(!Evaluate(withOnlyOneStorage));

    Offer missingFilter = p;
    missingFilter.requester.explicitFilter = false;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.requester.resource.clear();
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.provider->explicitFilter = false; // ordinary vanilla LOAD station accepted
    CHECK(Evaluate(missingFilter));
    missingFilter.provider->kinds = 2;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.provider->quantity = 47; // no partial provider pickup
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.provider->resource = "/Game/Desc_Copper";
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.requester.capacity = 47;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.requester.quantity = 1;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.requester.kinds = 1;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.provider->occupied = true;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.requester.paused = true;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.requester.depot = true;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.provider->powered = false;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.vehicle.driver = true;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.vehicle.autopilot = false;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.vehicle.fuelReady = false;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.vehicle.reserved = true;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.vehicle.homeReady = false;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.roads.providerToRequester = -1;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.roads.requesterToHome = std::numeric_limits<double>::quiet_NaN();
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.roads.homeToProvider = std::numeric_limits<double>::infinity();
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.roads.homeToProvider = 1e308;
    missingFilter.roads.providerToRequester = 1e308; // overflow in sum
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.provider->id = p.requester.id;
    CHECK(!Evaluate(missingFilter));
    missingFilter = p;
    missingFilter.provider->id = p.vehicle.home;
    CHECK(!Evaluate(missingFilter));

    Offer cargo = p;
    cargo.provider = std::nullopt;
    cargo.vehicle.cargoResource = p.requester.resource;
    cargo.vehicle.cargoKinds = 1;
    cargo.vehicle.cargoQuantity = 12;
    cargo.roads.homeToRequester = 200;
    candidate = Evaluate(cargo);
    CHECK(candidate.has_value());
    CHECK(candidate->kind == Kind::RetainedCargo);
    CHECK(candidate->amount == 12); // partial delivery is allowed for retained cargo
    CHECK(candidate->provider.empty());
    CHECK(candidate->roadDistance == 370);
    CHECK(Select({p, cargo})->kind == Kind::RetainedCargo);
    cargo.requester.label = "Storage [S]";
    CHECK(!Evaluate(cargo));
    cargo.requester.label = "Factory";
    cargo.vehicle.cargoKinds = 2;
    CHECK(!Evaluate(cargo));
    cargo.vehicle.cargoKinds = 1;
    cargo.vehicle.cargoResource = "/Game/Desc_Copper";
    CHECK(!Evaluate(cargo));
    cargo.vehicle.cargoResource = p.requester.resource;
    cargo.requester.capacity = 11;
    CHECK(!Evaluate(cargo));
    cargo.requester.capacity = 200;
    cargo.roads.homeToRequester = -1;
    CHECK(!Evaluate(cargo));
    cargo.roads.homeToRequester = 200;
    cargo.vehicle.cargoQuantity = 50;
    CHECK(!Evaluate(cargo));

    Offer p2 = p;
    p2.vehicle.id = "truck-b";
    p2.vehicle.home = "home-b";
    p2.provider->id = "provider-b";
    p2.roads.homeToProvider = 10;
    p2.roads.providerToRequester = 10;
    p2.roads.requesterToHome = 10;
    CHECK(Select({p, p2})->home == "home-b"); // directed road length, no Euclidean proxy
    p2.roads.homeToProvider = 120;
    p2.roads.providerToRequester = 220;
    p2.roads.requesterToHome = 170;
    CHECK(Select({p2, p})->home == "home-a"); // stable tie break
    CHECK(Select({p, p2})->home == "home-a");
    CHECK(!Select({}));
    CHECK(!Select({missingFilter}));

    Offer nearest = p;
    nearest.provider->id = "nearest-provider";
    nearest.roads.providerToRequester = 1;
    nearest.roads.homeToProvider = 10000;
    CHECK(Select({p, nearest})->provider == "nearest-provider");
    CHECK(Select({nearest, p})->provider == "nearest-provider");
    nearest.roads.providerToRequester = p.roads.providerToRequester;
    CHECK(Select({nearest, p})->provider == p.provider->id);
    Offer fluid = p;
    fluid.requester.medium = Medium::Fluid;
    CHECK(!Evaluate(fluid)); // solid truck cannot service fluid requester
    fluid.vehicle.medium = Medium::Fluid;
    CHECK(!Evaluate(fluid)); // solid provider cannot service fluid requester
    fluid.provider->medium = Medium::Fluid;
    CHECK(Evaluate(fluid));
    fluid.provider->resource = "/Game/Desc_Water";
    CHECK(!Evaluate(fluid));
    fluid.provider->resource = fluid.requester.resource;
    fluid.provider->quantity = fluid.vehicle.cargoCapacity - 1;
    CHECK(!Evaluate(fluid));
    fluid.provider->quantity = fluid.vehicle.cargoCapacity;
    CHECK(Evaluate(fluid));
    fluid.provider->label = "[s] Tank";
    fluid.requester.label = "[S] Tank";
    CHECK(!Evaluate(fluid));
    fluid.provider.reset();
    fluid.vehicle.cargoResource = fluid.requester.resource;
    fluid.vehicle.cargoQuantity = 5;
    fluid.vehicle.cargoKinds = 1;
    fluid.roads.homeToRequester = 10;
    CHECK(!Evaluate(fluid)); // unknown-origin retained fluid into warehouse
    fluid.requester.label = "Factory";
    CHECK(Evaluate(fluid)->amount == 5);
    fluid.vehicle.driver = true;
    CHECK(!Evaluate(fluid));

    cargo = p;
    cargo.provider.reset(); cargo.vehicle.cargoResource = p.requester.resource;
    cargo.vehicle.cargoKinds = 1; cargo.vehicle.cargoQuantity = 5;
    cargo.roads.homeToRequester = 10; cargo.requester.label = "[s] warehouse";
    CHECK(!Evaluate(cargo));
    cargo.vehicle.cargoOriginKnown = true;
    CHECK(Evaluate(cargo));
    cargo.vehicle.cargoOriginStorage = true;
    CHECK(!Evaluate(cargo));
    std::cout << checks << " dispatch policy checks passed (portable, no game API exercised)\n";
}
