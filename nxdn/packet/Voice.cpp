/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015-2020 by Jonathan Naylor G4KLX
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "nxdn/NXDNDefines.h"
#include "nxdn/packet/Voice.h"
#include "nxdn/channel/FACCH1.h"
#include "nxdn/channel/SACCH.h"
#include "nxdn/acl/AccessControl.h"
#include "nxdn/Audio.h"
#include "nxdn/Sync.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::packet;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Don't process RF frames if the network isn't in a idle state and the RF destination 
// is the network destination and stop network frames from processing -- RF wants to 
// transmit on a different talkgroup
#define CHECK_TRAFFIC_COLLISION(_SRC_ID, _DST_ID)                                       \
    if (m_nxdn->m_netState != RS_NET_IDLE && _DST_ID == m_nxdn->m_netLastDstId) {       \
        LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!"); \
        resetRF();                                                                      \
        return false;                                                                   \
    }                                                                                   \
                                                                                        \
    if (m_nxdn->m_netState != RS_NET_IDLE) {                                            \
        if (m_nxdn->m_netLayer3.getSrcId() == _SRC_ID && m_nxdn->m_netLastDstId == _DST_ID) { \
            LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", srcId, dstId, \
                m_nxdn->m_netLayer3.getSrcId(), m_nxdn->m_netLastDstId);                \
            resetRF();                                                                  \
            return false;                                                               \
        }                                                                               \
        else {                                                                          \
            LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", dstId, \
                m_nxdn->m_netLastDstId);                                                \
            resetNet();                                                                 \
        }                                                                               \
    }

// Don't process network frames if the destination ID's don't match and the network TG hang 
// timer is running, and don't process network frames if the RF modem isn't in a listening state
#define CHECK_NET_TRAFFIC_COLLISION(_LAYER3, _SRC_ID, _DST_ID)                          \
    if (m_nxdn->m_rfLastDstId != 0U) {                                                  \
        if (m_nxdn->m_rfLastDstId != dstId && (m_nxdn->m_rfTGHang.isRunning() && !m_nxdn->m_rfTGHang.hasExpired())) { \
            resetNet();                                                                 \
            return false;                                                               \
        }                                                                               \
                                                                                        \
        if (m_nxdn->m_rfLastDstId == dstId && (m_nxdn->m_rfTGHang.isRunning() && !m_nxdn->m_rfTGHang.hasExpired())) { \
            m_nxdn->m_rfTGHang.start();                                                 \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    if (m_nxdn->m_rfState != RS_RF_LISTENING) {                                         \
        if (_LAYER3.getSrcId() == srcId && _LAYER3.getDstId() == dstId) { \
            LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", _LAYER3.getSrcId(), _LAYER3.getDstId(), \
                srcId, dstId);                                                          \
            resetNet();                                                                 \
            return false;                                                               \
        }                                                                               \
        else {                                                                          \
            LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic, rfDstId = %u, netDstId = %u", _LAYER3.getDstId(), \
                dstId);                                                                 \
            resetNet();                                                                 \
            return false;                                                               \
        }                                                                               \
    }

// Validate the source RID
#define VALID_SRCID(_SRC_ID, _DST_ID, _GROUP)                                           \
    if (!acl::AccessControl::validateSrcId(_SRC_ID)) {                                  \
        if (m_lastRejectId == 0U || m_lastRejectId != _SRC_ID) {                        \
            LogWarning(LOG_RF, NXDN_MESSAGE_TYPE_VCALL " denial, RID rejection, srcId = %u", _SRC_ID); \
            ::ActivityLog("NXDN", true, "RF voice rejection from %u to %s%u ", _SRC_ID, _GROUP ? "TG " : "", _DST_ID); \
            m_lastRejectId = _SRC_ID;                                                   \
        }                                                                               \
                                                                                        \
        m_nxdn->m_rfLastDstId = 0U;                                                     \
        m_nxdn->m_rfTGHang.stop();                                                      \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Validate the destination ID
#define VALID_DSTID(_SRC_ID, _DST_ID, _GROUP)                                           \
    if (!_GROUP) {                                                                      \
        if (!acl::AccessControl::validateSrcId(_DST_ID)) {                              \
            if (m_lastRejectId == 0 || m_lastRejectId != _DST_ID) {                     \
                LogWarning(LOG_RF, NXDN_MESSAGE_TYPE_VCALL " denial, RID rejection, dstId = %u", _DST_ID); \
                ::ActivityLog("NXDN", true, "RF voice rejection from %u to %s%u ", _SRC_ID, _GROUP ? "TG " : "", _DST_ID); \
                m_lastRejectId = _DST_ID;                                               \
            }                                                                           \
                                                                                        \
            m_nxdn->m_rfLastDstId = 0U;                                                 \
            m_nxdn->m_rfTGHang.stop();                                                  \
            m_nxdn->m_rfState = RS_RF_REJECTED;                                         \
            return false;                                                               \
        }                                                                               \
    }                                                                                   \
    else {                                                                              \
        if (!acl::AccessControl::validateTGId(_DST_ID)) {                               \
            if (m_lastRejectId == 0 || m_lastRejectId != _DST_ID) {                     \
                LogWarning(LOG_RF, NXDN_MESSAGE_TYPE_VCALL " denial, TGID rejection, dstId = %u", _DST_ID); \
                ::ActivityLog("NXDN", true, "RF voice rejection from %u to %s%u ", _SRC_ID, _GROUP ? "TG " : "", _DST_ID); \
                m_lastRejectId = _DST_ID;                                               \
            }                                                                           \
                                                                                        \
            m_nxdn->m_rfLastDstId = 0U;                                                 \
            m_nxdn->m_rfTGHang.stop();                                                  \
            m_nxdn->m_rfState = RS_RF_REJECTED;                                         \
            return false;                                                               \
        }                                                                               \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void Voice::resetRF()
{
    m_rfFrames = 0U;
    m_rfErrs = 0U;
    m_rfBits = 1U;
    m_rfUndecodableLC = 0U;
}

/// <summary>
/// Resets the data states for the network.
/// </summary>
void Voice::resetNet()
{
    m_netFrames = 0U;
    m_netLost = 0U;
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="usc"></param>
/// <param name="option"></param>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Voice::process(uint8_t usc, uint8_t option, uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    channel::SACCH sacch;
    bool valid = sacch.decode(data + 2U);
    if (valid) {
        uint8_t ran = sacch.getRAN();
        if (ran != m_nxdn->m_ran && ran != 0U)
            return false;
    } else if (m_nxdn->m_rfState == RS_RF_LISTENING) {
        return false;
    }

    if (usc == NXDN_LICH_USC_SACCH_NS) {
        // the SACCH on a non-superblock frame is usually an idle and not interesting apart from the RAN.
        channel::FACCH1 facch;
        bool valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
        if (!valid)
            valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
        if (!valid)
            return false;

        uint8_t buffer[10U];
        facch.getData(buffer);

        data::Layer3 layer3;
        layer3.decode(buffer, NXDN_FACCH1_LENGTH_BITS);
        uint16_t dstId = layer3.getDstId();
        uint16_t srcId = layer3.getSrcId();
        bool group = layer3.getGroup();

        uint8_t type = layer3.getMessageType();
        if (type == MESSAGE_TYPE_TX_REL) {
            if (m_nxdn->m_rfState != RS_RF_AUDIO) {
                m_nxdn->m_rfState = RS_RF_LISTENING;
                m_nxdn->m_rfMask  = 0x00U;
                m_nxdn->m_rfLayer3.reset();
                return false;
            }
        } else if (type == MESSAGE_TYPE_VCALL) {
            CHECK_TRAFFIC_COLLISION(srcId, dstId);

            // validate source RID
            VALID_SRCID(srcId, dstId, group);

            // validate destination ID
            VALID_DSTID(srcId, dstId, group);
        } else {
            return false;
        }

        m_nxdn->m_rfLayer3 = layer3;

        Sync::addNXDNSync(data + 2U);

        // generate the LICH
        channel::LICH lich;
        lich.setRFCT(NXDN_LICH_RFCT_RDCH);
        lich.setFCT(NXDN_LICH_USC_SACCH_NS);
        lich.setOption(NXDN_LICH_STEAL_FACCH);
        lich.setDirection(!m_nxdn->m_duplex ? NXDN_LICH_DIRECTION_INBOUND : NXDN_LICH_DIRECTION_OUTBOUND);
        lich.encode(data + 2U);

        // generate the SACCH
        channel::SACCH sacch;
        sacch.setRAN(m_nxdn->m_ran);
        sacch.setStructure(NXDN_SR_SINGLE);
        sacch.setData(SACCH_IDLE);
        sacch.encode(data + 2U);

        facch.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
        facch.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);

		m_nxdn->scrambler(data + 2U);

        writeNetwork(data, NXDN_FRAME_LENGTH_BYTES + 2U);

        if (m_nxdn->m_duplex) {
            data[0U] = type == MESSAGE_TYPE_TX_REL ? modem::TAG_EOT : modem::TAG_DATA;
            data[1U] = 0x00U;

            m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
        }

        if (data[0U] == modem::TAG_EOT) {
            m_rfFrames++;
            if (m_nxdn->m_rssi != 0U) {
                ::ActivityLog("NXDN", true, "RF end of transmission, %.1f seconds, BER: %.1f%%, RSSI : -%u / -%u / -%u dBm", 
                    float(m_rfFrames) / 12.5F, float(m_rfErrs * 100U) / float(m_rfBits), m_nxdn->m_minRSSI, m_nxdn->m_maxRSSI, 
                    m_nxdn->m_aveRSSI / m_nxdn->m_rssiCount);
            }
            else {
                ::ActivityLog("NXDN", true, "RF end of transmission, %.1f seconds, BER: %.1f%%", 
                    float(m_rfFrames) / 12.5F, float(m_rfErrs * 100U) / float(m_rfBits));
            }

            LogMessage(LOG_RF, NXDN_MESSAGE_TYPE_TX_REL ", total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", 
                m_rfFrames, m_rfBits, m_rfUndecodableLC, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));

            m_nxdn->writeEndRF();
        } else {
            m_rfFrames = 0U;
            m_rfErrs = 0U;
            m_rfBits = 1U;
            m_nxdn->m_rfTimeout.start();
            m_nxdn->m_rfState = RS_RF_AUDIO;
            
            m_nxdn->m_minRSSI = m_nxdn->m_rssi;
            m_nxdn->m_maxRSSI = m_nxdn->m_rssi;
            m_nxdn->m_aveRSSI = m_nxdn->m_rssi;
            m_nxdn->m_rssiCount = 1U;

            ::ActivityLog("NXDN", true, "RF voice transmission from %u to %s%u", srcId, group ? "TG " : "", dstId);
        }

		return true;
    } else {
        if (m_nxdn->m_rfState == RS_RF_LISTENING) {
            channel::FACCH1 facch;
            bool valid = false;
            switch (option) {
            case NXDN_LICH_STEAL_FACCH:
                valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
                if (!valid)
                    valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
                break;
            case NXDN_LICH_STEAL_FACCH1_1:
                valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
                break;
            case NXDN_LICH_STEAL_FACCH1_2:
                valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
                break;
            default:
                break;
            }

            bool hasInfo = false;
            if (valid) {
                uint8_t buffer[10U];
                facch.getData(buffer);

                data::Layer3 layer3;
                layer3.decode(buffer, NXDN_FACCH1_LENGTH_BITS);

                hasInfo = layer3.getMessageType() == MESSAGE_TYPE_VCALL;
                if (!hasInfo)
                    return false;

                m_nxdn->m_rfLayer3 = layer3;
            }

            if (!hasInfo) {
                uint8_t message[3U];
                sacch.getData(message);

                uint8_t structure = sacch.getStructure();
                switch (structure) {
                case NXDN_SR_1_4:
                    m_nxdn->m_rfLayer3.decode(message, 18U, 0U);
                    if(m_nxdn->m_rfLayer3.getMessageType() == MESSAGE_TYPE_VCALL)
                        m_nxdn->m_rfMask = 0x01U;
                    else
                        m_nxdn->m_rfMask = 0x00U;
                    break;
                case NXDN_SR_2_4:
                    m_nxdn->m_rfMask |= 0x02U;
                    m_nxdn->m_rfLayer3.decode(message, 18U, 18U);
                    break;
                case NXDN_SR_3_4:
                    m_nxdn->m_rfMask |= 0x04U;
                    m_nxdn->m_rfLayer3.decode(message, 18U, 36U);
                    break;
                case NXDN_SR_4_4:
                    m_nxdn->m_rfMask |= 0x08U;
                    m_nxdn->m_rfLayer3.decode(message, 18U, 54U);
                    break;
                default:
                    break;
                }

                if (m_nxdn->m_rfMask != 0x0FU)
                    return false;

                uint8_t type = m_nxdn->m_rfLayer3.getMessageType();
                if (type != MESSAGE_TYPE_VCALL)
                    return false;
            }

            uint16_t dstId = m_nxdn->m_rfLayer3.getDstId();
            uint16_t srcId = m_nxdn->m_rfLayer3.getSrcId();
            bool group = m_nxdn->m_rfLayer3.getGroup();

            CHECK_TRAFFIC_COLLISION(srcId, dstId);

            // validate source RID
            VALID_SRCID(srcId, dstId, group);

            // validate destination ID
            VALID_DSTID(srcId, dstId, group);

            m_rfFrames = 0U;
            m_rfErrs = 0U;
            m_rfBits = 1U;
            m_nxdn->m_rfTimeout.start();
            m_nxdn->m_rfState = RS_RF_AUDIO;

            m_nxdn->m_minRSSI = m_nxdn->m_rssi;
            m_nxdn->m_maxRSSI = m_nxdn->m_rssi;
            m_nxdn->m_aveRSSI = m_nxdn->m_rssi;
            m_nxdn->m_rssiCount = 1U;

            ::ActivityLog("NXDN", true, "RF late entry from %u to %s%u", srcId, group ? "TG " : "", dstId);

            // create a dummy start message
            uint8_t start[NXDN_FRAME_LENGTH_BYTES + 2U];
            ::memset(start, 0x00U, NXDN_FRAME_LENGTH_BYTES + 2U);

            // generate the sync
            Sync::addNXDNSync(start + 2U);

            // generate the LICH
            channel::LICH lich;
            lich.setRFCT(NXDN_LICH_RFCT_RDCH);
            lich.setFCT(NXDN_LICH_USC_SACCH_NS);
            lich.setOption(NXDN_LICH_STEAL_FACCH);
            lich.setDirection(!m_nxdn->m_duplex ? NXDN_LICH_DIRECTION_INBOUND : NXDN_LICH_DIRECTION_OUTBOUND);
            lich.encode(start + 2U);

            lich.setDirection(NXDN_LICH_DIRECTION_INBOUND);

            // generate the SACCH
            channel::SACCH sacch;
            sacch.setRAN(m_nxdn->m_ran);
            sacch.setStructure(NXDN_SR_SINGLE);
            sacch.setData(SACCH_IDLE);
            sacch.encode(start + 2U);

            uint8_t message[22U];
            m_nxdn->m_rfLayer3.getData(message);

            facch.setData(message);
            facch.encode(start + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
            facch.encode(start + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);

            m_nxdn->scrambler(start + 2U);

            writeNetwork(data, NXDN_FRAME_LENGTH_BYTES + 2U);

            if (m_nxdn->m_duplex) {
                start[0U] = modem::TAG_DATA;
                start[1U] = 0x00U;

                m_nxdn->addFrame(start, NXDN_FRAME_LENGTH_BYTES + 2U);
            }
        }
    }

    if (m_nxdn->m_rfState == RS_RF_AUDIO) {
        // regenerate the sync
        Sync::addNXDNSync(data + 2U);

        // regenerate the LICH
        channel::LICH lich;
        lich.setRFCT(NXDN_LICH_RFCT_RDCH);
        lich.setFCT(NXDN_LICH_USC_SACCH_SS);
        lich.setOption(option);
        lich.setDirection(!m_nxdn->m_duplex ? NXDN_LICH_DIRECTION_INBOUND : NXDN_LICH_DIRECTION_OUTBOUND);
        lich.encode(data + 2U);

        lich.setDirection(NXDN_LICH_DIRECTION_INBOUND);

        // regenerate SACCH if it's valid
        channel::SACCH sacch;
        bool validSACCH = sacch.decode(data + 2U);
        if (validSACCH) {
            sacch.setRAN(m_nxdn->m_ran);
            sacch.encode(data + 2U);
        }

        // regenerate the audio and interpret the FACCH1 data
        if (option == NXDN_LICH_STEAL_NONE) {
            edac::AMBEFEC ambe;
            
            uint32_t errors = 0U;

            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 0U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 9U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 27U);

            m_rfErrs += errors;
            m_rfBits += 188U;

            if (m_verbose) {
                LogMessage(LOG_RF, NXDN_MESSAGE_TYPE_VCALL ", audio, errs = %u/141 (%.1f%%)",
                    errors, float(errors) / 1.88F);
            }
/*
            Audio audio;
            audio.decode(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 0U, netData + 5U + 0U);
            audio.decode(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U, netData + 5U + 14U);
*/            
        } else if (option == NXDN_LICH_STEAL_FACCH1_1) {
            channel::FACCH1 facch1;
            bool valid = facch1.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
            if (valid)
                facch1.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);

            edac::AMBEFEC ambe;

            uint32_t errors = 0U;
            
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 27U);
            
            m_rfErrs += errors;
            m_rfBits += 94U;
            
            if (m_verbose) {
                LogMessage(LOG_RF, NXDN_MESSAGE_TYPE_VCALL ", audio, errs = %u/94 (%.1f%%)",
                    errors, float(errors) / 0.94F);
            }
/*
            Audio audio;
            audio.decode(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U, netData + 5U + 14U);
*/
        } else if (option == NXDN_LICH_STEAL_FACCH1_2) {
            edac::AMBEFEC ambe;

            uint32_t errors = 0U;
            
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 9U);
            
            m_rfErrs += errors;
            m_rfBits += 94U;

            if (m_verbose) {
                LogMessage(LOG_RF, NXDN_MESSAGE_TYPE_VCALL ", audio, errs = %u/94 (%.1f%%)",
                    errors, float(errors) / 0.94F);
            }
/*
            Audio audio;
            audio.decode(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 0U, netData + 5U + 0U);
*/
            channel::FACCH1 facch1;
            bool valid = facch1.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
            if (valid)
                facch1.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
        } else {
            channel::FACCH1 facch11;
            bool valid1 = facch11.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
            if (valid1)
                facch11.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);

            channel::FACCH1 facch12;
            bool valid2 = facch12.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
            if (valid2)
                facch12.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
        }

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_nxdn->scrambler(data + 2U);

        writeNetwork(data, NXDN_FRAME_LENGTH_BYTES + 2U);

        if (m_nxdn->m_duplex) {
            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
        }

        m_rfFrames++;
    }

    return true;
}

/// <summary>
/// Process a data frame from the network.
/// </summary>
/// <param name="usc"></param>
/// <param name="option"></param>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Voice::processNetwork(uint8_t usc, uint8_t option, uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    if (m_nxdn->m_netState == RS_NET_IDLE) {
        m_nxdn->m_queue.clear();
        
        resetRF();
        resetNet();
    }

    channel::SACCH sacch;
    sacch.decode(data + 2U);

    if (usc == NXDN_LICH_USC_SACCH_NS) {
        // the SACCH on a non-superblock frame is usually an idle and not interesting apart from the RAN.
        channel::FACCH1 facch;
        bool valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
        if (!valid)
            valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
        if (!valid)
            return false;

        uint8_t buffer[10U];
        facch.getData(buffer);

        data::Layer3 layer3;
        layer3.decode(buffer, NXDN_FACCH1_LENGTH_BITS);
        uint16_t dstId = layer3.getDstId();
        uint16_t srcId = layer3.getSrcId();
        bool group = layer3.getGroup();

        uint8_t type = layer3.getMessageType();
        if (type == MESSAGE_TYPE_TX_REL) {
            if (m_nxdn->m_netState != RS_NET_AUDIO) {
                m_nxdn->m_netState = RS_NET_IDLE;
                m_nxdn->m_netMask  = 0x00U;
                m_nxdn->m_netLayer3.reset();
                return false;
            }
        } else if (type == MESSAGE_TYPE_VCALL) {
            CHECK_NET_TRAFFIC_COLLISION(layer3, srcId, dstId);

            // validate source RID
            VALID_SRCID(srcId, dstId, group);

            // validate destination ID
            VALID_DSTID(srcId, dstId, group);
        } else {
            return false;
        }

        m_nxdn->m_netLayer3 = layer3;

        Sync::addNXDNSync(data + 2U);

        // generate the LICH
        channel::LICH lich;
        lich.setRFCT(NXDN_LICH_RFCT_RDCH);
        lich.setFCT(NXDN_LICH_USC_SACCH_NS);
        lich.setOption(NXDN_LICH_STEAL_FACCH);
        lich.setDirection(NXDN_LICH_DIRECTION_OUTBOUND);
        lich.encode(data + 2U);

        // generate the SACCH
        channel::SACCH sacch;
        sacch.setRAN(m_nxdn->m_ran);
        sacch.setStructure(NXDN_SR_SINGLE);
        sacch.setData(SACCH_IDLE);
        sacch.encode(data + 2U);

        facch.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
        facch.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);

		m_nxdn->scrambler(data + 2U);

        if (m_nxdn->m_duplex) {
            data[0U] = type == MESSAGE_TYPE_TX_REL ? modem::TAG_EOT : modem::TAG_DATA;
            data[1U] = 0x00U;

            m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U, true);
        }

        if (data[0U] == modem::TAG_EOT) {
            m_netFrames++;
            ::ActivityLog("NXDN", false, "network end of transmission, %.1f seconds", 
                float(m_netFrames) / 12.5F);

            LogMessage(LOG_NET, NXDN_MESSAGE_TYPE_TX_REL ", total frames: %d", 
                m_netFrames);

            m_nxdn->writeEndNet();
        } else {
            m_netFrames = 0U;
            m_nxdn->m_netTimeout.start();
            m_nxdn->m_netState = RS_NET_AUDIO;

            ::ActivityLog("NXDN", false, "network voice transmission from %u to %s%u", srcId, group ? "TG " : "", dstId);
        }

		return true;
    } else {
        if (m_nxdn->m_netState == RS_NET_IDLE) {
            channel::FACCH1 facch;
            bool valid = false;
            switch (option) {
            case NXDN_LICH_STEAL_FACCH:
                valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
                if (!valid)
                    valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
                break;
            case NXDN_LICH_STEAL_FACCH1_1:
                valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
                break;
            case NXDN_LICH_STEAL_FACCH1_2:
                valid = facch.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
                break;
            default:
                break;
            }

            bool hasInfo = false;
            if (valid) {
                uint8_t buffer[10U];
                facch.getData(buffer);

                data::Layer3 layer3;
                layer3.decode(buffer, NXDN_FACCH1_LENGTH_BITS);

                hasInfo = layer3.getMessageType() == MESSAGE_TYPE_VCALL;
                if (!hasInfo)
                    return false;

                m_nxdn->m_netLayer3 = layer3;
            }

            if (!hasInfo) {
                uint8_t message[3U];
                sacch.getData(message);

                uint8_t structure = sacch.getStructure();
                switch (structure) {
                case NXDN_SR_1_4:
                    m_nxdn->m_netLayer3.decode(message, 18U, 0U);
                    if(m_nxdn->m_netLayer3.getMessageType() == MESSAGE_TYPE_VCALL)
                        m_nxdn->m_netMask = 0x01U;
                    else
                        m_nxdn->m_netMask = 0x00U;
                    break;
                case NXDN_SR_2_4:
                    m_nxdn->m_netMask |= 0x02U;
                    m_nxdn->m_netLayer3.decode(message, 18U, 18U);
                    break;
                case NXDN_SR_3_4:
                    m_nxdn->m_netMask |= 0x04U;
                    m_nxdn->m_netLayer3.decode(message, 18U, 36U);
                    break;
                case NXDN_SR_4_4:
                    m_nxdn->m_netMask |= 0x08U;
                    m_nxdn->m_netLayer3.decode(message, 18U, 54U);
                    break;
                default:
                    break;
                }

                if (m_nxdn->m_netMask != 0x0FU)
                    return false;

                uint8_t type = m_nxdn->m_netLayer3.getMessageType();
                if (type != MESSAGE_TYPE_VCALL)
                    return false;
            }

            uint16_t dstId = m_nxdn->m_netLayer3.getDstId();
            uint16_t srcId = m_nxdn->m_netLayer3.getSrcId();
            bool group = m_nxdn->m_netLayer3.getGroup();

            CHECK_NET_TRAFFIC_COLLISION(m_nxdn->m_netLayer3, srcId, dstId);

            // validate source RID
            VALID_SRCID(srcId, dstId, group);

            // validate destination ID
            VALID_DSTID(srcId, dstId, group);

            m_rfFrames = 0U;
            m_rfErrs = 0U;
            m_rfBits = 1U;
            m_nxdn->m_netTimeout.start();
            m_nxdn->m_netState = RS_NET_AUDIO;

            ::ActivityLog("NXDN", false, "network late entry from %u to %s%u", srcId, group ? "TG " : "", dstId);

            // create a dummy start message
            uint8_t start[NXDN_FRAME_LENGTH_BYTES + 2U];
            ::memset(start, 0x00U, NXDN_FRAME_LENGTH_BYTES + 2U);

            // generate the sync
            Sync::addNXDNSync(start + 2U);

            // generate the LICH
            channel::LICH lich;
            lich.setRFCT(NXDN_LICH_RFCT_RDCH);
            lich.setFCT(NXDN_LICH_USC_SACCH_NS);
            lich.setOption(NXDN_LICH_STEAL_FACCH);
            lich.setDirection(NXDN_LICH_DIRECTION_OUTBOUND);
            lich.encode(start + 2U);

            // generate the SACCH
            channel::SACCH sacch;
            sacch.setRAN(m_nxdn->m_ran);
            sacch.setStructure(NXDN_SR_SINGLE);
            sacch.setData(SACCH_IDLE);
            sacch.encode(start + 2U);

            uint8_t message[22U];
            m_nxdn->m_rfLayer3.getData(message);

            facch.setData(message);
            facch.encode(start + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
            facch.encode(start + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);

            m_nxdn->scrambler(start + 2U);

            if (m_nxdn->m_duplex) {
                start[0U] = modem::TAG_DATA;
                start[1U] = 0x00U;

                m_nxdn->addFrame(start, NXDN_FRAME_LENGTH_BYTES + 2U, true);
            }
        }
    }

    if (m_nxdn->m_rfState == RS_RF_AUDIO) {
        // regenerate the sync
        Sync::addNXDNSync(data + 2U);

        // regenerate the LICH
        channel::LICH lich;
        lich.setRFCT(NXDN_LICH_RFCT_RDCH);
        lich.setFCT(NXDN_LICH_USC_SACCH_SS);
        lich.setOption(option);
        lich.setDirection(NXDN_LICH_DIRECTION_OUTBOUND);
        lich.encode(data + 2U);

        // regenerate SACCH if it's valid
        channel::SACCH sacch;
        bool validSACCH = sacch.decode(data + 2U);
        if (validSACCH) {
            sacch.setRAN(m_nxdn->m_ran);
            sacch.encode(data + 2U);
        }

        // regenerate the audio and interpret the FACCH1 data
        if (option == NXDN_LICH_STEAL_NONE) {
            edac::AMBEFEC ambe;
            
            uint32_t errors = 0U;

            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 0U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 9U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 27U);

            m_rfErrs += errors;
            m_rfBits += 188U;

            if (m_verbose) {
                LogMessage(LOG_NET, NXDN_MESSAGE_TYPE_VCALL ", audio, errs = %u/141 (%.1f%%)",
                    errors, float(errors) / 1.88F);
            }
        } else if (option == NXDN_LICH_STEAL_FACCH1_1) {
            channel::FACCH1 facch1;
            bool valid = facch1.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
            if (valid)
                facch1.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);

            edac::AMBEFEC ambe;

            uint32_t errors = 0U;
            
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 27U);
            
            m_rfErrs += errors;
            m_rfBits += 94U;
            
            if (m_verbose) {
                LogMessage(LOG_RF, NXDN_MESSAGE_TYPE_VCALL ", audio, errs = %u/94 (%.1f%%)",
                    errors, float(errors) / 0.94F);
            }
        } else if (option == NXDN_LICH_STEAL_FACCH1_2) {
            edac::AMBEFEC ambe;

            uint32_t errors = 0U;
            
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES);
            errors += ambe.regenerateNXDN(data + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 9U);
            
            m_rfErrs += errors;
            m_rfBits += 94U;

            if (m_verbose) {
                LogMessage(LOG_RF, NXDN_MESSAGE_TYPE_VCALL ", audio, errs = %u/94 (%.1f%%)",
                    errors, float(errors) / 0.94F);
            }
            channel::FACCH1 facch1;
            bool valid = facch1.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
            if (valid)
                facch1.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
        } else {
            channel::FACCH1 facch11;
            bool valid1 = facch11.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
            if (valid1)
                facch11.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);

            channel::FACCH1 facch12;
            bool valid2 = facch12.decode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
            if (valid2)
                facch12.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);
        }

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_nxdn->scrambler(data + 2U);

        if (m_nxdn->m_duplex) {
            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
        }

        m_netFrames++;
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Voice class.
/// </summary>
/// <param name="nxdn">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="debug">Flag indicating whether NXDN debug is enabled.</param>
/// <param name="verbose">Flag indicating whether NXDN verbose logging is enabled.</param>
Voice::Voice(Control* nxdn, network::BaseNetwork* network, bool debug, bool verbose) :
    m_nxdn(nxdn),
    m_network(network),
    m_rfFrames(0U),
    m_rfBits(0U),
    m_rfErrs(0U),
    m_rfUndecodableLC(0U),
    m_netFrames(0U),
    m_netLost(0U),
    m_lastRejectId(0U),
    m_silenceThreshold(DEFAULT_SILENCE_THRESHOLD),
    m_verbose(verbose),
    m_debug(debug)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Voice class.
/// </summary>
Voice::~Voice()
{
    /* stub */
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="len"></param>
void Voice::writeNetwork(const uint8_t *data, uint32_t len)
{
    assert(data != NULL);

    if (m_network == NULL)
        return;

    if (m_nxdn->m_rfTimeout.isRunning() && m_nxdn->m_rfTimeout.hasExpired())
        return;

    m_network->writeNXDN(data, len);
}
