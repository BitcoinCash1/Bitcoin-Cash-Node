// Copyright (c) 2011-2020 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin Cash Node developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/** -upnp default */
#ifdef USE_UPNP
static const bool DEFAULT_UPNP = USE_UPNP;
#else
static const bool DEFAULT_UPNP = false;
#endif

enum class MapPortProtoFlag : unsigned int {
    NONE = 0x00,
    UPNP = 0x01,
};

void StartMapPort(bool use_upnp);
void InterruptMapPort();
void StopMapPort();
