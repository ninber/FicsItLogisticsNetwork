#pragma once
namespace filn::depot {
struct Snapshot {
 bool authority=false, valid=false, reserved=false, cargoConfig=false, claimed=false, docked=false;
};
inline bool CanDesignate(const Snapshot& s) {
 return s.authority&&s.valid&&!s.reserved&&!s.cargoConfig&&(!s.docked||s.claimed);
}
inline bool CanRemove(const Snapshot& s) {
 return s.authority&&!s.reserved&&!s.claimed&&!s.docked&&!s.cargoConfig;
}
}
