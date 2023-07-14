// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/hle/mii.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::FRD {

constexpr u32 FRIEND_SCREEN_NAME_SIZE = 0xB;            ///< 11-short UTF-16 screen name
constexpr u32 FRIEND_COMMENT_SIZE = 0x11;               ///< 17-short UTF-16 comment
constexpr u32 FRIEND_GAME_MODE_DESCRIPTION_SIZE = 0x80; ///< 128-short UTF-16 description
constexpr u32 FRIEND_LIST_SIZE = 0x64;

struct FriendKey {
    u32_le friend_id{};
    u32_le unknown{};
    u64_le friend_code{};

    bool operator==(const FriendKey& other) {
        return friend_id == other.friend_id;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& friend_id;
        ar& unknown;
        ar& friend_code;
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
    std::array<u16_le, FRIEND_GAME_MODE_DESCRIPTION_SIZE> description;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& game_mode;
        ar& description;
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
    std::array<u16_le, 11> user_name{};
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

struct FriendInfo {
    FriendKey friend_key{};
    s64_le account_creation_timestamp{};
    u8 relationship{};
    std::array<u8, 3> padding1{};
    u32_le padding2{};
    FriendProfile friend_profile{};
    TitleData favorite_game{};
    u32_le principal_ID{};
    u16_le comment[FRIEND_COMMENT_SIZE]{};
    u16_le padding3{};
    s64_le last_online_timestamp{};
    std::array<u16_le, FRIEND_SCREEN_NAME_SIZE> screen_name;
    u8 font_region{};
    u8 padding4{};
    Mii::ChecksummedMiiData mii_data{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& friend_key;
        ar& account_creation_timestamp;
        ar& relationship;
        ar& padding1;
        ar& padding2;
        ar& friend_profile;
        ar& favorite_game;
        ar& principal_ID;
        ar& comment;
        ar& padding3;
        ar& last_online_timestamp;
        ar& screen_name;
        ar& font_region;
        ar& padding4;
        ar& mii_data;
    }
    friend class boost::serialization::access;
};

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

    protected:
        std::shared_ptr<Module> frd;
    };

private:
    GameAuthenticationData last_game_auth_data{};
    FriendPresence my_presence{};
    Core::System& system;

    bool has_logged_in = false;
    u32 notif_event_mask = 0xF7;
    std::shared_ptr<Kernel::Event> notif_event;

    static const u32 fpd_version = 16;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& my_account_data;
        ar& last_game_auth_data;
        ar& my_presence;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::FRD

SERVICE_CONSTRUCT(Service::FRD::Module)
