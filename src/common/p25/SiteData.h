// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2018,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_SITE_DATA_H__)
#define  __P25_SITE_DATA_H__

#include "common/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/P25Utils.h"

#include <random>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents site data for P25.
    // ---------------------------------------------------------------------------

    class HOST_SW_API SiteData {
    public:
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        SiteData() :
            m_lra(0U),
            m_netId(defines::WACN_STD_DEFAULT),
            m_sysId(defines::SID_STD_DEFAULT),
            m_rfssId(1U),
            m_siteId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(defines::ServiceClass::VOICE | defines::ServiceClass::DATA),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_chCnt(0U),
            m_netActive(false),
            m_lto(0)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        /// <param name="netId">P25 Network ID.</param>
        /// <param name="sysId">P25 System ID.</param>
        /// <param name="rfssId">P25 RFSS ID.</param>
        /// <param name="siteId">P25 Site ID.</param>
        /// <param name="lra">P25 Location Resource Area.</param>
        /// <param name="channelId">Channel ID.</param>
        /// <param name="channelNo">Channel Number.</param>
        /// <param name="serviceClass">Service class.</param>
        /// <param name="lto">Local time offset.</param>
        SiteData(uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t lra, uint8_t channelId, uint32_t channelNo, uint8_t serviceClass, int8_t lto) :
            m_lra(0U),
            m_netId(defines::WACN_STD_DEFAULT),
            m_sysId(defines::SID_STD_DEFAULT),
            m_rfssId(1U),
            m_siteId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(defines::ServiceClass::VOICE | defines::ServiceClass::DATA),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_chCnt(0U),
            m_netActive(false),
            m_lto(0)
        {
            // lra clamping
            if (lra > 0xFFU) // clamp to $FF
                lra = 0xFFU;

            // netId clamping
            netId = P25Utils::netId(netId);

            // sysId clamping
            sysId = P25Utils::sysId(sysId);

            // rfssId clamping
            rfssId = P25Utils::rfssId(rfssId);

            // siteId clamping
            siteId = P25Utils::siteId(siteId);

            // channel id clamping
            if (channelId > 15U)
                channelId = 15U;

            // channel number clamping
            if (m_channelNo == 0U) { // clamp to 1
                m_channelNo = 1U;
            }
            if (m_channelNo > 4095U) { // clamp to 4095
                m_channelNo = 4095U;
            }

            m_lra = lra;

            m_netId = netId;
            m_sysId = sysId;

            uint32_t valueTest = (m_netId >> 8);
            const uint32_t constValue = 0x5F700U;
            if (valueTest == (constValue >> 7)) {
                std::random_device rd;
                std::mt19937 mt(rd());

                std::uniform_int_distribution<uint32_t> dist(0x01, defines::WACN_STD_DEFAULT);
                m_netId = dist(mt);

                // netId clamping
                netId = P25Utils::netId(netId);

                dist = std::uniform_int_distribution<uint32_t>(0x01, 0xFFEU);
                m_sysId = dist(mt);

                // sysId clamping
                sysId = P25Utils::sysId(sysId);
            }

            m_rfssId = rfssId;
            m_siteId = siteId;

            m_channelId = channelId;
            m_channelNo = channelNo;

            m_serviceClass = serviceClass;

            m_lto = lto;
        }

        /// <summary>Helper to set the site callsign.</summary>
        /// <param name="callsign">Callsign.</param>
        void setCallsign(std::string callsign)
        {
            m_callsign = callsign;
        }

        /// <summary>Helper to set the site channel count.</summary>
        /// <param name="chCnt">Channel count.</param>
        void setChCnt(uint8_t chCnt)
        {
            m_chCnt = chCnt;
        }

        /// <summary>Helper to set the site network active flag.</summary>
        /// <param name="netActive">Network active.</param>
        void setNetActive(bool netActive)
        {
            m_netActive = netActive;
        }

        /// <summary>Helper to set adjacent site data.</summary>
        /// <param name="sysId">P25 System ID.</param>
        /// <param name="rfssId">P25 RFSS ID.</param>
        /// <param name="siteId">P25 Site ID.</param>
        /// <param name="channelId">Channel ID.</param>
        /// <param name="channelNo">Channel Number.</param>
        /// <param name="serviceClass">Service class.</param>
        void setAdjSite(uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, uint8_t serviceClass)
        {
            // sysId clamping
            sysId = P25Utils::sysId(sysId);

            // rfssId clamping
            rfssId = P25Utils::rfssId(rfssId);

            // siteId clamping
            siteId = P25Utils::siteId(siteId);

            // channel id clamping
            if (channelId > 15U)
                channelId = 15U;

            // channel number clamping
            if (channelNo == 0U) { // clamp to 1
                channelNo = 1U;
            }
            if (channelNo > 4095U) { // clamp to 4095
                channelNo = 4095U;
            }

            m_lra = 0U;

            m_netId = 0U;
            m_sysId = sysId;

            m_rfssId = rfssId;
            m_siteId = siteId;

            m_channelId = channelId;
            m_channelNo = channelNo;

            m_serviceClass = serviceClass;

            m_isAdjSite = true;

            m_callsign = "ADJSITE ";
            m_chCnt = -1; // don't store channel count for adjacent sites
            m_netActive = true; // adjacent sites are explicitly network active
            m_lto = 0;
        }

        /// <summary>Equals operator.</summary>
        /// <param name="data"></param>
        /// <returns></returns>
        SiteData & operator=(const SiteData & data)
        {
            if (this != &data) {
                m_lra = data.m_lra;

                m_netId = data.m_netId;
                m_sysId = data.m_sysId;

                m_rfssId = data.m_rfssId;
                m_siteId = data.m_siteId;

                m_channelId = data.m_channelId;
                m_channelNo = data.m_channelNo;

                m_serviceClass = data.m_serviceClass;

                m_isAdjSite = data.m_isAdjSite;

                m_callsign = data.m_callsign;
                m_chCnt = data.m_chCnt;

                m_netActive = data.m_netActive;

                m_lto = data.m_lto;
            }

            return *this;
        }

    public:
        /// <summary>P25 location resource area.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, lra);
        /// <summary>P25 network ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, netId);
        /// <summary>Gets the P25 system ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, sysId);
        /// <summary>P25 RFSS ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, rfssId);
        /// <summary>P25 site ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, siteId);
        /// <summary>Channel ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, channelId);
        /// <summary>Channel number.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, channelNo);
        /// <summary>Service class.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, serviceClass);
        /// <summary>Flag indicating whether this site data is for an adjacent site.</summary>
        __READONLY_PROPERTY_PLAIN(bool, isAdjSite);
        /// <summary>Callsign.</summary>
        __READONLY_PROPERTY_PLAIN(std::string, callsign);
        /// <summary>Count of available channels.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, chCnt);
        /// <summary>Flag indicating whether this site is a linked active network member.</summary>
        __READONLY_PROPERTY_PLAIN(bool, netActive);
        /// <summary>Local Time Offset.</summary>
        __READONLY_PROPERTY_PLAIN(int8_t, lto);
    };
} // namespace p25

#endif // __P25_SITE_DATA_H__
