/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2011 Tatsuhiro Tsujikawa
 * Modified by dta0one for ariam, 2026-10-11.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_RPC_HELPER_H
#define D_RPC_HELPER_H

#include "common.h"

#include <cstdlib>
#include <string>
#include <memory>

#include "RpcRequest.h"

namespace aria2 {

class ValueBase;
class List;
class Dict;
class DownloadEngine;

namespace rpc {

struct RpcResponse;

#ifdef ENABLE_XML_RPC
RpcRequest xmlParseMemory(const char* xml, size_t size);
#endif // ENABLE_XML_RPC

// Creates error response. The |code| is the JSON-RPC error code.  The
// |msg| is the error message. The |id| is the id of the request .
RpcResponse createJsonRpcErrorResponse(int code, const std::string& msg,
                                       std::unique_ptr<ValueBase> id);

// Stores JSON-RPC request |jsondict| in |req| and returns nullptr, or
// returns the error response if |jsondict| is not a valid request.
std::unique_ptr<RpcResponse> parseJsonRpcRequest(Dict* jsondict,
                                                 RpcRequest& req);

// The token check of RpcMethod::authorize() on the first element of
// |params|, without removing it. False if |params| or |e| is null.
bool hasValidToken(const List* params, DownloadEngine* e);

// True if |req| is on the status page passphrase allowlist.
bool isPassphraseAllowed(const RpcRequest& req);

// Every RPC call runs through here, including each system.multicall
// sub-request. A passphrase-authorized |req| carrying a valid token
// takes the token path with the flag cleared. Any other
// passphrase-authorized |req| that isPassphraseAllowed() rejects gets
// the unauthorized response without running; an allowed one carrying
// an invalid token is rejected by RpcMethod::authorize().
RpcResponse dispatch(RpcRequest req, DownloadEngine* e);

// Processes JSON-RPC request |jsondict| and returns the result.
RpcResponse processJsonRpcRequest(Dict* jsondict, DownloadEngine* e);

} // namespace rpc

} // namespace aria2

#endif // D_RPC_HELPER_H
