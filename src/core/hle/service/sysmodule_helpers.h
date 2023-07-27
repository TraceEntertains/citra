#pragma once

#include "common/common_types.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/thread.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/file_backend.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"

namespace SysmoduleHelpers {

    enum class NascEnvironment : u8 {
        Prod = 0,
        Test = 1,
        Dev = 2
    };

    enum class LocalAccountID : u8 {
        Prod = 1,
        Test = 2,
        Dev = 3
    };

    enum class LongLocalAccountID : u32 {
        Prod = 1,
        Test = 2,
        Dev = 3
    };

    enum class TrivialCharacterSet : u8 {
        JapanUsaEuropeAustralia = 0,
        Korea = 1,
        China = 2,
        Taiwan = 3
    };

    class YearMonthDate {
    public:
        u16_le year;
        u16_le month;
        u16_le date;

        YearMonthDate(u16_le year, u16_le month, u16_le date)
            : year(year), month(month), date(date) {}

        YearMonthDate() : YearMonthDate(2000, 1, 1) {}

        static YearMonthDate fromDaysSinceEpoch(u32_le days_since_epoch) {
            if (days_since_epoch < 60) {
                if (days_since_epoch == 0) {
                    return YearMonthDate();
                }

                // We can only be in January or February here
                u16_le zero_indexed_month = static_cast<u16_le>(days_since_epoch / 32);
                return YearMonthDate(2000, zero_indexed_month + 1, (days_since_epoch % 32) + zero_indexed_month);
            }

            // Remove January and February and start the year with March - we'll
            // add January and February back later.
            // This means we don't have to worry about this year being a leap year,
            // nor do we have to worry about a month with 28 days.
            u32_le adjusted_days = days_since_epoch - 60;

            // 146097 is the number of days in 400 years, including leap days
            u32_le remaining_days_of_400_years = adjusted_days % 146097;
            u32_le number_of_400_years = adjusted_days / 146097;

            // 36524 is the number of days in 100 years, including leap days
            u32_le remaining_days_of_100_years = remaining_days_of_400_years % 36524;
            u32_le number_of_100_years = remaining_days_of_400_years / 36524;

            // 1461 is the number of days in 4 years, including leap day
            u32_le remaining_days_of_4_years = remaining_days_of_100_years % 1461;
            u32_le number_of_4_years = remaining_days_of_100_years / 1461;

            // 365 is the number of days in a non-leap year
            u32_le remaining_days_of_1_year = remaining_days_of_4_years % 365;
            u32_le number_of_1_years = remaining_days_of_4_years / 365;

            u32_le temp_year = number_of_400_years * 400 + number_of_100_years * 100 + number_of_4_years * 4 + number_of_1_years;

            u32_le year = temp_year + 2000;
            u32_le month = ((remaining_days_of_1_year * 5) + 2) / 153;
            u32_le day = (remaining_days_of_1_year - ((month * 153) + 2) / 5) + 1;

            if ((number_of_1_years == 4) || (number_of_100_years == 4)) {
                month = 2;
                day = 29;
            } else if (month < 10) {
                month += 3;
            } else {
                year += 1;
                month = std::max(month - 9, 1U);
            }

            return YearMonthDate(static_cast<u16_le>(year), static_cast<u16_le>(month), static_cast<u16_le>(day));
        }
    };

    class FormattedTimestamp {
    public:
        FormattedTimestamp() {}

        FormattedTimestamp(u16_le year, u16_le month, u16_le date, u16_le hours, u16_le minutes, u16_le seconds)
            : raw(((static_cast<u64_le>(year) & 0xFFFFu) << 26)
                + ((static_cast<u64_le>(month) & 0xFu) << 22)
                + ((static_cast<u64_le>(date) & 0x1Fu) << 17)
                + ((static_cast<u64_le>(hours) & 0x1Fu) << 12)
                + ((static_cast<u64_le>(minutes) & 0x3Fu) << 6)
                + (static_cast<u64_le>(seconds) & 0x3Fu)) {}

        s32_le get_days_since_system_epoch() const {
            s32_le year = static_cast<s32_le>(get_year());

            // The real sysmodule accounts for dates before 2000, even though
            // January 1, 2000, is the 3ds epoch.
            // We're not going to since that can cause issues if someone
            // is intentionally using bad data.
            if (year < 2000) {
                return 0;
            }

            s32_le month = static_cast<s32_le>(get_month());

            // Remove January and February and start the year with March - we'll
            // add January and February back later.
            // This means we don't have to worry about this year being a leap year,
            // nor do we have to worry about a month with 28 days.
            // If the month is before March, pretend it's last year, otherwise
            // continue as if it's the current year.
            s32_le adjusted_month, adjusted_year;
            if (month < 3) {
                adjusted_month = month + 9;
                adjusted_year = year - 2001;
            } else {
                adjusted_month = month - 3;
                adjusted_year = year - 2000;
            }

            // 1461 is the number of days in 4 years, including a leap day
            // The magic of accounting for the leap day happens when we round down during division
            s32_le days_from_last_100_years = ((adjusted_year % 100) * 1461) / 4;
            // 146097 is the number of days in 400 years, including leap days
            // The magic of accounting for leap days happens when we round down during division
            s32_le days_from_over_100_years_ago = ((adjusted_year / 100) * 146097) / 4;

            // Leap days were accounted for above, so we just need the days for this year since March (month 0)
            // Remember, January or February counted as the previous year, which we already accounted for
            // January + February have 59 days when it's not a leap year
            // 365 days - 59 days = 306 remaining days
            // 12 months - 2 months = 10 remaining months
            // 4 and 5 both appear to be valid correction numbers to handle months with 30 days
            // The original code used ((adjusted_month * 306) + 4) / 10, but to make it "more accurate to the original", I changed it
            s32_le days_from_current_year_months = ((adjusted_month * 153) + 2) / 5;

            return days_from_current_year_months
                + days_from_last_100_years
                + days_from_over_100_years_ago
                + static_cast<s32_le>(get_day())
                // The additional 59 days come from January and February
                + 59;
        }

        u16_le get_year() const {
            return static_cast<u16_le>((raw >> 26) & 0xFFFFu);
        }

        u16_le get_month() const {
            return static_cast<u16_le>((raw >> 22) & 0xFu);
        }

        u16_le get_day() const {
            return static_cast<u16_le>((raw >> 17) & 0x1Fu);
        }

        u16_le get_hours() const {
            return static_cast<u16_le>((raw >> 12) & 0x1Fu);
        }

        u16_le get_minutes() const {
            return static_cast<u16_le>((raw >> 6) & 0x3Fu);
        }

        u16_le get_seconds() const {
            return static_cast<u16_le>(raw & 0x3Fu);
        }

        u64_le raw;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& raw;
        }
        friend class boost::serialization::access;
    };

    class SystemTimestamp {
    public:
        SystemTimestamp() {};

        SystemTimestamp(const FormattedTimestamp &timestamp) {
            raw =
            (timestamp.get_days_since_system_epoch() * 86400000ULL +
            timestamp.get_hours() * 3600000ULL +
            timestamp.get_minutes() * 60000ULL +
            timestamp.get_seconds() * 1000) - 946684800000ULL;
        }

        SystemTimestamp(u64_le millis) : raw(millis) {}

        static SystemTimestamp from_unix_timestamp(uint64_t unix_millis) {
            return SystemTimestamp(unix_millis - 946684800000ULL);
        }

        u64_le get_unix_timestamp() const {
            // There's a 30 year offset between the 3ds epoch and the unix epoch
            return raw + 946684800000ULL;
        }

        u64_le get_epoch() const {
            return raw;
        }

        YearMonthDate get_year_month_date() const {
            return YearMonthDate::fromDaysSinceEpoch(get_days_since_system_epoch());
        }

        u16_le get_days_since_system_epoch() const {
            return static_cast<u32_le>(raw / 86400000ULL);
        }

        u16_le get_hours() const {
            return static_cast<u16_le>((raw / 3600000ULL) % 24ULL);
        }

        u16_le get_minutes() const {
            return static_cast<u16_le>((raw / 60000ULL) % 60ULL);
        }

        u16_le get_seconds() const {
            return static_cast<u16_le>((raw / 1000ULL) % 60ULL);
        }

        u64_le raw;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& raw;
        }
        friend class boost::serialization::access;
    };

    // Load file helper
    template <typename T>
    bool LoadSave(u32 classSize, T &loadStruct, const char *path, std::unique_ptr<FileSys::FileBackend> &fileHandle, std::unique_ptr<FileSys::ArchiveBackend> &fileSysHandle) {
        LOG_INFO(Service_FRD, path);

        FileSys::Mode mode = {};
        mode.write_flag.Assign(1);
        mode.read_flag.Assign(1);
        mode.create_flag.Assign(1);

        auto fileResult = fileSysHandle->OpenFile(FileSys::Path(path), mode);
        if (fileResult.Failed()) {
            return false;
        }

        fileHandle = std::move(fileResult).Unwrap();
        if (fileHandle->GetSize() != classSize) {
            fileHandle->Close();
            return false;
        }

        fileHandle->Read(0, classSize, reinterpret_cast<u8 *>(&loadStruct));
        return true;
    }

    // Load friendlist file helper
    template <typename T>
    bool LoadFlexSave(u32 classSize, u32 checkSize, u32 arraySize, T &loadStruct, u32 *loadInt, const char *path, std::unique_ptr<FileSys::FileBackend> &fileHandle, std::unique_ptr<FileSys::ArchiveBackend> &fileSysHandle) {
        LOG_INFO(Service_FRD, path);

        FileSys::Mode mode = {};
        mode.write_flag.Assign(1);
        mode.read_flag.Assign(1);
        mode.create_flag.Assign(1);

        auto fileResult = fileSysHandle->OpenFile(FileSys::Path(path), mode);
        if (fileResult.Failed()) {
            return false;
        }

        fileHandle = std::move(fileResult).Unwrap();
        if ((fileHandle->GetSize() % (classSize - checkSize)) != 0) {
            fileHandle->Close();
            return false;
        }

        fileHandle->Read(0, classSize, reinterpret_cast<u8 *>(&loadStruct));
        if (fileHandle->GetSize() - checkSize <= 0) {
            *loadInt = 0;
        }

        *loadInt = (fileHandle->GetSize() - (classSize - checkSize)) / arraySize;
        return true;
    }
};