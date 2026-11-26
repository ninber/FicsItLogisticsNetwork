// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace filn::identity {
// A non-default player name (including semantic [S] tags) is never edited.
// A name once considered is never changed again even if reset to default.
inline bool ShouldName(bool member,bool considered,bool defaultName) {
 return member && !considered && defaultName;
}
inline bool ProviderMember(bool configured,bool committedProvider,bool depot) {
 return configured || committedProvider || depot;
}
}
