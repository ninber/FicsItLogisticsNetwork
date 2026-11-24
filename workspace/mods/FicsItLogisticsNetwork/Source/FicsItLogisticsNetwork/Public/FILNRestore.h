// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <vector>
#include <cstddef>
namespace filn::restore {
enum class Result { Success, Rejected, RolledBack, Conflict };
// Read every original value before writing. Only revert our own observed writes.
// External changes are never overwritten, even on rollback.
template<class Value,class Read,class Write>
Result Transaction(const std::vector<Value>& before,const std::vector<Value>& after,Read read,Write write) {
 if(before.size()!=after.size()) return Result::Rejected;
 for(std::size_t i=0;i<before.size();++i) if(read(i)!=before[i]) return Result::Rejected;
 std::size_t attempted=0;
 bool success=true;
 for(std::size_t i=0;i<before.size();++i) {
  if(read(i)!=before[i]) { success=false; break; }
  attempted=i+1;
  if(before[i]!=after[i]) write(i,after[i]);
  if(read(i)!=after[i]) { success=false; break; }
 }
 if(success) for(std::size_t i=0;i<after.size();++i) if(read(i)!=after[i]) { success=false; break; }
 if(success) return Result::Success;
 bool conflict=false;
 while(attempted>0) {
  const auto i=--attempted;
  const auto current=read(i);
  if(current==before[i]) continue;
  if(current!=after[i]) { conflict=true; continue; }
  write(i,before[i]);
  if(read(i)!=before[i]) conflict=true;
 }
 for(std::size_t i=0;i<before.size();++i) if(read(i)!=before[i]) conflict=true;
 return conflict?Result::Conflict:Result::RolledBack;
}
}
