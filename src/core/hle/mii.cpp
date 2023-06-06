#include <boost/crc.hpp>
#include "core/hle/mii.h"

namespace Mii {
	u16 ChecksummedMiiData::CalcChecksum() {
		// Calculate the checksum of the selected Mii, see https://www.3dbrew.org/wiki/Mii#Checksum
		return boost::crc<16, 0x1021, 0, 0, false, false>(this, offsetof(ChecksummedMiiData, crc16));
	}
} // namespace Mii