// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string_view>
#include <vector>
#include <string>
namespace filn::legacy {
inline bool Guid(std::string_view s) {
 if(s.size()!=32) return false;
 bool nonzero=false;
 for(char c:s) { if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false; nonzero|=c!='0'; }
 return nonzero;
}
struct Entry { std::string home,provider,requester; };
inline bool Parse(std::string_view input,std::vector<Entry>& result) {
 // Parse the Lua journal as data only. Commit output only on complete success.
 if(input.size()>65536) return false;
 std::vector<Entry> entries;
 while(!input.empty()) {
  auto end=input.find('\n'); auto line=input.substr(0,end);
  if(!line.empty()&&line.back()=='\r') line.remove_suffix(1);
  if(line.empty()||entries.size()>=128) return false;
  auto a=line.find('\t'); if(a==line.npos) return false;
  auto b=line.find('\t',a+1); if(b==line.npos) return false;
  auto h=line.substr(0,a),p=line.substr(a+1,b-a-1),q=line.substr(b+1);
  if(!Guid(h)||!Guid(q)||(p!="-"&&!Guid(p))||h==q||p==h||p==q) return false;
  for(const auto& e:entries) if(e.home==h) return false;
  entries.push_back({std::string(h),std::string(p),std::string(q)});
  if(end==input.npos) break;
  input.remove_prefix(end+1);
 }
 result=std::move(entries); return true;
}
enum class RouteMatch { None, Idle, Pickup, Delivery };
inline RouteMatch Match(const Entry& e,const std::vector<std::string>& r) {
 if(r==std::vector<std::string>{e.home}) return RouteMatch::Idle;
 if(e.provider!="-"&&(r==std::vector<std::string>{e.provider,e.requester,e.home}||
    r==std::vector<std::string>{e.home,e.provider,e.requester,e.home})) return RouteMatch::Pickup;
 if(r==std::vector<std::string>{e.home,e.requester,e.home}||r==std::vector<std::string>{e.requester,e.home}) return RouteMatch::Delivery;
 return RouteMatch::None;
}
enum class RestoreAction { Reject, Deliver, Return, Pickup };
inline RestoreAction Restore(bool exclusive,bool mixed,int quantity,bool targetRequester,bool targetHome,bool deliveryOnly,bool storage) {
 if(!exclusive||mixed||quantity<0||!deliveryOnly) return RestoreAction::Reject;
 if(quantity>0 && targetRequester && !storage) return RestoreAction::Deliver;
 if(quantity==0 && targetHome) return RestoreAction::Return;
 return RestoreAction::Reject;
}
// The Lua pickup route can safely be observed after its provider has been
// passed. Require the exact remaining suffix, not merely a matching waypoint.
inline bool SafeRemainingRoute(const Entry& e,const std::vector<std::string>& route,int target) {
 if(Match(e,route)==RouteMatch::None || target<0 || static_cast<std::size_t>(target)>=route.size()) return false;
 const auto i=static_cast<std::size_t>(target);
 return (i+1==route.size() && route[i]==e.home) ||
        (i+2==route.size() && route[i]==e.requester && route[i+1]==e.home);
}
inline bool PickupSuffix(const Entry& e,const std::vector<std::string>& route,int target) {
 if(Match(e,route)!=RouteMatch::Pickup || target<0 || static_cast<std::size_t>(target)>=route.size()) return false;
 const auto i=static_cast<std::size_t>(target);
 return i+3==route.size() && route[i]==e.provider && route[i+1]==e.requester && route[i+2]==e.home;
}
inline bool CanResumePickup(bool exclusive,bool mixed,int quantity,bool exactSuffix,bool providerEligible,bool requesterEligible,bool mediumCompatible,bool warehouseAllowed,bool roadsProved,int fullLoad,int providerStock,int requesterCapacity) {
 return exclusive&&!mixed&&quantity==0&&exactSuffix&&providerEligible&&requesterEligible&&mediumCompatible&&warehouseAllowed&&roadsProved&&
        fullLoad>0&&providerStock>=fullLoad&&requesterCapacity>=fullLoad;
}
inline bool IdleAdoption(bool exactRecipe,bool validHome,bool uniqueHome,bool uniqueTruck,bool ap,bool driver,bool empty,bool stopped,bool nearby,bool homeOnly) {
 return exactRecipe&&validHome&&uniqueHome&&uniqueTruck&&ap&&!driver&&empty&&stopped&&nearby&&homeOnly;
}
}
