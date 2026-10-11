/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2009 Tatsuhiro Tsujikawa
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
#include "HttpServerBodyCommand.h"
#include "SocketCore.h"
#include "DownloadEngine.h"
#include "HttpServer.h"
#include "HttpHeader.h"
#include "Logger.h"
#include "LogFactory.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "RecoverableException.h"
#include "HttpServerResponseCommand.h"
#include "DelayedCommand.h"
#include "OptionParser.h"
#include "OptionHandler.h"
#include "wallclock.h"
#include "util.h"
#include "fmt.h"
#include "SocketRecvBuffer.h"
#include "json.h"
#include "DlAbortEx.h"
#include "message.h"
#include "RpcMethod.h"
#include "RpcMethodFactory.h"
#include "RpcMethodImpl.h"
#include "RpcRequest.h"
#include "RpcResponse.h"
#include "rpc_helper.h"
#include "JsonDiskWriter.h"
#include "ValueBaseJsonParser.h"
#include "StatusPage.h"
#ifdef ENABLE_XML_RPC
#  include "XmlRpcRequestParserStateMachine.h"
#  include "XmlRpcDiskWriter.h"
#endif // ENABLE_XML_RPC

namespace aria2 {

HttpServerBodyCommand::HttpServerBodyCommand(
    cuid_t cuid, const std::shared_ptr<HttpServer>& httpServer,
    DownloadEngine* e, const std::shared_ptr<SocketCore>& socket)
    : Command(cuid),
      e_(e),
      socket_(socket),
      httpServer_(httpServer),
      writeCheck_(false)
{
  // To handle Content-Length == 0 case
  setStatus(Command::STATUS_ONESHOT_REALTIME);
  e_->addSocketForReadCheck(socket_, this);
  if (!httpServer_->getSocketRecvBuffer()->bufferEmpty() ||
      socket_->getRecvBufferedLength()) {
    e_->setNoWait(true);
  }
}

HttpServerBodyCommand::~HttpServerBodyCommand()
{
  e_->deleteSocketForReadCheck(socket_, this);
  if (writeCheck_) {
    e_->deleteSocketForWriteCheck(socket_, this);
  }
}

namespace {
std::string getJsonRpcContentType(bool script)
{
  return script ? "text/javascript" : "application/json-rpc";
}
} // namespace

namespace {
// How the tokens of req, a call on the status page path, authorize it.
// The calls of a system.multicall are its method specs; of any other
// call, the call itself.
struct TokenCalls {
  bool anyValid;
  // req carries no token, and is not a system.multicall whose method
  // specs, one or more, all carry a valid token.
  bool needsPassphrase;
};

TokenCalls getTokenCalls(const rpc::RpcRequest& req, DownloadEngine* e)
{
  if (rpc::hasToken(req)) {
    return {rpc::hasValidToken(req.params.get(), e), false};
  }
  const List* specs = nullptr;
  if (req.methodName == rpc::SystemMulticallRpcMethod::getMethodName() &&
      req.params && !req.params->empty()) {
    specs = downcast<List>(req.params->get(0));
  }
  if (!specs || specs->empty()) {
    return {false, true};
  }
  TokenCalls res{false, false};
  for (const auto& spec : *specs) {
    auto dict = downcast<Dict>(spec);
    const List* params = dict ? downcast<List>(dict->get("params")) : nullptr;
    if (rpc::hasValidToken(params, e)) {
      res.anyValid = true;
    }
    else {
      res.needsPassphrase = true;
    }
  }
  return res;
}

// Runs json, a call or batch that passed
// status_page::acceptsPassphraseRpc(), storing one response per call in
// results, in order. A call carrying a token is authorized by its token
// alone. The passphrase is checked once, only if a valid call needs it.
// If the client is locked out, returns false without running any call,
// unless a call carries a valid token: then every call takes the token
// path. On a wrong passphrase, the calls that need it take the token
// path and are unauthorized.
bool processPassphraseJsonRpc(const HttpServer& server, DownloadEngine* e,
                              ValueBase* json,
                              std::vector<rpc::RpcResponse>& results)
{
  std::vector<Dict*> calls;
  auto batch = downcast<List>(json);
  if (batch) {
    for (auto& v : *batch) {
      auto call = downcast<Dict>(v);
      if (call) {
        calls.push_back(call);
      }
    }
  }
  else {
    auto call = downcast<Dict>(json);
    if (!call) {
      results.push_back(rpc::createJsonRpcErrorResponse(
          -32600, "Invalid Request.", Null::g()));
      return true;
    }
    calls.push_back(call);
  }
  // Every call is parsed before any runs, so that a lockout runs none.
  std::vector<rpc::RpcRequest> reqs(calls.size());
  std::vector<std::unique_ptr<rpc::RpcResponse>> errors;
  std::vector<bool> needs(calls.size(), false);
  bool anyValid = false;
  bool needsPassphrase = false;
  for (size_t i = 0; i < calls.size(); ++i) {
    errors.push_back(rpc::parseJsonRpcRequest(calls[i], reqs[i]));
    if (!errors.back()) {
      auto tokenCalls = getTokenCalls(reqs[i], e);
      needs[i] = tokenCalls.needsPassphrase;
      anyValid = anyValid || tokenCalls.anyValid;
      needsPassphrase = needsPassphrase || tokenCalls.needsPassphrase;
    }
  }
  bool passphraseOK = false;
  if (needsPassphrase) {
    switch (status_page::checkBasicAuthorization(server, e)) {
    case PASSPHRASE_OK:
      passphraseOK = true;
      break;
    case PASSPHRASE_LOCKED:
      if (!anyValid) {
        return false;
      }
      break;
    case PASSPHRASE_MISSING:
    case PASSPHRASE_WRONG:
      break;
    }
  }
  for (size_t i = 0; i < calls.size(); ++i) {
    if (errors[i]) {
      results.push_back(std::move(*errors[i]));
      continue;
    }
    reqs[i].passphraseAuthorized = passphraseOK && needs[i];
    results.push_back(rpc::dispatch(std::move(reqs[i]), e));
  }
  return true;
}
} // namespace

void HttpServerBodyCommand::sendJsonRpcResponse(const rpc::RpcResponse& res,
                                                const std::string& callback)
{
  bool notauthorized = rpc::not_authorized(res);
  bool gzip = httpServer_->supportsGZip();
  std::string responseData = rpc::toJson(res, callback, gzip);
  if (res.code == 0) {
    httpServer_->feedResponse(std::move(responseData),
                              getJsonRpcContentType(!callback.empty()));
  }
  else {
    httpServer_->disableKeepAlive();
    int httpCode;
    switch (res.code) {
    case 1:
      // error caught while executing RpcMethod
      httpCode = 400;
      break;
    case -32600:
      httpCode = 400;
      break;
    case -32601:
      httpCode = 404;
      break;
    default:
      httpCode = 500;
    };
    httpServer_->feedResponse(httpCode, A2STR::NIL, std::move(responseData),
                              getJsonRpcContentType(!callback.empty()));
  }
  addHttpServerResponseCommand(notauthorized);
}

void HttpServerBodyCommand::sendJsonRpcBatchResponse(
    const std::vector<rpc::RpcResponse>& results, const std::string& callback)
{
  bool notauthorized = rpc::any_not_authorized(results.begin(), results.end());
  bool gzip = httpServer_->supportsGZip();
  std::string responseData = rpc::toJsonBatch(results, callback, gzip);
  httpServer_->feedResponse(std::move(responseData),
                            getJsonRpcContentType(!callback.empty()));
  addHttpServerResponseCommand(notauthorized);
}

void HttpServerBodyCommand::addHttpServerResponseCommand(bool delayed)
{
  auto resp = make_unique<HttpServerResponseCommand>(getCuid(), httpServer_, e_,
                                                     socket_);
  if (delayed) {
    resp->disableWriteCheckSocket();
    e_->addCommand(
        make_unique<DelayedCommand>(getCuid(), e_, 1_s, std::move(resp), true));
    return;
  }

  e_->addCommand(std::move(resp));
  e_->setNoWait(true);
}

void HttpServerBodyCommand::updateWriteCheck()
{
  if (httpServer_->wantWrite()) {
    if (!writeCheck_) {
      writeCheck_ = true;
      e_->addSocketForWriteCheck(socket_, this);
    }
  }
  else if (writeCheck_) {
    writeCheck_ = false;
    e_->deleteSocketForWriteCheck(socket_, this);
  }
}

bool HttpServerBodyCommand::execute()
{
  if (e_->getRequestGroupMan()->downloadFinished() || e_->isHaltRequested()) {
    return true;
  }
  try {
    if (socket_->isReadable(0) || (writeCheck_ && socket_->isWritable(0)) ||
        socket_->getRecvBufferedLength() ||
        !httpServer_->getSocketRecvBuffer()->bufferEmpty() ||
        httpServer_->getContentLength() == 0) {
      timeoutTimer_ = global::wallclock();

      if (httpServer_->receiveBody()) {
        std::string reqPath = httpServer_->getRequestPath();
        reqPath.erase(std::find(reqPath.begin(), reqPath.end(), '#'),
                      reqPath.end());
        std::string query(std::find(reqPath.begin(), reqPath.end(), '?'),
                          reqPath.end());
        reqPath.erase(reqPath.size() - query.size(), query.size());

        if (httpServer_->getMethod() == "OPTIONS") {
          // Response to Preflight Request.
          // See http://www.w3.org/TR/cors/
          auto& header = httpServer_->getRequestHeader();
          std::string accessControlHeaders;
          if (!header->find(HttpHeader::ORIGIN).empty() &&
              !header->find(HttpHeader::ACCESS_CONTROL_REQUEST_METHOD)
                   .empty() &&
              !httpServer_->getAllowOrigin().empty()) {
            accessControlHeaders +=
                "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                "Access-Control-Max-Age: 1728000\r\n";
            const std::string& accReqHeaders =
                header->find(HttpHeader::ACCESS_CONTROL_REQUEST_HEADERS);
            if (!accReqHeaders.empty()) {
              // We allow all headers requested.
              accessControlHeaders += "Access-Control-Allow-Headers: ";
              accessControlHeaders += accReqHeaders;
              accessControlHeaders += "\r\n";
            }
          }
          httpServer_->feedResponse(200, accessControlHeaders);
          addHttpServerResponseCommand(false);
          return true;
        }

        // Do something for requestpath and body
        switch (httpServer_->getRequestType()) {
        case RPC_TYPE_XML: {
#ifdef ENABLE_XML_RPC
          auto dw = static_cast<rpc::XmlRpcDiskWriter*>(httpServer_->getBody());
          int error;
          error = dw->finalize();
          rpc::RpcRequest req;
          if (error == 0) {
            req = dw->getResult();
          }
          dw->reset();
          if (error < 0) {
            A2_LOG_INFO(fmt("CUID#%" PRId64
                            " - Failed to parse XML-RPC request",
                            getCuid()));
            httpServer_->feedResponse(400);
            addHttpServerResponseCommand(false);
            return true;
          }
          A2_LOG_INFO(fmt("Executing RPC method %s", req.methodName.c_str()));
          auto method = rpc::getMethod(req.methodName);
          auto res = method->execute(std::move(req), e_);
          bool gzip = httpServer_->supportsGZip();
          std::string responseData = rpc::toXml(res, gzip);
          httpServer_->feedResponse(std::move(responseData), "text/xml");
          addHttpServerResponseCommand(false);
#else  // !ENABLE_XML_RPC
          httpServer_->feedResponse(404);
          addHttpServerResponseCommand(false);
#endif // !ENABLE_XML_RPC
          return true;
        }
        case RPC_TYPE_JSON:
        case RPC_TYPE_JSONP: {
          std::string callback;
          std::unique_ptr<ValueBase> json;
          ssize_t error = 0;
          if (httpServer_->getRequestType() == RPC_TYPE_JSONP) {
            json::JsonGetParam param = json::decodeGetParams(query);
            if (httpServer_->getStatusPage() &&
                !status_page::acceptsJsonpCallback(param.callback)) {
              // Refused before parsing; answered as JSON, never as script.
              rpc::RpcResponse res(rpc::createJsonRpcErrorResponse(
                  -32600, "Invalid Request.", Null::g()));
              sendJsonRpcResponse(res, A2STR::NIL);
              return true;
            }
            callback = param.callback;
            ssize_t error = 0;
            json = json::ValueBaseJsonParser().parseFinal(
                param.request.c_str(), param.request.size(), error);
          }
          else {
            auto dw =
                static_cast<json::JsonDiskWriter*>(httpServer_->getBody());
            error = dw->finalize();
            if (error == 0) {
              json = dw->getResult();
            }
            dw->reset();
          }
          if (error < 0) {
            A2_LOG_INFO(fmt("CUID#%" PRId64
                            " - Failed to parse JSON-RPC request",
                            getCuid()));
            rpc::RpcResponse res(rpc::createJsonRpcErrorResponse(
                -32700, "Parse error.", Null::g()));
            sendJsonRpcResponse(res, callback);
            return true;
          }
          if (status_page::acceptsPassphraseRpc(*httpServer_)) {
            std::vector<rpc::RpcResponse> results;
            if (!processPassphraseJsonRpc(*httpServer_, e_, json.get(),
                                          results)) {
              // Locked out: answered at once, as on the page routes.
              httpServer_->feedResponse(429);
              addHttpServerResponseCommand(false);
            }
            else if (downcast<List>(json)) {
              sendJsonRpcBatchResponse(results, callback);
            }
            else {
              sendJsonRpcResponse(results.front(), callback);
            }
            return true;
          }
          Dict* jsondict = downcast<Dict>(json);
          if (jsondict) {
            auto res = rpc::processJsonRpcRequest(jsondict, e_);
            sendJsonRpcResponse(res, callback);
          }
          else {
            List* jsonlist = downcast<List>(json);
            if (jsonlist) {
              // This is batch call
              std::vector<rpc::RpcResponse> results;
              for (List::ValueType::const_iterator i = jsonlist->begin(),
                                                   eoi = jsonlist->end();
                   i != eoi; ++i) {
                Dict* jsondict = downcast<Dict>(*i);
                if (jsondict) {
                  auto resp = rpc::processJsonRpcRequest(jsondict, e_);
                  results.push_back(std::move(resp));
                }
              }
              sendJsonRpcBatchResponse(results, callback);
            }
            else {
              rpc::RpcResponse res(rpc::createJsonRpcErrorResponse(
                  -32600, "Invalid Request.", Null::g()));
              sendJsonRpcResponse(res, callback);
            }
          }
          return true;
        }
        case STATUS_PAGE_TYPE:
          // Answered at once, also for 401 and 429: the limiter throttles
          // guessing, and a browser without cached credentials gets a
          // credential-less 401 first.
          status_page::feedResponse(*httpServer_, e_);
          addHttpServerResponseCommand(false);
          return true;
        default:
          httpServer_->feedResponse(404);
          addHttpServerResponseCommand(false);
          return true;
        }
      }
      else {
        updateWriteCheck();
        e_->addCommand(std::unique_ptr<Command>(this));
        return false;
      }
    }
    else {
      if (timeoutTimer_.difference(global::wallclock()) >= 30_s) {
        A2_LOG_INFO("HTTP request body timeout.");
        return true;
      }
      else {
        e_->addCommand(std::unique_ptr<Command>(this));
        return false;
      }
    }
  }
  catch (RecoverableException& e) {
    A2_LOG_INFO_EX(fmt("CUID#%" PRId64
                       " - Error occurred while reading HTTP request body",
                       getCuid()),
                   e);
    return true;
  }
}

} // namespace aria2
