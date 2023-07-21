// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

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
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"
#include "core/hle/service/frd/frd_u.h"
#include "core/hle/service/fs/fs_user.h"
#include "network/network_clients/nasc.h"

SERVICE_CONSTRUCT_IMPL(Service::FRD::Module)

namespace Service::FRD {

Module::Interface::Interface(std::shared_ptr<Module> frd, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), frd(std::move(frd)) {}

Module::Interface::~Interface() = default;

// just a little helper for u16 arrays
template <size_t size>
std::string ConvertU16ArrayToString(std::array<u16, size> u16Array) {
    std::string result;
    for (u32 i = 0; i < size; i++) {
        char lowerByte = static_cast<char>(u16Array[i]);
        if (u16Array[i] == 0x00)
            break;
        result += lowerByte;
    }
    return result;
}

void PIDToFC(u32 &principalId, u64 *friendCode) {

    // Convert u32 to a byte array (Little-Endian assumed)
    u8 byteArray[4];
    byteArray[0] = static_cast<u8>(principalId);
    byteArray[1] = static_cast<u8>(principalId >> 8);
    byteArray[2] = static_cast<u8>(principalId >> 16);
    byteArray[3] = static_cast<u8>(principalId >> 24);

    // Feed the byte array into Boost SHA1 hashing function
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(byteArray, 4);

    // Get the SHA1 digest (160-bit)
    u32 hashDigest[5];
    sha1.get_digest(hashDigest);

    // Perform a bitshift on the digest to get the "first" byte of the digest and shift that byte by 1
    u8 shiftedValue = hashDigest[0] >> 25;

    // Combine the principalId and checksum byte into a u64
    // make principalId a u64 by doing a bitwise and with 0xFFFFFFFF, and get the value of shiftedValue as a u64 by casting it to a u64 then shifting left 32
    // you could also accomplish this by shifting principalId right by 32 and shiftedValue like normal, but to avoid compiler warnings, doing a bitwise and is the best option
    *friendCode = (principalId & 0xFFFFFFFF) | static_cast<u64>(shiftedValue) << 32;
}

// Load file helper
bool LoadFile(size_t classSize, void *loadStruct, const char *path) {
    std::string file = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + *path;
    if (FileUtil::Exists(file)) {
        FileUtil::IOFile iofile(file, "rb");
        if (iofile.IsOpen() && iofile.GetSize() == classSize) {
            iofile.ReadBytes(loadStruct, classSize);
        }
        else {
            iofile.Close();
            return false;
        }
    }
    else {
        return false;
    }

    return true;
}

// Load friendlist file helper
bool LoadFriendlistFile(size_t classSize, size_t checkSize, u32 arraySize, void *loadStruct, u32 *loadInt, const char *path) {
    std::string file = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + *path;
    if (FileUtil::Exists(file)) {
        FileUtil::IOFile iofile(file, "rb");
        if (iofile.IsOpen() && iofile.GetSize() % (classSize - checkSize) == 0) {
            iofile.ReadBytes(loadStruct, classSize);

            if (iofile.GetSize() - checkSize > 0) {
                *loadInt = (iofile.GetSize() - (classSize - checkSize)) / arraySize;
            }
            else {
                *loadInt = 0;
            }
        }
        else {
            iofile.Close();
            return false;
        }
    }
    else {
        return false;
    }

    return true;
}

void Module::Interface::HasLoggedIn(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);
    rb.Push(frd->has_logged_in);
}

void Module::Interface::Login(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x3, 0, 2);
    auto event = rp.PopObject<Kernel::Event>();

    frd->has_logged_in = true;
    event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::Logout(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x4, 0, 0);

    frd->has_logged_in = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetMyFriendKey(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);

    u64 friendCode;
    PIDToFC(frd->account.principal_id, &friendCode);

    rb.PushRaw(friendCode);
    rb.Push<u32>(0);
    rb.PushRaw(frd->account.principal_id);
}

void Module::Interface::GetMyPreference(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x6, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(frd->my_data.my_pref_public_mode);
    rb.Push<u32>(frd->my_data.my_pref_public_game_name);
    rb.Push<u32>(frd->my_data.my_pref_public_played_game);
}

void Module::Interface::GetMyProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x7, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<FriendProfile>(frd->my_data.profile);
}

void Module::Interface::GetMyPresence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 0, 0);

    std::vector<u8> buffer(sizeof(FriendPresence));
    std::memcpy(buffer.data(), &frd->my_presence, buffer.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);
}
void Module::Interface::GetMyScreenName(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(7, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<std::array<u16_le, FRIEND_SCREEN_NAME_SIZE>>(frd->my_data.screen_name);
}

void Module::Interface::GetMyMii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xA, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(25, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<Mii::ChecksummedMiiData>(frd->my_data.mii);
}

void Module::Interface::GetMyFavoriteGame(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xD, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<TitleData>(frd->my_data.favorite_game);
}

void Module::Interface::GetMyComment(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xF, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(10, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<std::array<u16_le, FRIEND_COMMENT_SIZE>>(frd->my_data.comment);
}

void Module::Interface::GetMyPassword(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 1, 0);
    u32 pass_len = rp.Pop<u32>();
    std::vector<u8> pass_buf(pass_len);


    strncpy(reinterpret_cast<char*>(pass_buf.data()), ConvertU16ArrayToString(frd->account.nex_password).c_str(),
            pass_len - 1);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(pass_buf), 0);
}

void Module::Interface::GetFriendKeyList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 2, 0);
    const u32 offset = rp.Pop<u32>();
    const u32 frd_count = rp.Pop<u32>();

    u32 start = offset;
    u32 end = std::min(offset + frd_count, frd->my_friend_count);
    std::vector<u8> buffer(sizeof(FriendKey) * (end - start), 0);
    FriendKey* buffer_ptr = reinterpret_cast<FriendKey*>(buffer.data());
    u32 count = 0;
    while (start < end) {
        if (frd->friendlist.friends[start].friendKey.principalId) {
            buffer_ptr[count++] = frd->friendlist.friends[start].friendKey;
        } else {
            break;
        }
        ++start;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(count);
    rb.PushStaticBuffer(std::move(buffer), 0);
}

void Module::Interface::GetFriendPresence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 1, 2);
    const u32 count = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    std::vector<u8> buffer(sizeof(FriendPresence) * count, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::GetFriendMii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 1, 4);
    const u32 count = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));
    auto out_mii_buffer = rp.PopMappedBuffer();
    ASSERT(out_mii_buffer.GetSize() == count * sizeof(Mii::ChecksummedMiiData));

    const FriendKey* friend_keys_data = reinterpret_cast<const FriendKey*>(frd_keys.data());
    std::vector<u8> out_mii_vector(count * sizeof(Mii::ChecksummedMiiData));
    Mii::ChecksummedMiiData* out_mii_data =
        reinterpret_cast<Mii::ChecksummedMiiData*>(out_mii_vector.data());

    for (u32 i = 0; i < count; i++) {
        auto friend_info = frd->friendlist.GetFriendInfo(friend_keys_data[i]);
        if (friend_info.has_value()) {
            out_mii_data[i] = friend_info.value()->mii_data;
        } else {
            out_mii_data[i] = Mii::ChecksummedMiiData();
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);

    out_mii_buffer.Write(out_mii_vector.data(), 0,
                         out_mii_vector.size() * sizeof(Mii::ChecksummedMiiData));
    rb.PushMappedBuffer(out_mii_buffer);
}

void Module::Interface::GetFriendProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 1, 2);
    const u32 count = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    std::vector<u8> buffer(sizeof(FriendProfile) * count, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::GetFriendAttributeFlags(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 1, 2);
    const u32 count = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    // TODO:(mailwl) figure out AttributeFlag size and zero all buffer. Assume 1 byte
    std::vector<u8> buffer(sizeof(u32) * count, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::UnscrambleLocalFriendCode(Kernel::HLERequestContext& ctx) {
    const std::size_t scrambled_friend_code_size = 12;
    const std::size_t friend_code_size = 8;

    IPC::RequestParser rp(ctx, 0x1C, 1, 2);
    const u32 friend_code_count = rp.Pop<u32>();
    const std::vector<u8> scrambled_friend_codes = rp.PopStaticBuffer();
    ASSERT_MSG(scrambled_friend_codes.size() == (friend_code_count * scrambled_friend_code_size),
               "Wrong input buffer size");

    std::vector<u8> unscrambled_friend_codes(friend_code_count * friend_code_size, 0);
    // TODO(B3N30): Unscramble the codes and compare them against the friend list
    //              Only write 0 if the code isn't in friend list, otherwise write the
    //              unscrambled one
    //
    // Code for unscrambling (should be compared to HW):
    // std::array<u16, 6> scambled_friend_code;
    // Memory::ReadBlock(scrambled_friend_codes+(current*scrambled_friend_code_size),
    // scambled_friend_code.data(), scrambled_friend_code_size); std::array<u16, 4>
    // unscrambled_friend_code; unscrambled_friend_code[0] = scambled_friend_code[0] ^
    // scambled_friend_code[5]; unscrambled_friend_code[1] = scambled_friend_code[1] ^
    // scambled_friend_code[5]; unscrambled_friend_code[2] = scambled_friend_code[2] ^
    // scambled_friend_code[5]; unscrambled_friend_code[3] = scambled_friend_code[3] ^
    // scambled_friend_code[5];

    LOG_WARNING(Service_FRD, "(STUBBED) called");
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(unscrambled_friend_codes), 0);
}

void Module::Interface::AttachToEventNotification(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x20, 0, 2);
    frd->notif_event = rp.PopObject<Kernel::Event>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::SetNotificationMask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x21, 1, 0);
    frd->notif_event_mask = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetLastResponseResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x23, 0, 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::RequestGameAuthentication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x28, 9, 4);
    u32 gameID = rp.Pop<u32>();

    struct ScreenNameIPC {
        // 24 bytes
        std::array<char16_t, 12> name;
    };
    auto screenName = rp.PopRaw<ScreenNameIPC>();
    auto sdkMajor = rp.Pop<u32>();
    auto sdkMinor = rp.Pop<u32>();
    auto processID = rp.PopPID();
    auto process = frd->system.Kernel().GetProcessById(processID);
    auto event = rp.PopObject<Kernel::Event>();

    event->Signal();
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (frd->account.nex_password[0] == '\0' || frd->account.principal_id_hmac[0] == '\0') {
        LOG_ERROR(Service_FRD, "called, but no account data is present!");
        rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::Friends,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }
    auto fs_user =
        Core::System::GetInstance().ServiceManager().GetService<Service::FS::FS_USER>("fs:USER");
    Service::FS::FS_USER::ProductInfo product_info;

    if (!fs_user->GetProductInfo(processID, product_info)) {
        LOG_ERROR(Service_FRD, "called, but no game product info is available!");
        rb.Push(ResultCode(ErrorDescription::NoData, ErrorModule::Friends,
                           ErrorSummary::InvalidState, ErrorLevel::Status));
        return;
    }

    std::string nasc_url = frd->account.nasc_environment == NascEnvironment::Prod ? "https://nasc.nintendowifi.net/ac" : "https://nasc.pretendo.cc/ac/";
    NetworkClient::NASC::NASCClient nasc_client(
        nasc_url,
        std::vector<u8>(std::begin(frd->my_account_data.ctr_common_prod_cert),
                        std::end(frd->my_account_data.ctr_common_prod_cert)),
        std::vector<u8>(std::begin(frd->my_account_data.ctr_common_prod_key),
                        std::end(frd->my_account_data.ctr_common_prod_key)));
    nasc_client.SetParameter("gameid", fmt::format("{:08X}", gameID));
    nasc_client.SetParameter("sdkver", fmt::format("{:03d}{:03d}", (u8)sdkMajor, (u8)sdkMinor));
    nasc_client.SetParameter("titleid", fmt::format("{:016X}", process->codeset->program_id));
    nasc_client.SetParameter(
        "gamecd", std::string(reinterpret_cast<char*>(product_info.product_code.data() + 6)));
    nasc_client.SetParameter("gamever", fmt::format("{:04X}", product_info.remaster_version));
    nasc_client.SetParameter("mediatype", 1);
    char makercd[3];
    makercd[0] = (product_info.maker_code >> 8);
    makercd[1] = (product_info.maker_code & 0xFF);
    makercd[2] = '\0';
    nasc_client.SetParameter("makercd", std::string(makercd));
    nasc_client.SetParameter("unitcd", (int)frd->my_data.profile.platform);
    const u8* mac = frd->my_data.mii.GetMiiData().mac.data();
    nasc_client.SetParameter("macadr", fmt::format("{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", mac[0],
                                                   mac[1], mac[2], mac[3], mac[4], mac[5]));
    nasc_client.SetParameter("bssid", std::string("000000000000"));
    nasc_client.SetParameter("apinfo", std::string("01:0000000000"));

    {
        time_t raw_time;
        struct tm* time_info;
        char time_buffer[80];

        time(&raw_time);
        time_info = localtime(&raw_time);

        strftime(time_buffer, sizeof(time_buffer), "%y%m%d%H%M%S", time_info);
        nasc_client.SetParameter("devtime", std::string(time_buffer));
    }

    std::vector<u8> device_cert(sizeof(frd->my_account_data.device_cert));
    memcpy(device_cert.data(), frd->my_account_data.device_cert.data(), device_cert.size());
    nasc_client.SetParameter("fcdcert", device_cert);
    std::vector<u8> device_name;
    for (int i = 0;
         i < (sizeof(frd->my_data.unk68.data()) / sizeof(u16)) - 1; i++) {
        u16_le unit = frd->my_data.unk68[i];
        if (!unit)
            break;
        device_name.push_back((u8)(unit & 0xFF));
        device_name.push_back((u8)(unit >> 8));
    }
    nasc_client.SetParameter("devname", device_name);
    nasc_client.SetParameter("servertype", std::string("L1"));
    nasc_client.SetParameter("fpdver", fmt::format("{:04X}", frd->fpd_version));
    nasc_client.SetParameter("lang",
                             fmt::format("{:02X}", frd->my_data.profile.language));
    nasc_client.SetParameter("region",
                             fmt::format("{:02X}", frd->my_data.profile.region));
    nasc_client.SetParameter("csnum", ConvertU16ArrayToString(frd->my_data.serial_number));
    nasc_client.SetParameter("uidhmac", ConvertU16ArrayToString(frd->account.principal_id_hmac));
    nasc_client.SetParameter("userid", (int)frd->account.principal_id);
    nasc_client.SetParameter("action", std::string("LOGIN"));
    nasc_client.SetParameter("ingamesn", std::string(""));

    LOG_INFO(Service_FRD, "Performing NASC request to: {}", nasc_url);
    NetworkClient::NASC::NASCClient::NASCResult nasc_result = nasc_client.Perform();
    frd->last_game_auth_data.Init();
    frd->last_game_auth_data.result = nasc_result.result;
    if (nasc_result.result != 1) {
        LOG_ERROR(Service_FRD, "NASC Error: {}", nasc_result.log_message);
        if (nasc_result.result != 0) {
            frd->last_game_auth_data.http_status_code = nasc_result.http_status;
        }
    } else {
        frd->last_game_auth_data.http_status_code = nasc_result.http_status;
        strncpy(frd->last_game_auth_data.server_address.data(), nasc_result.server_address.c_str(),
                sizeof(frd->last_game_auth_data.server_address) - 1);
        frd->last_game_auth_data.server_port = nasc_result.server_port;
        strncpy(frd->last_game_auth_data.auth_token.data(), nasc_result.auth_token.c_str(),
                sizeof(frd->last_game_auth_data.auth_token) - 1);
        frd->last_game_auth_data.server_time = nasc_result.time_stamp;
    }

    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetGameAuthenticationData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x29, 0, 0);

    std::vector<u8> out_auth_data(sizeof(GameAuthenticationData));
    memcpy(out_auth_data.data(), &frd->last_game_auth_data, sizeof(GameAuthenticationData));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(out_auth_data, 0);
}

void Module::Interface::SetClientSdkVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x32, 1, 2);
    u32 version = rp.Pop<u32>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_FRD, "(STUBBED) called, version: 0x{:08X}", version);
}

Module::Module(Core::System& system) : system(system) {
    if (!LoadFile(sizeof(FRDMyData), &my_data, "mydata")) {
        my_data = FRDMyData();
        LOG_INFO(Service_FRD, "No mydata file found, using default");
    }

    if (!LoadFile(sizeof(FRDAccount), &account, "account")) {
        account = FRDAccount();
        LOG_INFO(Service_FRD, "No account file found, using default");
    }

    if (!LoadFriendlistFile(sizeof(FRDFriendlist), sizeof(std::array<FriendInfo, FRIEND_LIST_SIZE>), sizeof(FriendInfo), &friendlist, &my_friend_count, "friendlist")) {
        friendlist = FRDFriendlist();
        LOG_INFO(Service_FRD, "No friendlist file found, using default");
    }

    if (!LoadFile(sizeof(FRDConfig), &config, "config")) {
        config = FRDConfig();
        LOG_INFO(Service_FRD, "No config file found, using default");
    }
};

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto frd = std::make_shared<Module>(system);
    std::make_shared<FRD_U>(frd)->InstallAsService(service_manager);
    std::make_shared<FRD_A>(frd)->InstallAsService(service_manager);
}


} // namespace Service::FRD
