// Copyright (c) 2021 The Bitcoin Core developers
// Copyright (c) 2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <node/context.h>

#include <any>

NodeContext& EnsureAnyNodeContext(const std::any& context);
