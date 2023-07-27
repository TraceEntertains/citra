// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/uuid/detail/sha1.hpp>
#include <memory>
#include "common/common_types.h"
#include "core/hle/mii.h"
#include "core/hle/service/service.h"
#include <array>
#include <cstring>
#include <ctime>
#include <vector>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/sysmodule_helpers.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/http_c.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/file_sys/file_backend.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"
#include "network/network_clients/nasc.h"

namespace Core {
class System;
}

namespace Service::FRD {

constexpr u32 FRIEND_SCREEN_NAME_SIZE = 0xB;            ///< 11-short UTF-16 screen name
constexpr u32 FRIEND_COMMENT_SIZE = 0x10;               ///< 16-short UTF-16 comment
constexpr u32 FRIEND_GAME_MODE_DESCRIPTION_SIZE = 0x80; ///< 128-short UTF-16 description
constexpr u32 FRIEND_LIST_SIZE = 0x64;
constexpr u32 MAGIC_NUMBER = 0x20101021;

struct FriendKey {
    u32_le principalId{};
    u32_le unknown{};
    u64_le localFriendCode{};

    bool operator==(const FriendKey& other) {
        return principalId == other.principalId;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& principalId;
        ar& unknown;
        ar& localFriendCode;
    }
    friend class boost::serialization::access;
};

struct GameMode {
    u32_le join_flags{};
    u32_le type{};
    u32_le game_ID{};
    u32_le game_mode{};
    u32_le host_principal_ID{};
    u32_le gathering_ID{};
    std::array<u8, 20> app_args{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& join_flags;
        ar& type;
        ar& game_ID;
        ar& game_mode;
        ar& host_principal_ID;
        ar& gathering_ID;
        ar& app_args;
    }
    friend class boost::serialization::access;
};

struct FriendPresence {
    GameMode game_mode;
    u32_le unk;
    //std::array<u16_le, FRIEND_GAME_MODE_DESCRIPTION_SIZE> description;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& game_mode;
        //ar& description;
        ar& unk;
    }
    friend class boost::serialization::access;
};

struct GameAuthenticationData {
    s32_le result{};
    s32_le http_status_code{};
    std::array<char, 32> server_address{};
    u16_le server_port{};
    u16_le padding1{};
    u32_le unused{};
    std::array<char, 256> auth_token{};
    u64_le server_time{};

    void Init() {
        memset(this, 0, sizeof(*this));
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& result;
        ar& http_status_code;
        ar& server_address;
        ar& server_port;
        ar& padding1;
        ar& unused;
        ar& auth_token;
        ar& server_time;
    }
    friend class boost::serialization::access;
};

struct UserNameData {
    std::array<u16_le, FRIEND_SCREEN_NAME_SIZE> user_name{};
    u8 is_bad_word{};
    u8 unknown{};
    u32_le bad_word_ver{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& user_name;
        ar& is_bad_word;
        ar& unknown;
        ar& bad_word_ver;
    }
    friend class boost::serialization::access;
};

struct FriendProfile {
    u8 region{};
    u8 country{};
    u8 area{};
    u8 language{};
    u8 platform{};
    std::array<u8, 3> padding{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& region;
        ar& country;
        ar& area;
        ar& language;
        ar& platform;
        ar& padding;
    }
    friend class boost::serialization::access;
};

struct TitleData {
    u64_le tid{};
    u32_le version{};
    u32_le unk{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& tid;
        ar& version;
        ar& unk;
    }
    friend class boost::serialization::access;
};

struct FriendEntry {
    FriendKey friendKey{};
    u32_le padding1{};
    u8 friendRelationship{};
    FriendProfile profile{};
    std::array<u8, 3> padding2{};
    TitleData favorite_game{};
    std::array<u16_le, FRIEND_COMMENT_SIZE> comment{};
    u64_le padding3{};
    SysmoduleHelpers::FormattedTimestamp timestamp1{};
    SysmoduleHelpers::FormattedTimestamp timestamp2{};
    SysmoduleHelpers::FormattedTimestamp last_online{};
    Mii::ChecksummedMiiData mii{};
    std::array<u16_le, FRIEND_SCREEN_NAME_SIZE> screen_name{};
    u8 unk82{};
    SysmoduleHelpers::TrivialCharacterSet character_set{};
    SysmoduleHelpers::FormattedTimestamp timestamp4{};
    SysmoduleHelpers::FormattedTimestamp timestamp5{};
    SysmoduleHelpers::FormattedTimestamp timestamp6{};

    u32 get_attribute() const {
        if (friendRelationship > 5)
            return 3;

        return friendRelationship;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& friendKey;
        ar& padding1;
        ar& friendRelationship;
        ar& profile;
        ar& padding2;
        ar& favorite_game;
        ar& comment;
        ar& padding3;
        ar& timestamp1;
        ar& timestamp2;
        ar& last_online;
        ar& mii;
        ar& screen_name;
        ar& unk82;
        ar& character_set;
        ar& timestamp4;
        ar& timestamp5;
        ar& timestamp6;
    }
    friend class boost::serialization::access;
};

class FriendInfo {
    FriendKey friendKey{};
    SysmoduleHelpers::SystemTimestamp timestamp1{};
    u8 friendRelationship{};
    std::array<u8, 3> padding1{};
    u32 unk28{};
    FriendProfile profile{};
    TitleData favorite_game{};
    u32 unk52{};
    std::array<u16_le, FRIEND_COMMENT_SIZE> comment{};
    u32 unk84{};
    SysmoduleHelpers::SystemTimestamp last_online{};
    std::array<u16_le, FRIEND_SCREEN_NAME_SIZE> screen_name{};
    SysmoduleHelpers::TrivialCharacterSet character_set{};
    u8 unk128{};
    Mii::ChecksummedMiiData mii{};

public:
    FriendInfo(const FriendEntry &frd) {
        friendKey = frd.friendKey;
        timestamp1 = SysmoduleHelpers::SystemTimestamp();
        friendRelationship = frd.friendRelationship;
        profile = frd.profile;
        favorite_game = frd.favorite_game;
        comment = frd.comment;
        last_online = SysmoduleHelpers::SystemTimestamp(frd.last_online);
        screen_name = frd.screen_name;
        character_set = frd.character_set;
        mii = frd.mii;
    }

    FriendInfo() = default;
};

struct FRDMyData {
    static constexpr u32_le MAGIC_MY_DATA = 0x444D5046;

    u32_le magic{MAGIC_MY_DATA};
    u32_le magic_number{MAGIC_NUMBER};
    u64_le padding1{};
    u32_le my_nc_principal_id{};
    u32_le unk18{};
    u32_le changed_bit_flags{};
    bool my_pref_public_mode{};
    bool my_pref_public_game_name{};
    bool my_pref_public_played_game{};
    TitleData favorite_game{};
    std::array<u16_le, FRIEND_COMMENT_SIZE> comment{};
    u64_le padding2{}; // likely padding
    FriendProfile profile{};
    u64_le local_friend_code_seed{};
    std::array<u16_le, 0xD> unk68{}; // utf16 hex identifier?
    std::array<u16_le, 0x10> serial_number{};
    std::array<u16_le, FRIEND_SCREEN_NAME_SIZE> screen_name{};
    std::array<u8, 3> padding3{};
    Mii::ChecksummedMiiData mii{};
    std::array<u8, 5> padding4{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& magic;
        ar& magic_number;
        ar& padding1;
        ar& my_nc_principal_id;
        ar& unk18;
        ar& changed_bit_flags;
        ar& my_pref_public_mode;
        ar& my_pref_public_game_name;
        ar& my_pref_public_played_game;
        ar& favorite_game;
        ar& comment;
        ar& padding2;
        ar& profile;
        ar& local_friend_code_seed;
        ar& unk68;
        ar& serial_number;
        ar& screen_name;
        ar& padding3;
        ar& mii;
        ar& padding4;
    }
    friend class boost::serialization::access;
};

struct FRDAccount {
    static constexpr u32_le MAGIC_ACCOUNT = 0x43415046;

    u32_le magic{MAGIC_ACCOUNT};
    u32_le magic_number{MAGIC_NUMBER};
    u64_le padding1{};
    SysmoduleHelpers::LongLocalAccountID local_account_id{};
    u32_le principal_id{};
    u64_le local_friend_code{};
    std::array<u16_le, 0x10> nex_password{};
    u16_le unk40;
    std::array<u16_le, 0x9> principal_id_hmac{};
    SysmoduleHelpers::NascEnvironment nasc_environment;
    u8 server_type_1;
    u8 server_type_2;
    u8 padding2{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& magic;
        ar& magic_number;
        ar& padding1;
        ar& local_account_id;
        ar& principal_id;
        ar& local_friend_code;
        ar& nex_password;
        ar& unk40;
        ar& principal_id_hmac;
        ar& nasc_environment;
        ar& server_type_1;
        ar& server_type_2;
        ar& padding2;
    }
    friend class boost::serialization::access;
};

struct FRDFriendlist {
    static constexpr u32_le MAGIC_FRIENDLIST = 0x4C465046;
    static u32 my_friend_count;

    u32_le magic{MAGIC_FRIENDLIST};
    u32_le magic_number{MAGIC_NUMBER};
    u64_le padding1{};
    std::array<FriendEntry, FRIEND_LIST_SIZE> friends{};

    std::optional<const FriendEntry*> GetFriendEntry(const FriendKey& key) {
        for (u32 i = 0; i < my_friend_count; i++) {
            if (friends[i].friendKey == key) {
                return &friends[i];
            }
        }
        return std::nullopt;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& magic;
        ar& magic_number;
        ar& padding1;
        ar& friends;
    }
    friend class boost::serialization::access;
};

#pragma pack(push, 1)
struct FRDConfig {
    static constexpr u32_le MAGIC_CONFIG = 0x46435046;

    u32_le magic{MAGIC_CONFIG};
    u32_le magic_number{MAGIC_NUMBER};
    u64_le padding1{};
    SysmoduleHelpers::LocalAccountID local_account_id{/*SysmoduleHelpers::LocalAccountID::Test*/};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& magic;
        ar& magic_number;
        ar& padding1;
        ar& local_account_id;
    }
    friend class boost::serialization::access;
};
#pragma pack(pop)

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> frd, const char* name, u32 max_session);
        ~Interface();

    protected:
        void HasLoggedIn(Kernel::HLERequestContext& ctx);

        void Login(Kernel::HLERequestContext& ctx);

        void Logout(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyFriendKey service function
         *  Inputs:
         *      none
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2-5 : FriendKey
         */
        void GetMyFriendKey(Kernel::HLERequestContext& ctx);

        void GetMyPreference(Kernel::HLERequestContext& ctx);

        void GetMyProfile(Kernel::HLERequestContext& ctx);


        /**
         * FRD::GetMyPresence service function
         *  Inputs:
         *      64 : sizeof (MyPresence) << 14 | 2
         *      65 : Address of MyPresence structure
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetMyPresence(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyScreenName service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : UTF16 encoded name (max 11 symbols)
         */
        void GetMyScreenName(Kernel::HLERequestContext& ctx);

        void GetMyMii(Kernel::HLERequestContext& ctx);

        void GetMyLocalAccountId(Kernel::HLERequestContext& ctx);

        void GetMyFavoriteGame(Kernel::HLERequestContext& ctx);

        void GetMyComment(Kernel::HLERequestContext& ctx);

        void GetMyPassword(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetFriendKeyList service function
         *  Inputs:
         *      1 : Unknown
         *      2 : Max friends count
         *      65 : Address of FriendKey List
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : FriendKey count filled
         */
        void GetFriendKeyList(Kernel::HLERequestContext& ctx);

        void GetFriendPresence(Kernel::HLERequestContext& ctx);

        void GetFriendScreenName(Kernel::HLERequestContext& ctx);

        void GetFriendMii(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetFriendProfile service function
         *  Inputs:
         *      1 : Friends count
         *      2 : Friends count << 18 | 2
         *      3 : Address of FriendKey List
         *      64 : (count * sizeof (Profile)) << 10 | 2
         *      65 : Address of Profiles List
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetFriendProfile(Kernel::HLERequestContext& ctx);

        void GetFriendRelationship(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetFriendAttributeFlags service function
         *  Inputs:
         *      1 : Friends count
         *      2 : Friends count << 18 | 2
         *      3 : Address of FriendKey List
         *      65 : Address of AttributeFlags
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetFriendAttributeFlags(Kernel::HLERequestContext& ctx);

        void GetFriendInfo(Kernel::HLERequestContext& ctx);

        void IsIncludedInFriendList(Kernel::HLERequestContext& ctx);

        /**
         * FRD::UnscrambleLocalFriendCode service function
         *  Inputs:
         *      1 : Friend code count
         *      2 : ((count * 12) << 14) | 0x402
         *      3 : Pointer to encoded friend codes. Each is 12 bytes large
         *      64 : ((count * 8) << 14) | 2
         *      65 : Pointer to write decoded local friend codes to. Each is 8 bytes large.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void UnscrambleLocalFriendCode(Kernel::HLERequestContext& ctx);

        void AttachToEventNotification(Kernel::HLERequestContext& ctx);

        void SetNotificationMask(Kernel::HLERequestContext& ctx);

        void GetLastResponseResult(Kernel::HLERequestContext& ctx);

        void PrincipalIdToFriendCode(Kernel::HLERequestContext& ctx);

        void RequestGameAuthentication(Kernel::HLERequestContext& ctx);

        void GetGameAuthenticationData(Kernel::HLERequestContext& ctx);

        /**
         * FRD::SetClientSdkVersion service function
         *  Inputs:
         *      1 : Used SDK Version
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetClientSdkVersion(Kernel::HLERequestContext& ctx);

        void SetLocalAccountId(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> frd;
    };

private:
    FRDConfig config;

    FRDMyData my_data;
    FRDAccount account;
    FRDFriendlist friendlist;

    std::unique_ptr<FileSys::ArchiveBackend> file_sys_handle;

    std::unique_ptr<FileSys::FileBackend> my_data_handle;
    std::unique_ptr<FileSys::FileBackend> account_handle;
    std::unique_ptr<FileSys::FileBackend> friendlist_handle;
    std::unique_ptr<FileSys::FileBackend> config_handle;

    GameAuthenticationData last_game_auth_data{};
    FriendPresence my_presence{};
    Core::System& system;

    bool has_logged_in = false;
    u32 notif_event_mask = 0xF7;
    std::shared_ptr<Kernel::Event> notif_event;

    const u32 fpd_version = 16;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& my_data;
        ar& account;
        ar& friendlist;
        ar& config;
        ar& last_game_auth_data;
        ar& my_presence;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::FRD

SERVICE_CONSTRUCT(Service::FRD::Module)
