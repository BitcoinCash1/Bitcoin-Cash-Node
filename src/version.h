// Copyright (c) 2012-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/**
 * network protocol versioning
 */
static const int PROTOCOL_VERSION = 70016;

//! initial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 209;

//! disconnect from peers older than this proto version
static const int MIN_PEER_PROTO_VERSION = 31800;

//! BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 60000;

//! "filter*" commands are disabled without NODE_BLOOM after and including this
//! version
static const int NO_BLOOM_VERSION = 70011;

//! "sendheaders" command and announcing blocks with headers starts with this
//! version
static const int SENDHEADERS_VERSION = 70012;

//! "feefilter" tells peers to filter invs to you by fee starts with this
//! version
static const int FEEFILTER_VERSION = 70013;

//! short-id-based block download starts with this version
static const int SHORT_IDS_BLOCKS_VERSION = 70014;

//! not banning for invalid compact blocks starts with this version
static const int INVALID_CB_NO_BAN_VERSION = 70015;

//! This is the first version of the software that accepts receiving unknown
//! messages before verack, without applying a banscore penalty, as part of
//! protocol feature negotiation. Versions before this will add +10 to banscore
//! if they are sent unknown messages before verack. This constant was added
//! for BIP155 "sendaddrv2" support, and is used there, but can be used to
//! conditionally omit sending any such "before verack" feature negotiation
//! messages to peers running earlier versions.
static const int FEATURE_NEGOTIATION_BEFORE_VERACK_VERSION = 70016;

// Make sure that none of the values above collide with `ADDRV2_FORMAT`.
