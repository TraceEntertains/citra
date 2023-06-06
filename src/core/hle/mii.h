#pragma once

#include <boost/serialization/base_object.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"

namespace Mii {

#pragma pack(push, 1)
// Reference: https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/mii.h
class MiiData {
public:
    u8 magic{}; ///< Always 3?

    /// Mii options
    union {
        u8 raw;

        BitField<0, 1, u8> allow_copying;   ///< True if copying is allowed
        BitField<1, 1, u8> is_private_name; ///< Private name?
        BitField<2, 2, u8> region_lock;     ///< Region lock (0=no lock, 1=JPN, 2=USA, 3=EUR)
        BitField<4, 2, u8> char_set;        ///< Character set (0=JPN+USA+EUR, 1=CHN, 2=KOR, 3=TWN)
    } mii_options{};

    /// Mii position in Mii selector or Mii maker
    union {
        u8 raw;

        BitField<0, 4, u8> page_index; ///< Page index of Mii
        BitField<4, 4, u8> slot_index; ///< Slot offset of Mii on its Page
    } mii_pos{};

    /// Console Identity
    union {
        u8 raw;

        BitField<0, 4, u8> unknown0; ///< Mabye padding (always seems to be 0)?
        BitField<4, 3, u8>
            origin_console; ///< Console that the Mii was created on (1=WII, 2=DSI, 3=3DS)
    } console_identity{};

    u64_be system_id{}; ///< Identifies the system that the Mii was created on (Determines pants)
    u32_be mii_id{};    ///< ID of Mii
    std::array<u8, 6> mac{}; ///< Creator's system's full MAC address
    u16 pad{};               ///< Padding

    /// Mii details
    union {
        u16_be raw;

        BitField<0, 1, u16> sex;          ///< Sex of Mii (False=Male, True=Female)
        BitField<1, 4, u16> bday_month;   ///< Month of Mii's birthday
        BitField<5, 5, u16> bday_day;     ///< Day of Mii's birthday
        BitField<10, 4, u16> shirt_color; ///< Color of Mii's shirt
        BitField<14, 1, u16> favorite;    ///< Whether the Mii is one of your 10 favorite Mii's
    } mii_details{};

    std::array<u16_le, 10> mii_name{}; ///< Name of Mii (Encoded using UTF16)
    u8 height{};                       ///< How tall the Mii is
    u8 width{};                        ///< How wide the Mii is

    /// Face style
    union {
        u8 raw;

        BitField<0, 1, u8> disable_sharing; ///< Whether or not Sharing of the Mii is allowed
        BitField<1, 4, u8> shape;           ///< Face shape
        BitField<5, 3, u8> skin_color;      ///< Color of skin
    } face_style{};

    /// Face details
    union {
        u8 raw;

        BitField<0, 4, u8> wrinkles;
        BitField<4, 4, u8> makeup;
    } face_details{};

    u8 hair_style{};

    /// Hair details
    union {
        u8 raw;

        BitField<0, 3, u8> color;
        BitField<3, 1, u8> flip;
    } hair_details{};

    /// Eye details
    union {
        u32_be raw;

        BitField<0, 6, u32> style;
        BitField<6, 3, u32> color;
        BitField<9, 4, u32> scale;
        BitField<13, 3, u32> yscale;
        BitField<16, 5, u32> rotation;
        BitField<21, 4, u32> xspacing;
        BitField<25, 5, u32> yposition;
    } eye_details{};

    /// Eyebrow details
    union {
        u32_be raw;

        BitField<0, 5, u32> style;
        BitField<5, 3, u32> color;
        BitField<8, 4, u32> scale;
        BitField<12, 3, u32> yscale;
        BitField<15, 1, u32> pad;
        BitField<16, 5, u32> rotation;
        BitField<21, 4, u32> xspacing;
        BitField<25, 5, u32> yposition;
    } eyebrow_details{};

    /// Nose details
    union {
        u16_be raw;

        BitField<0, 5, u16> style;
        BitField<5, 4, u16> scale;
        BitField<9, 5, u16> yposition;
    } nose_details{};

    /// Mouth details
    union {
        u16_be raw;

        BitField<0, 6, u16> style;
        BitField<6, 3, u16> color;
        BitField<9, 4, u16> scale;
        BitField<13, 3, u16> yscale;
    } mouth_details{};

    /// Mustache details
    union {
        u16_be raw;

        BitField<0, 5, u16> mouth_yposition;
        BitField<5, 3, u16> mustach_style;
        BitField<8, 2, u16> pad;
    } mustache_details{};

    /// Beard details
    union {
        u16_be raw;

        BitField<0, 3, u16> style;
        BitField<3, 3, u16> color;
        BitField<6, 4, u16> scale;
        BitField<10, 5, u16> ypos;
    } beard_details{};

    /// Glasses details
    union {
        u16_be raw;

        BitField<0, 4, u16> style;
        BitField<4, 3, u16> color;
        BitField<7, 4, u16> scale;
        BitField<11, 5, u16> ypos;
    } glasses_details{};

    /// Mole details
    union {
        u16_be raw;

        BitField<0, 1, u16> enable;
        BitField<1, 5, u16> scale;
        BitField<6, 5, u16> xpos;
        BitField<11, 5, u16> ypos;
    } mole_details{};

    std::array<u16_le, 10> author_name{}; ///< Name of Mii's author (Encoded using UTF16)
private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& magic;
        ar& mii_options.raw;
        ar& mii_pos.raw;
        ar& console_identity.raw;
        u64 system_id_ = system_id;
        ar& system_id_;
        system_id = system_id_;
        u32 mii_id_ = mii_id;
        ar& mii_id_;
        mii_id = mii_id_;
        ar& mac;
        ar& pad;
        u16 mii_details_ = mii_details.raw;
        ar& mii_details_;
        mii_details.raw = mii_details_;
        ar& mii_name;
        ar& height;
        ar& width;
        ar& face_style.raw;
        ar& face_details.raw;
        ar& hair_style;
        ar& hair_details.raw;
        u32 eye_details_ = eye_details.raw;
        ar& eye_details_;
        eye_details.raw = eye_details_;
        u32 eyebrow_details_ = eyebrow_details.raw;
        ar& eyebrow_details_;
        eyebrow_details.raw = eyebrow_details_;
        u16 nose_details_ = nose_details.raw;
        ar& nose_details_;
        nose_details.raw = nose_details_;
        u16 mouth_details_ = mouth_details.raw;
        ar& mouth_details_;
        mouth_details.raw = mouth_details_;
        u16 mustache_details_ = mustache_details.raw;
        ar& mustache_details_;
        mustache_details.raw = mustache_details_;
        u16 beard_details_ = beard_details.raw;
        ar& beard_details_;
        beard_details.raw = beard_details_;
        u16 glasses_details_ = glasses_details.raw;
        ar& glasses_details_;
        glasses_details.raw = glasses_details_;
        u16 mole_details_ = mole_details.raw;
        ar& mole_details_;
        mole_details.raw = mole_details_;
        ar& author_name;
    }
    friend class boost::serialization::access;
};

static_assert(sizeof(MiiData) == 0x5C, "MiiData structure has incorrect size");

class ChecksummedMiiData {
public:
    ChecksummedMiiData() {
        FixChecksum();
    }
    ChecksummedMiiData(const ChecksummedMiiData& data) = default;
    ChecksummedMiiData(ChecksummedMiiData&& data) = default;
    ChecksummedMiiData& operator=(const ChecksummedMiiData&) = default;
    ChecksummedMiiData& operator=(ChecksummedMiiData&&) = default;

    ChecksummedMiiData(const MiiData& data) : mii_data(data) {
        FixChecksum();
    }

    ChecksummedMiiData(MiiData&& data) : mii_data(data) {
        FixChecksum();
    }

    ChecksummedMiiData& operator=(const MiiData& data) {
        mii_data = data;
        FixChecksum();
        return *this;
    }

    ChecksummedMiiData& operator=(MiiData&& data) {
        mii_data = std::move(data);
        FixChecksum();
        return *this;
    }

    MiiData& GetMiiData() {
        return mii_data;
    }

    bool IsChecksumValid() {
        return crc16 == CalcChecksum();
    }

    u16 CalcChecksum();

private:
    MiiData mii_data{};
    u16_be unknown{0};
    u16_be crc16;

    void FixChecksum() {
        crc16 = CalcChecksum();
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& mii_data;
        u16 unknown_ = unknown;
        ar& unknown_;
        unknown = unknown_;
        u16 crc16_ = crc16;
        ar& crc16_;
        crc16 = crc16_;
    }
    friend class boost::serialization::access;
};
#pragma pack(pop)
static_assert(sizeof(ChecksummedMiiData) == 0x60,
              "ChecksummedMiiData structure has incorrect size");
} // namespace Mii