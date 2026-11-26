#pragma once
#include <cstdint>
namespace filn::dockfilter {
enum class Action { Native, Block, SelectiveSolid };
inline Action Decide(bool Configured,bool Load,bool Valid,bool Matching,bool Other,bool Solid) {
 if(!Configured || Load) return Action::Native;
 if(!Valid || !Matching) return Action::Block;
 if(!Other) return Action::Native;
 return Solid?Action::SelectiveSolid:Action::Block;
}
inline bool Conserved(std::int64_t SourceBefore,std::int64_t SourceAfter,
                      std::int64_t TargetBefore,std::int64_t TargetAfter,int Added) {
 return Added>0 && SourceBefore>=Added && SourceAfter==SourceBefore-Added &&
        TargetBefore>=0 && TargetAfter>=TargetBefore && TargetAfter-TargetBefore==Added;
}
}
