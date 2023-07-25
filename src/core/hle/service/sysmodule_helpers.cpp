#include "sysmodule_helpers.h"

namespace SysmoduleHelpers {

    LocalFriendCodeSeed_B GetLFCS_B(u64_le local_friend_code_seed) {
        LocalFriendCodeSeed_B lfcsb = LocalFriendCodeSeed_B();
        lfcsb.local_friend_code_seed = local_friend_code_seed;
        lfcsb.bitflags = 0;
        return lfcsb;
    }
};