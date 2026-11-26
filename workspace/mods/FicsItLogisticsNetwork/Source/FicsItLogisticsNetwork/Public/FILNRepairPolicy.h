// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace filn::repair {
enum class SlotAction { Keep, RemoveLegacyRestriction, Retry };
template<class Descriptor>
SlotAction Slot(const Descriptor& Original,const Descriptor& Actual,const Descriptor& Resource) {
 if(Actual==Original) return SlotAction::Keep;
 if(!Original && Resource && Actual==Resource) return SlotAction::RemoveLegacyRestriction;
 return SlotAction::Retry;
}
inline bool CargoUnchanged(int Before,int After,bool SameDescriptor) {
 return Before>=0 && Before==After && (Before==0 || SameDescriptor);
}
inline bool CanRestoreJob(bool ValidState,bool OwnLease,bool RouteMatches,bool EndpointsReady) {
 return ValidState && OwnLease && RouteMatches && EndpointsReady;
}
}
