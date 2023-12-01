/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the C++ AES project. (https://github.com/SergeyBel/AES)
// Licensed under the MIT License (https://opensource.org/license/MIT)
//
/*
*   Copyright (C) 2019 SergeyBel
*   Copyright (C) 2023 Bryan Biedenkapp N2PLL
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files (the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions:
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*/
#include "Defines.h"
#include "AESCrypto.h"

using namespace crypto;

#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t SBOX[16][16] = {
    { 0x63U, 0x7CU, 0x77U, 0x7BU, 0xF2U, 0x6BU, 0x6FU, 0xC5U, 0x30U, 0x01U, 0x67U, 0x2BU, 0xFEU, 0xD7U, 0xABU, 0x76U },
    { 0xCAU, 0x82U, 0xC9U, 0x7DU, 0xFAU, 0x59U, 0x47U, 0xF0U, 0xADU, 0xD4U, 0xA2U, 0xAFU, 0x9CU, 0xA4U, 0x72U, 0xC0U },
    { 0xB7U, 0xFDU, 0x93U, 0x26U, 0x36U, 0x3FU, 0xF7U, 0xCCU, 0x34U, 0xA5U, 0xE5U, 0xF1U, 0x71U, 0xD8U, 0x31U, 0x15U },
    { 0x04U, 0xC7U, 0x23U, 0xC3U, 0x18U, 0x96U, 0x05U, 0x9AU, 0x07U, 0x12U, 0x80U, 0xE2U, 0xEBU, 0x27U, 0xB2U, 0x75U },
    { 0x09U, 0x83U, 0x2CU, 0x1AU, 0x1BU, 0x6EU, 0x5AU, 0xA0U, 0x52U, 0x3BU, 0xD6U, 0xB3U, 0x29U, 0xE3U, 0x2FU, 0x84U },
    { 0x53U, 0xD1U, 0x00U, 0xEDU, 0x20U, 0xFCU, 0xB1U, 0x5BU, 0x6AU, 0xCBU, 0xBEU, 0x39U, 0x4AU, 0x4CU, 0x58U, 0xCFU },
    { 0xD0U, 0xEFU, 0xAAU, 0xFBU, 0x43U, 0x4DU, 0x33U, 0x85U, 0x45U, 0xF9U, 0x02U, 0x7FU, 0x50U, 0x3CU, 0x9FU, 0xA8U },
    { 0x51U, 0xA3U, 0x40U, 0x8FU, 0x92U, 0x9DU, 0x38U, 0xF5U, 0xBCU, 0xB6U, 0xDAU, 0x21U, 0x10U, 0xFFU, 0xF3U, 0xD2U },
    { 0xCDU, 0x0CU, 0x13U, 0xECU, 0x5FU, 0x97U, 0x44U, 0x17U, 0xC4U, 0xA7U, 0x7EU, 0x3DU, 0x64U, 0x5DU, 0x19U, 0x73U },
    { 0x60U, 0x81U, 0x4FU, 0xDCU, 0x22U, 0x2AU, 0x90U, 0x88U, 0x46U, 0xEEU, 0xB8U, 0x14U, 0xDEU, 0x5EU, 0x0BU, 0xDBU },
    { 0xE0U, 0x32U, 0x3AU, 0x0AU, 0x49U, 0x06U, 0x24U, 0x5CU, 0xC2U, 0xD3U, 0xACU, 0x62U, 0x91U, 0x95U, 0xE4U, 0x79U },
    { 0xE7U, 0xC8U, 0x37U, 0x6DU, 0x8DU, 0xD5U, 0x4EU, 0xA9U, 0x6CU, 0x56U, 0xF4U, 0xEAU, 0x65U, 0x7AU, 0xAEU, 0x08U },
    { 0xBAU, 0x78U, 0x25U, 0x2EU, 0x1CU, 0xA6U, 0xB4U, 0xC6U, 0xE8U, 0xDDU, 0x74U, 0x1FU, 0x4BU, 0xBDU, 0x8BU, 0x8AU },
    { 0x70U, 0x3EU, 0xB5U, 0x66U, 0x48U, 0x03U, 0xF6U, 0x0EU, 0x61U, 0x35U, 0x57U, 0xB9U, 0x86U, 0xC1U, 0x1DU, 0x9EU },
    { 0xE1U, 0xF8U, 0x98U, 0x11U, 0x69U, 0xD9U, 0x8EU, 0x94U, 0x9BU, 0x1EU, 0x87U, 0xE9U, 0xCEU, 0x55U, 0x28U, 0xDFU },
    { 0x8CU, 0xA1U, 0x89U, 0x0DU, 0xBFU, 0xE6U, 0x42U, 0x68U, 0x41U, 0x99U, 0x2DU, 0x0FU, 0xB0U, 0x54U, 0xBBU, 0x16U }
};

const uint8_t INV_SBOX[16][16] = {
    { 0x52U, 0x09U, 0x6AU, 0xD5U, 0x30U, 0x36U, 0xA5U, 0x38U, 0xBFU, 0x40U, 0xA3U, 0x9EU, 0x81U, 0xF3U, 0xD7U, 0xFBU },
    { 0x7CU, 0xE3U, 0x39U, 0x82U, 0x9BU, 0x2FU, 0xFFU, 0x87U, 0x34U, 0x8EU, 0x43U, 0x44U, 0xC4U, 0xDEU, 0xE9U, 0xCBU },
    { 0x54U, 0x7BU, 0x94U, 0x32U, 0xA6U, 0xC2U, 0x23U, 0x3DU, 0xEEU, 0x4CU, 0x95U, 0x0BU, 0x42U, 0xFAU, 0xC3U, 0x4EU },
    { 0x08U, 0x2EU, 0xA1U, 0x66U, 0x28U, 0xD9U, 0x24U, 0xB2U, 0x76U, 0x5BU, 0xA2U, 0x49U, 0x6DU, 0x8BU, 0xD1U, 0x25U },
    { 0x72U, 0xF8U, 0xF6U, 0x64U, 0x86U, 0x68U, 0x98U, 0x16U, 0xD4U, 0xA4U, 0x5CU, 0xCCU, 0x5DU, 0x65U, 0xB6U, 0x92U },
    { 0x6CU, 0x70U, 0x48U, 0x50U, 0xFDU, 0xEDU, 0xB9U, 0xDAU, 0x5EU, 0x15U, 0x46U, 0x57U, 0xA7U, 0x8DU, 0x9DU, 0x84U },
    { 0x90U, 0xD8U, 0xABU, 0x00U, 0x8CU, 0xBCU, 0xD3U, 0x0AU, 0xF7U, 0xE4U, 0x58U, 0x05U, 0xB8U, 0xB3U, 0x45U, 0x06U },
    { 0xD0U, 0x2CU, 0x1EU, 0x8FU, 0xCAU, 0x3FU, 0x0FU, 0x02U, 0xC1U, 0xAFU, 0xBDU, 0x03U, 0x01U, 0x13U, 0x8AU, 0x6BU },
    { 0x3AU, 0x91U, 0x11U, 0x41U, 0x4FU, 0x67U, 0xDCU, 0xEAU, 0x97U, 0xF2U, 0xCFU, 0xCEU, 0xF0U, 0xB4U, 0xE6U, 0x73U },
    { 0x96U, 0xACU, 0x74U, 0x22U, 0xE7U, 0xADU, 0x35U, 0x85U, 0xE2U, 0xF9U, 0x37U, 0xE8U, 0x1CU, 0x75U, 0xDFU, 0x6EU },
    { 0x47U, 0xF1U, 0x1AU, 0x71U, 0x1DU, 0x29U, 0xC5U, 0x89U, 0x6FU, 0xB7U, 0x62U, 0x0EU, 0xAAU, 0x18U, 0xBEU, 0x1BU },
    { 0xFCU, 0x56U, 0x3EU, 0x4BU, 0xC6U, 0xD2U, 0x79U, 0x20U, 0x9AU, 0xDBU, 0xC0U, 0xFEU, 0x78U, 0xCDU, 0x5AU, 0xF4U },
    { 0x1FU, 0xDDU, 0xA8U, 0x33U, 0x88U, 0x07U, 0xC7U, 0x31U, 0xB1U, 0x12U, 0x10U, 0x59U, 0x27U, 0x80U, 0xECU, 0x5FU },
    { 0x60U, 0x51U, 0x7FU, 0xA9U, 0x19U, 0xB5U, 0x4AU, 0x0DU, 0x2DU, 0xE5U, 0x7AU, 0x9FU, 0x93U, 0xC9U, 0x9CU, 0xEFU },
    { 0xA0U, 0xE0U, 0x3BU, 0x4DU, 0xAEU, 0x2AU, 0xF5U, 0xB0U, 0xC8U, 0xEBU, 0xBBU, 0x3CU, 0x83U, 0x53U, 0x99U, 0x61U },
    { 0x17U, 0x2BU, 0x04U, 0x7EU, 0xBAU, 0x77U, 0xD6U, 0x26U, 0xE1U, 0x69U, 0x14U, 0x63U, 0x55U, 0x21U, 0x0CU, 0x7DU }
};

// Galois Multiplication lookup tables
static const uint8_t GF_MUL_TABLE[15][256] = {
    {},
    {},

    // mul 2
    {   
        0x00U, 0x02U, 0x04U, 0x06U, 0x08U, 0x0AU, 0x0CU, 0x0EU, 0x10U, 0x12U, 0x14U, 0x16U,
        0x18U, 0x1AU, 0x1CU, 0x1EU, 0x20U, 0x22U, 0x24U, 0x26U, 0x28U, 0x2AU, 0x2CU, 0x2EU,
        0x30U, 0x32U, 0x34U, 0x36U, 0x38U, 0x3AU, 0x3CU, 0x3EU, 0x40U, 0x42U, 0x44U, 0x46U,
        0x48U, 0x4AU, 0x4CU, 0x4EU, 0x50U, 0x52U, 0x54U, 0x56U, 0x58U, 0x5AU, 0x5CU, 0x5EU,
        0x60U, 0x62U, 0x64U, 0x66U, 0x68U, 0x6AU, 0x6CU, 0x6EU, 0x70U, 0x72U, 0x74U, 0x76U,
        0x78U, 0x7AU, 0x7CU, 0x7EU, 0x80U, 0x82U, 0x84U, 0x86U, 0x88U, 0x8AU, 0x8CU, 0x8EU,
        0x90U, 0x92U, 0x94U, 0x96U, 0x98U, 0x9AU, 0x9CU, 0x9EU, 0xA0U, 0xA2U, 0xA4U, 0xA6U,
        0xA8U, 0xAAU, 0xACU, 0xAEU, 0xB0U, 0xB2U, 0xB4U, 0xB6U, 0xB8U, 0xBAU, 0xBCU, 0xBEU,
        0xC0U, 0xC2U, 0xC4U, 0xC6U, 0xC8U, 0xCAU, 0xCCU, 0xCEU, 0xD0U, 0xD2U, 0xD4U, 0xD6U,
        0xD8U, 0xDAU, 0xDCU, 0xDEU, 0xE0U, 0xE2U, 0xE4U, 0xE6U, 0xE8U, 0xEAU, 0xECU, 0xEEU,
        0xF0U, 0xF2U, 0xF4U, 0xF6U, 0xF8U, 0xFAU, 0xFCU, 0xFEU, 0x1BU, 0x19U, 0x1FU, 0x1DU,
        0x13U, 0x11U, 0x17U, 0x15U, 0x0BU, 0x09U, 0x0FU, 0x0DU, 0x03U, 0x01U, 0x07U, 0x05U,
        0x3BU, 0x39U, 0x3FU, 0x3DU, 0x33U, 0x31U, 0x37U, 0x35U, 0x2BU, 0x29U, 0x2FU, 0x2DU,
        0x23U, 0x21U, 0x27U, 0x25U, 0x5BU, 0x59U, 0x5FU, 0x5DU, 0x53U, 0x51U, 0x57U, 0x55U,
        0x4BU, 0x49U, 0x4FU, 0x4DU, 0x43U, 0x41U, 0x47U, 0x45U, 0x7BU, 0x79U, 0x7FU, 0x7DU,
        0x73U, 0x71U, 0x77U, 0x75U, 0x6BU, 0x69U, 0x6FU, 0x6DU, 0x63U, 0x61U, 0x67U, 0x65U,
        0x9BU, 0x99U, 0x9FU, 0x9DU, 0x93U, 0x91U, 0x97U, 0x95U, 0x8BU, 0x89U, 0x8FU, 0x8DU,
        0x83U, 0x81U, 0x87U, 0x85U, 0xBBU, 0xB9U, 0xBFU, 0xBDU, 0xB3U, 0xB1U, 0xB7U, 0xB5U,
        0xABU, 0xA9U, 0xAFU, 0xADU, 0xA3U, 0xA1U, 0xA7U, 0xA5U, 0xDBU, 0xD9U, 0xDFU, 0xDDU,
        0xD3U, 0xD1U, 0xD7U, 0xD5U, 0xCBU, 0xC9U, 0xCFU, 0xCDU, 0xC3U, 0xC1U, 0xC7U, 0xC5U,
        0xFBU, 0xF9U, 0xFFU, 0xFDU, 0xF3U, 0xF1U, 0xF7U, 0xF5U, 0xEBU, 0xE9U, 0xEFU, 0xEDU,
        0xE3U, 0xE1U, 0xE7U, 0xE5U
    },

    // mul 3
    {
        0x00U, 0x03U, 0x06U, 0x05U, 0x0CU, 0x0FU, 0x0AU, 0x09U, 0x18U, 0x1BU, 0x1EU, 0x1DU,
        0x14U, 0x17U, 0x12U, 0x11U, 0x30U, 0x33U, 0x36U, 0x35U, 0x3CU, 0x3FU, 0x3AU, 0x39U,
        0x28U, 0x2BU, 0x2EU, 0x2DU, 0x24U, 0x27U, 0x22U, 0x21U, 0x60U, 0x63U, 0x66U, 0x65U,
        0x6CU, 0x6FU, 0x6AU, 0x69U, 0x78U, 0x7BU, 0x7EU, 0x7DU, 0x74U, 0x77U, 0x72U, 0x71U,
        0x50U, 0x53U, 0x56U, 0x55U, 0x5CU, 0x5FU, 0x5AU, 0x59U, 0x48U, 0x4BU, 0x4EU, 0x4DU,
        0x44U, 0x47U, 0x42U, 0x41U, 0xC0U, 0xC3U, 0xC6U, 0xC5U, 0xCCU, 0xCFU, 0xCAU, 0xC9U,
        0xD8U, 0xDBU, 0xDEU, 0xDDU, 0xD4U, 0xD7U, 0xD2U, 0xD1U, 0xF0U, 0xF3U, 0xF6U, 0xF5U,
        0xFCU, 0xFFU, 0xFAU, 0xF9U, 0xE8U, 0xEBU, 0xEEU, 0xEDU, 0xE4U, 0xE7U, 0xE2U, 0xE1U,
        0xA0U, 0xA3U, 0xA6U, 0xA5U, 0xACU, 0xAFU, 0xAAU, 0xA9U, 0xB8U, 0xBBU, 0xBEU, 0xBDU,
        0xB4U, 0xB7U, 0xB2U, 0xB1U, 0x90U, 0x93U, 0x96U, 0x95U, 0x9CU, 0x9FU, 0x9AU, 0x99U,
        0x88U, 0x8BU, 0x8EU, 0x8DU, 0x84U, 0x87U, 0x82U, 0x81U, 0x9BU, 0x98U, 0x9DU, 0x9EU,
        0x97U, 0x94U, 0x91U, 0x92U, 0x83U, 0x80U, 0x85U, 0x86U, 0x8FU, 0x8CU, 0x89U, 0x8AU,
        0xABU, 0xA8U, 0xADU, 0xAEU, 0xA7U, 0xA4U, 0xA1U, 0xA2U, 0xB3U, 0xB0U, 0xB5U, 0xB6U,
        0xBFU, 0xBCU, 0xB9U, 0xBAU, 0xFBU, 0xF8U, 0xFDU, 0xFEU, 0xF7U, 0xF4U, 0xF1U, 0xF2U,
        0xE3U, 0xE0U, 0xE5U, 0xE6U, 0xEFU, 0xECU, 0xE9U, 0xEAU, 0xCBU, 0xC8U, 0xCDU, 0xCEU,
        0xC7U, 0xC4U, 0xC1U, 0xC2U, 0xD3U, 0xD0U, 0xD5U, 0xD6U, 0xDFU, 0xDCU, 0xD9U, 0xDAU,
        0x5BU, 0x58U, 0x5DU, 0x5EU, 0x57U, 0x54U, 0x51U, 0x52U, 0x43U, 0x40U, 0x45U, 0x46U,
        0x4FU, 0x4CU, 0x49U, 0x4AU, 0x6BU, 0x68U, 0x6DU, 0x6EU, 0x67U, 0x64U, 0x61U, 0x62U,
        0x73U, 0x70U, 0x75U, 0x76U, 0x7FU, 0x7CU, 0x79U, 0x7AU, 0x3BU, 0x38U, 0x3DU, 0x3EU,
        0x37U, 0x34U, 0x31U, 0x32U, 0x23U, 0x20U, 0x25U, 0x26U, 0x2FU, 0x2CU, 0x29U, 0x2AU,
        0x0BU, 0x08U, 0x0DU, 0x0EU, 0x07U, 0x04U, 0x01U, 0x02U, 0x13U, 0x10U, 0x15U, 0x16U,
        0x1FU, 0x1CU, 0x19U, 0x1AU
    },

    {},
    {},
    {},
    {},
    {},

    // mul 9
    {
        0x00U, 0x09U, 0x12U, 0x1BU, 0x24U, 0x2DU, 0x36U, 0x3FU, 0x48U, 0x41U, 0x5AU, 0x53U,
        0x6CU, 0x65U, 0x7EU, 0x77U, 0x90U, 0x99U, 0x82U, 0x8BU, 0xB4U, 0xBDU, 0xA6U, 0xAFU,
        0xD8U, 0xD1U, 0xCAU, 0xC3U, 0xFCU, 0xF5U, 0xEEU, 0xE7U, 0x3BU, 0x32U, 0x29U, 0x20U,
        0x1FU, 0x16U, 0x0DU, 0x04U, 0x73U, 0x7AU, 0x61U, 0x68U, 0x57U, 0x5EU, 0x45U, 0x4CU,
        0xABU, 0xA2U, 0xB9U, 0xB0U, 0x8FU, 0x86U, 0x9DU, 0x94U, 0xE3U, 0xEAU, 0xF1U, 0xF8U,
        0xC7U, 0xCEU, 0xD5U, 0xDCU, 0x76U, 0x7FU, 0x64U, 0x6DU, 0x52U, 0x5BU, 0x40U, 0x49U,
        0x3EU, 0x37U, 0x2CU, 0x25U, 0x1AU, 0x13U, 0x08U, 0x01U, 0xE6U, 0xEFU, 0xF4U, 0xFDU,
        0xC2U, 0xCBU, 0xD0U, 0xD9U, 0xAEU, 0xA7U, 0xBCU, 0xB5U, 0x8AU, 0x83U, 0x98U, 0x91U,
        0x4DU, 0x44U, 0x5FU, 0x56U, 0x69U, 0x60U, 0x7BU, 0x72U, 0x05U, 0x0CU, 0x17U, 0x1EU,
        0x21U, 0x28U, 0x33U, 0x3AU, 0xDDU, 0xD4U, 0xCFU, 0xC6U, 0xF9U, 0xF0U, 0xEBU, 0xE2U,
        0x95U, 0x9CU, 0x87U, 0x8EU, 0xB1U, 0xB8U, 0xA3U, 0xAAU, 0xECU, 0xE5U, 0xFEU, 0xF7U,
        0xC8U, 0xC1U, 0xDAU, 0xD3U, 0xA4U, 0xADU, 0xB6U, 0xBFU, 0x80U, 0x89U, 0x92U, 0x9BU,
        0x7CU, 0x75U, 0x6EU, 0x67U, 0x58U, 0x51U, 0x4AU, 0x43U, 0x34U, 0x3DU, 0x26U, 0x2FU,
        0x10U, 0x19U, 0x02U, 0x0BU, 0xD7U, 0xDEU, 0xC5U, 0xCCU, 0xF3U, 0xFAU, 0xE1U, 0xE8U,
        0x9FU, 0x96U, 0x8DU, 0x84U, 0xBBU, 0xB2U, 0xA9U, 0xA0U, 0x47U, 0x4EU, 0x55U, 0x5CU,
        0x63U, 0x6AU, 0x71U, 0x78U, 0x0FU, 0x06U, 0x1DU, 0x14U, 0x2BU, 0x22U, 0x39U, 0x30U,
        0x9AU, 0x93U, 0x88U, 0x81U, 0xBEU, 0xB7U, 0xACU, 0xA5U, 0xD2U, 0xDBU, 0xC0U, 0xC9U,
        0xF6U, 0xFFU, 0xE4U, 0xEDU, 0x0AU, 0x03U, 0x18U, 0x11U, 0x2EU, 0x27U, 0x3CU, 0x35U,
        0x42U, 0x4BU, 0x50U, 0x59U, 0x66U, 0x6FU, 0x74U, 0x7DU, 0xA1U, 0xA8U, 0xB3U, 0xBAU,
        0x85U, 0x8CU, 0x97U, 0x9EU, 0xE9U, 0xE0U, 0xFBU, 0xF2U, 0xCDU, 0xC4U, 0xDFU, 0xD6U,
        0x31U, 0x38U, 0x23U, 0x2AU, 0x15U, 0x1CU, 0x07U, 0x0EU, 0x79U, 0x70U, 0x6BU, 0x62U,
        0x5DU, 0x54U, 0x4FU, 0x46U
    },

    {},

    // mul 11
    {
        0x00U, 0x0BU, 0x16U, 0x1DU, 0x2CU, 0x27U, 0x3AU, 0x31U, 0x58U, 0x53U, 0x4EU, 0x45U,
        0x74U, 0x7FU, 0x62U, 0x69U, 0xB0U, 0xBBU, 0xA6U, 0xADU, 0x9CU, 0x97U, 0x8AU, 0x81U,
        0xE8U, 0xE3U, 0xFEU, 0xF5U, 0xC4U, 0xCFU, 0xD2U, 0xD9U, 0x7BU, 0x70U, 0x6DU, 0x66U,
        0x57U, 0x5CU, 0x41U, 0x4AU, 0x23U, 0x28U, 0x35U, 0x3EU, 0x0FU, 0x04U, 0x19U, 0x12U,
        0xCBU, 0xC0U, 0xDDU, 0xD6U, 0xE7U, 0xECU, 0xF1U, 0xFAU, 0x93U, 0x98U, 0x85U, 0x8EU,
        0xBFU, 0xB4U, 0xA9U, 0xA2U, 0xF6U, 0xFDU, 0xE0U, 0xEBU, 0xDAU, 0xD1U, 0xCCU, 0xC7U,
        0xAEU, 0xA5U, 0xB8U, 0xB3U, 0x82U, 0x89U, 0x94U, 0x9FU, 0x46U, 0x4DU, 0x50U, 0x5BU,
        0x6AU, 0x61U, 0x7CU, 0x77U, 0x1EU, 0x15U, 0x08U, 0x03U, 0x32U, 0x39U, 0x24U, 0x2FU,
        0x8DU, 0x86U, 0x9BU, 0x90U, 0xA1U, 0xAAU, 0xB7U, 0xBCU, 0xD5U, 0xDEU, 0xC3U, 0xC8U,
        0xF9U, 0xF2U, 0xEFU, 0xE4U, 0x3DU, 0x36U, 0x2BU, 0x20U, 0x11U, 0x1AU, 0x07U, 0x0CU,
        0x65U, 0x6EU, 0x73U, 0x78U, 0x49U, 0x42U, 0x5FU, 0x54U, 0xF7U, 0xFCU, 0xE1U, 0xEAU,
        0xDBU, 0xD0U, 0xCDU, 0xC6U, 0xAFU, 0xA4U, 0xB9U, 0xB2U, 0x83U, 0x88U, 0x95U, 0x9EU,
        0x47U, 0x4CU, 0x51U, 0x5AU, 0x6BU, 0x60U, 0x7DU, 0x76U, 0x1FU, 0x14U, 0x09U, 0x02U,
        0x33U, 0x38U, 0x25U, 0x2EU, 0x8CU, 0x87U, 0x9AU, 0x91U, 0xA0U, 0xABU, 0xB6U, 0xBDU,
        0xD4U, 0xDFU, 0xC2U, 0xC9U, 0xF8U, 0xF3U, 0xEEU, 0xE5U, 0x3CU, 0x37U, 0x2AU, 0x21U,
        0x10U, 0x1BU, 0x06U, 0x0DU, 0x64U, 0x6FU, 0x72U, 0x79U, 0x48U, 0x43U, 0x5EU, 0x55U,
        0x01U, 0x0AU, 0x17U, 0x1CU, 0x2DU, 0x26U, 0x3BU, 0x30U, 0x59U, 0x52U, 0x4FU, 0x44U,
        0x75U, 0x7EU, 0x63U, 0x68U, 0xB1U, 0xBAU, 0xA7U, 0xACU, 0x9DU, 0x96U, 0x8BU, 0x80U,
        0xE9U, 0xE2U, 0xFFU, 0xF4U, 0xC5U, 0xCEU, 0xD3U, 0xD8U, 0x7AU, 0x71U, 0x6CU, 0x67U,
        0x56U, 0x5DU, 0x40U, 0x4BU, 0x22U, 0x29U, 0x34U, 0x3FU, 0x0EU, 0x05U, 0x18U, 0x13U,
        0xCAU, 0xC1U, 0xDCU, 0xD7U, 0xE6U, 0xEDU, 0xF0U, 0xFBU, 0x92U, 0x99U, 0x84U, 0x8FU,
        0xBEU, 0xB5U, 0xA8U, 0xA3U
    },

    {},

    // mul 13
    {
        0x00U, 0x0DU, 0x1AU, 0x17U, 0x34U, 0x39U, 0x2EU, 0x23U, 0x68U, 0x65U, 0x72U, 0x7FU,
        0x5CU, 0x51U, 0x46U, 0x4BU, 0xD0U, 0xDDU, 0xCAU, 0xC7U, 0xE4U, 0xE9U, 0xFEU, 0xF3U,
        0xB8U, 0xB5U, 0xA2U, 0xAFU, 0x8CU, 0x81U, 0x96U, 0x9BU, 0xBBU, 0xB6U, 0xA1U, 0xACU,
        0x8FU, 0x82U, 0x95U, 0x98U, 0xD3U, 0xDEU, 0xC9U, 0xC4U, 0xE7U, 0xEAU, 0xFDU, 0xF0U,
        0x6BU, 0x66U, 0x71U, 0x7CU, 0x5FU, 0x52U, 0x45U, 0x48U, 0x03U, 0x0EU, 0x19U, 0x14U,
        0x37U, 0x3AU, 0x2DU, 0x20U, 0x6DU, 0x60U, 0x77U, 0x7AU, 0x59U, 0x54U, 0x43U, 0x4EU,
        0x05U, 0x08U, 0x1FU, 0x12U, 0x31U, 0x3CU, 0x2BU, 0x26U, 0xBDU, 0xB0U, 0xA7U, 0xAAU,
        0x89U, 0x84U, 0x93U, 0x9EU, 0xD5U, 0xD8U, 0xCFU, 0xC2U, 0xE1U, 0xECU, 0xFBU, 0xF6U,
        0xD6U, 0xDBU, 0xCCU, 0xC1U, 0xE2U, 0xEFU, 0xF8U, 0xF5U, 0xBEU, 0xB3U, 0xA4U, 0xA9U,
        0x8AU, 0x87U, 0x90U, 0x9DU, 0x06U, 0x0BU, 0x1CU, 0x11U, 0x32U, 0x3FU, 0x28U, 0x25U,
        0x6EU, 0x63U, 0x74U, 0x79U, 0x5AU, 0x57U, 0x40U, 0x4DU, 0xDAU, 0xD7U, 0xC0U, 0xCDU,
        0xEEU, 0xE3U, 0xF4U, 0xF9U, 0xB2U, 0xBFU, 0xA8U, 0xA5U, 0x86U, 0x8BU, 0x9CU, 0x91U,
        0x0AU, 0x07U, 0x10U, 0x1DU, 0x3EU, 0x33U, 0x24U, 0x29U, 0x62U, 0x6FU, 0x78U, 0x75U,
        0x56U, 0x5BU, 0x4CU, 0x41U, 0x61U, 0x6CU, 0x7BU, 0x76U, 0x55U, 0x58U, 0x4FU, 0x42U,
        0x09U, 0x04U, 0x13U, 0x1EU, 0x3DU, 0x30U, 0x27U, 0x2AU, 0xB1U, 0xBCU, 0xABU, 0xA6U,
        0x85U, 0x88U, 0x9FU, 0x92U, 0xD9U, 0xD4U, 0xC3U, 0xCEU, 0xEDU, 0xE0U, 0xF7U, 0xFAU,
        0xB7U, 0xBAU, 0xADU, 0xA0U, 0x83U, 0x8EU, 0x99U, 0x94U, 0xDFU, 0xD2U, 0xC5U, 0xC8U,
        0xEBU, 0xE6U, 0xF1U, 0xFCU, 0x67U, 0x6AU, 0x7DU, 0x70U, 0x53U, 0x5EU, 0x49U, 0x44U,
        0x0FU, 0x02U, 0x15U, 0x18U, 0x3BU, 0x36U, 0x21U, 0x2CU, 0x0CU, 0x01U, 0x16U, 0x1BU,
        0x38U, 0x35U, 0x22U, 0x2FU, 0x64U, 0x69U, 0x7EU, 0x73U, 0x50U, 0x5DU, 0x4AU, 0x47U,
        0xDCU, 0xD1U, 0xC6U, 0xCBU, 0xE8U, 0xE5U, 0xF2U, 0xFFU, 0xB4U, 0xB9U, 0xAEU, 0xA3U,
        0x80U, 0x8DU, 0x9AU, 0x97U
    },

    // mul 14
    {
        0x00U, 0x0EU, 0x1CU, 0x12U, 0x38U, 0x36U, 0x24U, 0x2AU, 0x70U, 0x7EU, 0x6CU, 0x62U,
        0x48U, 0x46U, 0x54U, 0x5AU, 0xE0U, 0xEEU, 0xFCU, 0xF2U, 0xD8U, 0xD6U, 0xC4U, 0xCAU,
        0x90U, 0x9EU, 0x8CU, 0x82U, 0xA8U, 0xA6U, 0xB4U, 0xBAU, 0xDBU, 0xD5U, 0xC7U, 0xC9U,
        0xE3U, 0xEDU, 0xFFU, 0xF1U, 0xABU, 0xA5U, 0xB7U, 0xB9U, 0x93U, 0x9DU, 0x8FU, 0x81U,
        0x3BU, 0x35U, 0x27U, 0x29U, 0x03U, 0x0DU, 0x1FU, 0x11U, 0x4BU, 0x45U, 0x57U, 0x59U,
        0x73U, 0x7DU, 0x6FU, 0x61U, 0xADU, 0xA3U, 0xB1U, 0xBFU, 0x95U, 0x9BU, 0x89U, 0x87U,
        0xDDU, 0xD3U, 0xC1U, 0xCFU, 0xE5U, 0xEBU, 0xF9U, 0xF7U, 0x4DU, 0x43U, 0x51U, 0x5FU,
        0x75U, 0x7BU, 0x69U, 0x67U, 0x3DU, 0x33U, 0x21U, 0x2FU, 0x05U, 0x0BU, 0x19U, 0x17U,
        0x76U, 0x78U, 0x6AU, 0x64U, 0x4EU, 0x40U, 0x52U, 0x5CU, 0x06U, 0x08U, 0x1AU, 0x14U,
        0x3EU, 0x30U, 0x22U, 0x2CU, 0x96U, 0x98U, 0x8AU, 0x84U, 0xAEU, 0xA0U, 0xB2U, 0xBCU,
        0xE6U, 0xE8U, 0xFAU, 0xF4U, 0xDEU, 0xD0U, 0xC2U, 0xCCU, 0x41U, 0x4FU, 0x5DU, 0x53U,
        0x79U, 0x77U, 0x65U, 0x6BU, 0x31U, 0x3FU, 0x2DU, 0x23U, 0x09U, 0x07U, 0x15U, 0x1BU,
        0xA1U, 0xAFU, 0xBDU, 0xB3U, 0x99U, 0x97U, 0x85U, 0x8BU, 0xD1U, 0xDFU, 0xCDU, 0xC3U,
        0xE9U, 0xE7U, 0xF5U, 0xFBU, 0x9AU, 0x94U, 0x86U, 0x88U, 0xA2U, 0xACU, 0xBEU, 0xB0U,
        0xEAU, 0xE4U, 0xF6U, 0xF8U, 0xD2U, 0xDCU, 0xCEU, 0xC0U, 0x7AU, 0x74U, 0x66U, 0x68U,
        0x42U, 0x4CU, 0x5EU, 0x50U, 0x0AU, 0x04U, 0x16U, 0x18U, 0x32U, 0x3CU, 0x2EU, 0x20U,
        0xECU, 0xE2U, 0xF0U, 0xFEU, 0xD4U, 0xDAU, 0xC8U, 0xC6U, 0x9CU, 0x92U, 0x80U, 0x8EU,
        0xA4U, 0xAAU, 0xB8U, 0xB6U, 0x0CU, 0x02U, 0x10U, 0x1EU, 0x34U, 0x3AU, 0x28U, 0x26U,
        0x7CU, 0x72U, 0x60U, 0x6EU, 0x44U, 0x4AU, 0x58U, 0x56U, 0x37U, 0x39U, 0x2BU, 0x25U,
        0x0FU, 0x01U, 0x13U, 0x1DU, 0x47U, 0x49U, 0x5BU, 0x55U, 0x7FU, 0x71U, 0x63U, 0x6DU,
        0xD7U, 0xD9U, 0xCBU, 0xC5U, 0xEFU, 0xE1U, 0xF3U, 0xFDU, 0xA7U, 0xA9U, 0xBBU, 0xB5U,
        0x9FU, 0x91U, 0x83U, 0x8DU
    }
};

// Circulant MDS matrix
static const uint8_t CMDS[4][4] = { {2, 3, 1, 1}, {1, 2, 3, 1}, {1, 1, 2, 3}, {3, 1, 1, 2} };

// Inverse circulant MDS matrix
static const uint8_t INV_CMDS[4][4] = { {14, 11, 13, 9}, {9, 14, 11, 13}, {13, 9, 14, 11}, {11, 13, 9, 14} };

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the AES class.
/// </summary>
AES::AES(const AESKeyLength keyLength) {
    switch (keyLength) {
        case AESKeyLength::AES_128:
        this->m_Nk = 4;
        this->m_Nr = 10;
        break;
        case AESKeyLength::AES_192:
        this->m_Nk = 6;
        this->m_Nr = 12;
        break;
        case AESKeyLength::AES_256:
        this->m_Nk = 8;
        this->m_Nr = 14;
        break;
    }
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="inLen"></param>
/// <param name="key"></param>
/// <returns></returns>
uint8_t* AES::encryptECB(const uint8_t in[], uint32_t inLen, const uint8_t key[]) 
{
    if (inLen % m_blockBytesLen != 0) {
        throw std::length_error("Plaintext length must be divisible by " + std::to_string(m_blockBytesLen));
    }

    uint8_t* out = new uint8_t[inLen];
    uint8_t* roundKeys = new uint8_t[4 * AES_NB * (m_Nr + 1)];

    keyExpansion(key, roundKeys);
    for (uint32_t i = 0; i < inLen; i += m_blockBytesLen) {
        encryptBlock(in + i, out + i, roundKeys);
    }

    delete[] roundKeys;
    return out;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="inLen"></param>
/// <param name="key"></param>
/// <returns></returns>
uint8_t* AES::decryptECB(const uint8_t in[], uint32_t inLen, const uint8_t key[]) 
{
    if (inLen % m_blockBytesLen != 0) {
        throw std::length_error("Plaintext length must be divisible by " + std::to_string(m_blockBytesLen));
    }

    uint8_t* out = new uint8_t[inLen];
    uint8_t* roundKeys = new uint8_t[4 * AES_NB * (m_Nr + 1)];

    keyExpansion(key, roundKeys);
    for (uint32_t i = 0; i < inLen; i += m_blockBytesLen) {
        decryptBlock(in + i, out + i, roundKeys);
    }

    delete[] roundKeys;
    return out;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="inLen"></param>
/// <param name="key"></param>
/// <param name="iv"></param>
/// <returns></returns>
uint8_t* AES::encryptCBC(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t* iv) 
{
    if (inLen % m_blockBytesLen != 0) {
        throw std::length_error("Plaintext length must be divisible by " + std::to_string(m_blockBytesLen));
    }

    uint8_t* out = new uint8_t[inLen];
    uint8_t block[m_blockBytesLen];
    uint8_t* roundKeys = new uint8_t[4 * AES_NB * (m_Nr + 1)];

    keyExpansion(key, roundKeys);
    memcpy(block, iv, m_blockBytesLen);
    for (uint32_t i = 0; i < inLen; i += m_blockBytesLen) {
        xorBlocks(block, in + i, block, m_blockBytesLen);
        encryptBlock(block, out + i, roundKeys);
        memcpy(block, out + i, m_blockBytesLen);
    }

    delete[] roundKeys;
    return out;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="inLen"></param>
/// <param name="key"></param>
/// <param name="iv"></param>
/// <returns></returns>
uint8_t *AES::decryptCBC(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t *iv) 
{
    if (inLen % m_blockBytesLen != 0) {
        throw std::length_error("Plaintext length must be divisible by " + std::to_string(m_blockBytesLen));
    }

    uint8_t* out = new uint8_t[inLen];
    uint8_t block[m_blockBytesLen];
    uint8_t* roundKeys = new uint8_t[4 * AES_NB * (m_Nr + 1)];

    keyExpansion(key, roundKeys);
    memcpy(block, iv, m_blockBytesLen);
    for (uint32_t i = 0; i < inLen; i += m_blockBytesLen) {
        decryptBlock(in + i, out + i, roundKeys);
        xorBlocks(block, out + i, out + i, m_blockBytesLen);
        memcpy(block, in + i, m_blockBytesLen);
    }

    delete[] roundKeys;
    return out;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="inLen"></param>
/// <param name="key"></param>
/// <param name="iv"></param>
/// <returns></returns>
uint8_t *AES::encryptCFB(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t *iv) 
{
    if (inLen % m_blockBytesLen != 0) {
        throw std::length_error("Plaintext length must be divisible by " + std::to_string(m_blockBytesLen));
    }

    uint8_t* out = new uint8_t[inLen];
    uint8_t block[m_blockBytesLen];
    uint8_t encryptedBlock[m_blockBytesLen];
    uint8_t* roundKeys = new uint8_t[4 * AES_NB * (m_Nr + 1)];

    keyExpansion(key, roundKeys);
    memcpy(block, iv, m_blockBytesLen);
    for (uint32_t i = 0; i < inLen; i += m_blockBytesLen) {
        encryptBlock(block, encryptedBlock, roundKeys);
        xorBlocks(in + i, encryptedBlock, out + i, m_blockBytesLen);
        memcpy(block, out + i, m_blockBytesLen);
    }

    delete[] roundKeys;
    return out;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="inLen"></param>
/// <param name="key"></param>
/// <param name="iv"></param>
/// <returns></returns>
uint8_t *AES::decryptCFB(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t *iv) 
{
    if (inLen % m_blockBytesLen != 0) {
        throw std::length_error("Plaintext length must be divisible by " + std::to_string(m_blockBytesLen));
    }

    uint8_t* out = new uint8_t[inLen];
    uint8_t block[m_blockBytesLen];
    uint8_t encryptedBlock[m_blockBytesLen];
    uint8_t* roundKeys = new uint8_t[4 * AES_NB * (m_Nr + 1)];

    keyExpansion(key, roundKeys);
    memcpy(block, iv, m_blockBytesLen);
    for (uint32_t i = 0; i < inLen; i += m_blockBytesLen) {
        encryptBlock(block, encryptedBlock, roundKeys);
        xorBlocks(in + i, encryptedBlock, out + i, m_blockBytesLen);
        memcpy(block, in + i, m_blockBytesLen);
    }

    delete[] roundKeys;
    return out;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="state"></param>
void AES::subBytes(uint8_t state[4][AES_NB]) {
    uint8_t t;
    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < AES_NB; j++) {
            t = state[i][j];
            state[i][j] = SBOX[t / 16][t % 16];
        }
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="state"></param>
void AES::invSubBytes(uint8_t state[4][AES_NB]) 
{
    uint8_t t;
    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < AES_NB; j++) {
            t = state[i][j];
            state[i][j] = INV_SBOX[t / 16][t % 16];
        }
    }
}

/// <summary>
/// Shift row i on n positions.
/// </summary>
/// <param name="state"></param>
/// <param name="i"></param>
/// <param name="n"></param>
void AES::shiftRow(uint8_t state[4][AES_NB], uint32_t i, uint32_t n)
{
    uint8_t tmp[AES_NB];
    for (uint32_t j = 0; j < AES_NB; j++) {
        tmp[j] = state[i][(j + n) % AES_NB];
    }
    memcpy(state[i], tmp, AES_NB * sizeof(uint8_t));
}

/// <summary>
/// 
/// </summary>
/// <param name="state"></param>
void AES::shiftRows(uint8_t state[4][AES_NB]) 
{
    shiftRow(state, 1, 1);
    shiftRow(state, 2, 2);
    shiftRow(state, 3, 3);
}

/// <summary>
/// 
/// </summary>
/// <param name="state"></param>
void AES::invShiftRows(uint8_t state[4][AES_NB]) 
{
    shiftRow(state, 1, AES_NB - 1);
    shiftRow(state, 2, AES_NB - 2);
    shiftRow(state, 3, AES_NB - 3);
}

/// <summary>
/// 
/// </summary>
/// <param name="state"></param>
void AES::mixColumns(uint8_t state[4][AES_NB]) {
    uint8_t tempState[4][AES_NB];
    for (size_t i = 0; i < 4; ++i) {
        memset(tempState[i], 0, 4);
    }

    for (size_t i = 0; i < 4; ++i) {
        for (size_t k = 0; k < 4; ++k) {
            for (size_t j = 0; j < 4; ++j) {
            if (CMDS[i][k] == 1)
                tempState[i][j] ^= state[k][j];
            else
                tempState[i][j] ^= GF_MUL_TABLE[CMDS[i][k]][state[k][j]];
            }
        }
    }

    for (size_t i = 0; i < 4; ++i) {
        memcpy(state[i], tempState[i], 4);
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="state"></param>
void AES::invMixColumns(uint8_t state[4][AES_NB]) {
    uint8_t tempState[4][AES_NB];
    for (size_t i = 0; i < 4; ++i) {
        memset(tempState[i], 0, 4);
    }

    for (size_t i = 0; i < 4; ++i) {
        for (size_t k = 0; k < 4; ++k) {
            for (size_t j = 0; j < 4; ++j) {
                tempState[i][j] ^= GF_MUL_TABLE[INV_CMDS[i][k]][state[k][j]];
            }
        }
    }

    for (size_t i = 0; i < 4; ++i) {
        memcpy(state[i], tempState[i], 4);
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="state"></param>
/// <param name="key"></param>
void AES::addRoundKey(uint8_t state[4][AES_NB], uint8_t* key) 
{
    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < AES_NB; j++) {
            state[i][j] = state[i][j] ^ key[i + 4 * j];
        }
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="a"></param>
void AES::subWord(uint8_t* a) 
{
    for (uint32_t i = 0; i < 4; i++) {
        a[i] = SBOX[a[i] / 16][a[i] % 16];
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="a"></param>
void AES::rotWord(uint8_t* a) 
{
    uint8_t c = a[0];
    a[0] = a[1];
    a[1] = a[2];
    a[2] = a[3];
    a[3] = c;
}

/// <summary>
/// 
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <param name="c"></param>
void AES::xorWords(uint8_t* a, uint8_t* b, uint8_t* c) 
{
    for (uint32_t i = 0; i < 4; i++) {
        c[i] = a[i] ^ b[i];
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="a"></param>
/// <param name="n"></param>
void AES::rcon(uint8_t *a, uint32_t n) 
{
    uint8_t c = 1;
    for (uint32_t i = 0; i < n - 1; i++) {
        c = xtime(c);
    }

    a[0] = c;
    a[1] = a[2] = a[3] = 0;
}

/// <summary>
/// 
/// </summary>
/// <param name="key"></param>
/// <param name="w"></param>
void AES::keyExpansion(const uint8_t key[], uint8_t w[]) {
    uint8_t temp[4], rcon[4];

    uint32_t i = 0;
    while (i < 4 * m_Nk) {
        w[i] = key[i];
        i++;
    }

    i = 4 * m_Nk;
    while (i < 4 * AES_NB * (m_Nr + 1)) {
        temp[0] = w[i - 4 + 0];
        temp[1] = w[i - 4 + 1];
        temp[2] = w[i - 4 + 2];
        temp[3] = w[i - 4 + 3];

        if (i / 4 % m_Nk == 0) {
            rotWord(temp);
            subWord(temp);
            this->rcon(rcon, i / (m_Nk * 4));
            xorWords(temp, rcon, temp);
        } else if (m_Nk > 6 && i / 4 % m_Nk == 4) {
            subWord(temp);
        }

        w[i + 0] = w[i - 4 * m_Nk] ^ temp[0];
        w[i + 1] = w[i + 1 - 4 * m_Nk] ^ temp[1];
        w[i + 2] = w[i + 2 - 4 * m_Nk] ^ temp[2];
        w[i + 3] = w[i + 3 - 4 * m_Nk] ^ temp[3];
        i += 4;
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="roundKeys"></param>
void AES::encryptBlock(const uint8_t in[], uint8_t out[], uint8_t* roundKeys) 
{
    uint8_t state[4][AES_NB];
    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < AES_NB; j++) {
            state[i][j] = in[i + 4 * j];
        }
    }

    addRoundKey(state, roundKeys);

    for (uint32_t round = 1; round <= m_Nr - 1; round++) {
        subBytes(state);
        shiftRows(state);
        mixColumns(state);
        addRoundKey(state, roundKeys + round * 4 * AES_NB);
    }

    subBytes(state);
    shiftRows(state);
    addRoundKey(state, roundKeys + m_Nr * 4 * AES_NB);

    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < AES_NB; j++) {
            out[i + 4 * j] = state[i][j];
        }
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="roundKeys"></param>
void AES::decryptBlock(const uint8_t in[], uint8_t out[], uint8_t* roundKeys) 
{
    uint8_t state[4][AES_NB];
    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < AES_NB; j++) {
            state[i][j] = in[i + 4 * j];
        }
    }

    addRoundKey(state, roundKeys + m_Nr * 4 * AES_NB);

    for (uint32_t round = m_Nr - 1; round >= 1; round--) {
        invSubBytes(state);
        invShiftRows(state);
        addRoundKey(state, roundKeys + round * 4 * AES_NB);
        invMixColumns(state);
    }

    invSubBytes(state);
    invShiftRows(state);
    addRoundKey(state, roundKeys);

    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < AES_NB; j++) {
            out[i + 4 * j] = state[i][j];
        }
    }
}

/// <summary>
/// 
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <param name="c"></param>
/// <param name="len"></param>
void AES::xorBlocks(const uint8_t *a, const uint8_t *b, uint8_t *c, uint32_t len) 
{
    for (uint32_t i = 0; i < len; i++) {
        c[i] = a[i] ^ b[i];
    }
}
