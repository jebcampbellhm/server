#pragma once
// The Web-888 RX_READ ABI has no sample timestamp. PPS_READ is a period
// measurement used for frequency correction, not a sample counter anchor.
// Age 255 is the existing SND convention for unavailable GPS timing.
// Preserve the administrator's timestamp-redaction convention (all zero).
template<class Header>
inline void set_unavailable_iq_timing(Header& header, bool disclose) {
    header.last_gps_solution = disclose ? 255 : 0;
    header.dummy = 0;
    header.gpssec = 0;
    header.gpsnsec = 0;
}
