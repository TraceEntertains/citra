// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"
#include "core/hle/service/frd/frd_u.h"

SERVICE_CONSTRUCT_IMPL(Service::FRD::Module)

namespace Service::FRD {

u32 FRDFriendlist::my_friend_count;

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

u64 PIDToFC(u32 &principalId) {

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
    return (principalId & 0xFFFFFFFF) | static_cast<u64>(shiftedValue) << 32;
}

u32 FCToPID(const u64 &friend_code) {
    return static_cast<u32>(friend_code);
}

void LoadFRDSaves(SysmoduleHelpers::LocalAccountID enumId, FRDMyData &my_data, FRDAccount &account, FRDFriendlist &friendlist, std::unique_ptr<FileSys::FileBackend> &my_data_handle, std::unique_ptr<FileSys::FileBackend> &account_handle, std::unique_ptr<FileSys::FileBackend> &friendlist_handle, std::unique_ptr<FileSys::ArchiveBackend> &file_sys_handle) {
    u32 id = static_cast<int>(enumId);

    if (!SysmoduleHelpers::LoadSave(sizeof(FRDMyData), my_data, fmt::format("/{}/mydata", id).c_str(), my_data_handle, file_sys_handle)) {
        my_data = FRDMyData();
        my_data_handle = nullptr;
        LOG_INFO(Service_FRD, "No mydata file found, using default");
    }

    if (!SysmoduleHelpers::LoadSave(sizeof(FRDAccount), account, fmt::format("/{}/account", id).c_str(), account_handle, file_sys_handle)) {
        account = FRDAccount();
        account_handle = nullptr;
        LOG_INFO(Service_FRD, "No account file found, using default");
    }

    if (!SysmoduleHelpers::LoadFlexSave(sizeof(FRDFriendlist), sizeof(std::array<FriendEntry, FRIEND_LIST_SIZE>), sizeof(FriendEntry), friendlist, &friendlist.my_friend_count, fmt::format("/{}/friendlist", id).c_str(), friendlist_handle, file_sys_handle)) {
        friendlist = FRDFriendlist();
        friendlist_handle = nullptr;
        LOG_INFO(Service_FRD, "No friendlist file found, using default");
    }
}

void LoadFRDConfig(FRDConfig &config, std::unique_ptr<FileSys::FileBackend> &config_handle, std::unique_ptr<FileSys::ArchiveBackend> &file_sys_handle) {
    if (!SysmoduleHelpers::LoadSave(sizeof(FRDConfig), config, "/config", config_handle, file_sys_handle)) {
        config = FRDConfig();
        config_handle = nullptr;
        LOG_INFO(Service_FRD, "No config file found, using default");
    }
}

void InitFileSys(std::unique_ptr<FileSys::ArchiveBackend> &file_sys_handle) {
    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010032
    constexpr std::array<u8, 8> frd_system_savedata_id{
        0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x01, 0x00,
    };
    FileSys::Path archive_path(frd_system_savedata_id);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERROR_NOT_FOUND) {
        // Format the archive to create the directories
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0);

        // Open it again to get a valid archive now that the folder exists
        file_sys_handle = systemsavedata_factory.Open(archive_path, 0).Unwrap();
    } else {
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the FRD SystemSaveData archive!");

        file_sys_handle = std::move(archive_result).Unwrap();
    }
}

void Module::Interface::HasLoggedIn(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);
    rb.Push(frd->has_logged_in);
}

void Module::Interface::Login(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto event = rp.PopObject<Kernel::Event>();

    frd->has_logged_in = true;
    event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::Logout(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    frd->has_logged_in = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetMyFriendKey(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);


    rb.Push(RESULT_SUCCESS);

    // friendkey
    rb.PushRaw(PIDToFC(frd->account.principal_id));
    rb.Push<u32>(0);
    rb.PushRaw(frd->account.principal_id);
}

void Module::Interface::GetMyPreference(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(frd->my_data.my_pref_public_mode);
    rb.Push<u32>(frd->my_data.my_pref_public_game_name);
    rb.Push<u32>(frd->my_data.my_pref_public_played_game);
}

void Module::Interface::GetMyProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<FriendProfile>(frd->my_data.profile);
}

void Module::Interface::GetMyPresence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    std::vector<u8> buffer(sizeof(FriendPresence));
    std::memcpy(buffer.data(), &frd->my_presence, buffer.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);
}
void Module::Interface::GetMyScreenName(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(7, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<std::array<u16_le, FRIEND_SCREEN_NAME_SIZE>>(frd->my_data.screen_name);
}

void Module::Interface::GetMyMii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(25, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<Mii::ChecksummedMiiData>(frd->my_data.mii);
}

void Module::Interface::GetMyLocalAccountId(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<SysmoduleHelpers::LongLocalAccountID>(frd->account.local_account_id);
}

void Module::Interface::GetMyFavoriteGame(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<TitleData>(frd->my_data.favorite_game);
}

void Module::Interface::GetMyComment(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(10, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<std::array<u16_le, FRIEND_COMMENT_SIZE>>(frd->my_data.comment);
}

void Module::Interface::GetMyPassword(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 pass_len = rp.Pop<u32>();
    std::vector<u8> pass_buf(pass_len);


    strncpy(reinterpret_cast<char*>(pass_buf.data()), ConvertU16ArrayToString(frd->account.nex_password).c_str(),
            pass_len - 1);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(pass_buf), 0);
}

void Module::Interface::GetFriendKeyList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 offset = rp.Pop<u32>();
    const u32 frd_count = rp.Pop<u32>();

    u32 start = offset;
    u32 end = std::min(offset + frd_count, frd->friendlist.my_friend_count);
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
    IPC::RequestParser rp(ctx);
    const u32 count = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    std::vector<u8> buffer(sizeof(FriendPresence) * count, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::GetFriendScreenName(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto max_screen_name_out = rp.Pop<u32>();
    auto max_string_language_out = rp.Pop<u32>();
    const u32 friend_key_count = std::min(rp.Pop<u32>(), FRIEND_LIST_SIZE);
    const u32 unk1 = rp.Pop<u32>();
    const u32 unk2 = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    const FriendKey* friend_keys_data = reinterpret_cast<const FriendKey*>(frd_keys.data());

    u32 count = std::min(friend_key_count, std::min(max_screen_name_out, max_string_language_out));

    std::vector<u8> friend_names_vector(count * (max_screen_name_out * 2));
    std::vector<u8> character_sets_vector(count * sizeof(SysmoduleHelpers::TrivialCharacterSet));
    std::array<u16_le, FRIEND_SCREEN_NAME_SIZE>* friend_names = reinterpret_cast<std::array<u16_le, FRIEND_SCREEN_NAME_SIZE>*>(friend_names_vector.data());
    SysmoduleHelpers::TrivialCharacterSet* character_sets = reinterpret_cast<SysmoduleHelpers::TrivialCharacterSet*>(character_sets_vector.data());

    for (u32 i = 0; i < count; i++) {
        auto friend_entry = frd->friendlist.GetFriendEntry(friend_keys_data[i]);
        if (friend_entry.has_value()) {
            friend_names[i] = friend_entry.value()->screen_name;
            character_sets[i] = friend_entry.value()->character_set;
        } else {
            character_sets[i] = SysmoduleHelpers::TrivialCharacterSet::JapanUsaEuropeAustralia;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(friend_names_vector), 0);
    rb.PushStaticBuffer(std::move(character_sets_vector), 1);
}


void Module::Interface::GetFriendMii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 count = std::min(rp.Pop<u32>(), FRIEND_LIST_SIZE);
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));
    auto out_mii_buffer = rp.PopMappedBuffer();
    ASSERT(out_mii_buffer.GetSize() == count * sizeof(Mii::ChecksummedMiiData));

    const FriendKey* friend_keys_data = reinterpret_cast<const FriendKey*>(frd_keys.data());
    std::vector<u8> out_mii_vector(count * sizeof(Mii::ChecksummedMiiData));
    Mii::ChecksummedMiiData* out_mii_data =
        reinterpret_cast<Mii::ChecksummedMiiData*>(out_mii_vector.data());

    for (u32 i = 0; i < count; i++) {
        auto friend_entry = frd->friendlist.GetFriendEntry(friend_keys_data[i]);
        if (friend_entry.has_value()) {
            out_mii_data[i] = friend_entry.value()->mii;
        } else {
            out_mii_data[i] = Mii::ChecksummedMiiData();
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);

    out_mii_buffer.Write(out_mii_vector.data(), 0, out_mii_vector.size());
    rb.PushMappedBuffer(out_mii_buffer);
}


void Module::Interface::GetFriendProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 count = std::min(rp.Pop<u32>(), FRIEND_LIST_SIZE);
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    const FriendKey* friend_keys_data = reinterpret_cast<const FriendKey*>(frd_keys.data());
    std::vector<u8> buffer(sizeof(FriendProfile) * count, 0);
    FriendProfile* out_friend_profile_data =
        reinterpret_cast<FriendProfile*>(buffer.data());

    for (u32 i = 0; i < count; i++) {
        auto friend_entry = frd->friendlist.GetFriendEntry(friend_keys_data[i]);
        if (friend_entry.has_value()) {
            out_friend_profile_data[i] = friend_entry.value()->profile;
        } else {
            out_friend_profile_data[i] = FriendProfile();
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);

    rb.PushStaticBuffer(std::move(buffer), 0);
}

void Module::Interface::GetFriendRelationship(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 count = std::min(rp.Pop<u32>(), FRIEND_LIST_SIZE);
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    const FriendKey* friend_keys_data = reinterpret_cast<const FriendKey*>(frd_keys.data());
    std::vector<u8> buffer(sizeof(u8) * count, 0);
    u8* out_friend_relationship_data =
        reinterpret_cast<u8*>(buffer.data());

    for (u32 i = 0; i < count; i++) {
        auto friend_entry = frd->friendlist.GetFriendEntry(friend_keys_data[i]);
        if (friend_entry.has_value()) {
            out_friend_relationship_data[i] = friend_entry.value()->friendRelationship;
        } else {
            out_friend_relationship_data[i] = 0;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);

    rb.PushStaticBuffer(std::move(buffer), 0);
}


void Module::Interface::GetFriendAttributeFlags(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 count = std::min(rp.Pop<u32>(), FRIEND_LIST_SIZE);
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    const FriendKey* friend_keys_data = reinterpret_cast<const FriendKey*>(frd_keys.data());
    std::vector<u8> buffer(sizeof(u32) * count, 0);
    u32* out_friend_attribute_data =
        reinterpret_cast<u32*>(buffer.data());

    for (u32 i = 0; i < count; i++) {
        auto friend_entry = frd->friendlist.GetFriendEntry(friend_keys_data[i]);
        if (friend_entry.has_value()) {
            out_friend_attribute_data[i] = friend_entry.value()->get_attribute();
        } else {
            out_friend_attribute_data[i] = 0;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);

    rb.PushStaticBuffer(std::move(buffer), 0);
}

void Module::Interface::GetFriendInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 count = std::min(rp.Pop<u32>(), FRIEND_LIST_SIZE);
    const u32 unk1 = rp.Pop<u32>();
    const u32 character_set = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));
    auto out_friend_entry_buffer = rp.PopMappedBuffer();
    LOG_INFO(Service_FRD, fmt::format("size: {}, count: {}, sizeof(FriendInfo)*count: {}", out_friend_entry_buffer.GetSize(), count, sizeof(FriendInfo) * count).c_str());
    ASSERT(out_friend_entry_buffer.GetSize() == count * sizeof(FriendInfo));


    const FriendKey* friend_keys_data = reinterpret_cast<const FriendKey*>(frd_keys.data());
    std::vector<u8> out_friend_entry_vector(count * sizeof(FriendInfo));
    FriendInfo* out_friend_entry_data =
        reinterpret_cast<FriendInfo*>(out_friend_entry_vector.data());

    for (u32 i = 0; i < count; i++) {
        auto friend_entry = frd->friendlist.GetFriendEntry(friend_keys_data[i]);
        if (friend_entry.has_value()) {
            out_friend_entry_data[i] = FriendInfo(*friend_entry.value());
        } else {
            out_friend_entry_data[i] = FriendInfo();
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);

    out_friend_entry_buffer.Write(out_friend_entry_vector.data(), 0, out_friend_entry_vector.size());
    rb.PushMappedBuffer(out_friend_entry_buffer);
}

void Module::Interface::IsIncludedInFriendList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const u64 friend_code = rp.Pop<u64>();
    bool isInFriendList = false;

    for (u32 i = 0; i < frd->friendlist.my_friend_count; i++) {
        if (FCToPID(friend_code) == frd->friendlist.friends[i].friendKey.principalId) {
            isInFriendList = true;
            break;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(int(isInFriendList));
}

void Module::Interface::UnscrambleLocalFriendCode(Kernel::HLERequestContext& ctx) {
    const std::size_t scrambled_friend_code_size = 12;
    const std::size_t friend_code_size = 8;

    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
    frd->notif_event = rp.PopObject<Kernel::Event>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::SetNotificationMask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    frd->notif_event_mask = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetLastResponseResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::PrincipalIdToFriendCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 principalId = rp.Pop<u32>();
    u32 hashByte = PIDToFC(principalId) >> 32;

    LOG_INFO(Service_FRD, fmt::format("PID: {}, Hash Byte: {}", principalId, hashByte).c_str());

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(principalId);
    rb.Push(hashByte);
}

void Module::Interface::RequestGameAuthentication(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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

    std::string nasc_url;
    switch (frd->account.nasc_environment) {
        case SysmoduleHelpers::NascEnvironment::Prod:
            nasc_url = "nasc.nintendowifi.net";
            break;
        case SysmoduleHelpers::NascEnvironment::Test:
            nasc_url = "nasc.pretendo.cc";
            break;
        case SysmoduleHelpers::NascEnvironment::Dev:
            nasc_url = "127.0.0.1";
            break;
    }
    NetworkClient::NASC::NASCClient nasc_client(nasc_url, Service::HTTP::HTTP_C::ClCertA.certificate, Service::HTTP::HTTP_C::ClCertA.private_key);
    nasc_client.SetParameter("gameid", fmt::format("{:08X}", gameID));
    nasc_client.SetParameter("sdkver", fmt::format("{:03d}{:03d}", (u8)sdkMajor, (u8)sdkMinor));
    nasc_client.SetParameter("titleid", fmt::format("{:016X}", process->codeset->program_id));
    nasc_client.SetParameter(
        "gamecd", std::string(reinterpret_cast<char*>(product_info.product_code.data() + 6)));
    nasc_client.SetParameter("gamever", fmt::format("{:04X}", product_info.remaster_version));
    nasc_client.SetParameter("mediatype", 1);
    char makercd[3];
    makercd[0] = (product_info.maker_code & 0xFF);
    makercd[1] = (product_info.maker_code >> 8);
    makercd[2] = '\0';
    nasc_client.SetParameter("makercd", std::string(makercd));
    nasc_client.SetParameter("unitcd", (int)frd->my_data.profile.platform);
    const u8* mac = frd->my_data.mii.mii_data.mac.data();
    nasc_client.SetParameter("macadr", fmt::format("{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", mac[0],
                                                   mac[1], mac[2], mac[3], mac[4], mac[5]));
    nasc_client.SetParameter("bssid", std::string("000000000000"));
    nasc_client.SetParameter("apinfo", std::string("01:0000000000"));

    Core::System& system = Core::System::GetInstance();
    auto cfg = Service::CFG::GetModule(system);
    ASSERT_MSG(cfg, "CFG Module missing!");

    std::vector<u8> device_cert(sizeof(Service::CFG::LocalFriendCodeSeed_B));
    Service::CFG::LocalFriendCodeSeed_B* device_cert_data =
        reinterpret_cast<Service::CFG::LocalFriendCodeSeed_B*>(device_cert.data());
    *device_cert_data = *cfg->GetLFCSData();
    nasc_client.SetParameter("fcdcert", device_cert);
    std::vector<u8> device_name;
    for (u32 i = 0;
         i < (sizeof(frd->my_data.screen_name) / sizeof(u16)) - 1; i++) {
        u16_le unit = frd->my_data.screen_name[i];
        if (!unit)
            break;
        device_name.push_back((u8)(unit & 0xFF));
        device_name.push_back((u8)(unit >> 8));
    }
    nasc_client.SetParameter("devname", device_name);
    nasc_client.SetParameter("servertype", std::string("L1"));
    nasc_client.SetParameter("fpdver", fmt::format("{:04X}", frd->fpd_version));

    {
        time_t raw_time;
        struct tm* time_info;
        char time_buffer[80];

        time(&raw_time);
        time_info = localtime(&raw_time);

        strftime(time_buffer, sizeof(time_buffer), "%y%m%d%H%M%S", time_info);
        nasc_client.SetParameter("devtime", std::string(time_buffer));
    }

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
    IPC::RequestParser rp(ctx);

    std::vector<u8> out_auth_data(sizeof(GameAuthenticationData));
    memcpy(out_auth_data.data(), &frd->last_game_auth_data, sizeof(GameAuthenticationData));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(out_auth_data, 0);
}

void Module::Interface::SetClientSdkVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 version = rp.Pop<u32>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_FRD, "(STUBBED) called, version: 0x{:08X}", version);
}

void Module::Interface::SetLocalAccountId(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 id = rp.Pop<u32>();

    // close any account data files if they arent a nullptr, then clear their data so they can be reloaded
    if (frd->my_data_handle != nullptr) frd->my_data_handle->Close();
    if (frd->account_handle != nullptr) frd->account_handle->Close();
    if (frd->friendlist_handle != nullptr) frd->friendlist_handle->Close();
    frd->my_data = {};
    frd->account = {};
    frd->friendlist = {};

    // if this function is being called and the config_handle is (somehow) a nullptr, load it
    if (frd->config_handle == nullptr) {
        LoadFRDConfig(frd->config, frd->config_handle, frd->file_sys_handle);
    }

    // cast the input id to a struct and then set the current config id to it, then log the id it changed to
    frd->config.local_account_id = static_cast<SysmoduleHelpers::LocalAccountID>(id);
    LOG_INFO(Service_FRD, fmt::format("{}", frd->config.local_account_id).c_str());

    // if the config handle isnt a nullptr), write the current config to the file and flush the buffer
    if (frd->config_handle != nullptr) {
        frd->config_handle->Write(0, sizeof(FRDConfig), 1, reinterpret_cast<u8 *>(&frd->config));
    }

    // after that, reload the account saves based on the new account id and push RESULT_SUCCESS
    LoadFRDSaves(frd->config.local_account_id, frd->my_data, frd->account, frd->friendlist, frd->my_data_handle, frd->account_handle, frd->friendlist_handle, frd->file_sys_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

Module::Module(Core::System& system) : system(system) {
    InitFileSys(file_sys_handle);

    LoadFRDConfig(config, config_handle, file_sys_handle);
    LoadFRDSaves(config.local_account_id, my_data, account, friendlist, my_data_handle, account_handle, friendlist_handle, file_sys_handle);
};

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto frd = std::make_shared<Module>(system);
    std::make_shared<FRD_U>(frd)->InstallAsService(service_manager);
    std::make_shared<FRD_A>(frd)->InstallAsService(service_manager);
}


} // namespace Service::FRD
