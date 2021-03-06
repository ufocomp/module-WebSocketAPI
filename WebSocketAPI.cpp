/*++

Program name:

  Apostol Web Service

Module Name:

  WebSocketAPI.cpp

Notices:

  Module: Web Socket API

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

//----------------------------------------------------------------------------------------------------------------------

#include "Core.hpp"
#include "WebSocketAPI.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include "jwt.h"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Workers {

        //--------------------------------------------------------------------------------------------------------------

        //-- CWebSocketAPI ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CWebSocketAPI::CWebSocketAPI(CModuleProcess *AProcess) : CApostolModule(AProcess, "web socket api") {
            m_Headers.Add("Authorization");
            m_Headers.Add("Session");
            m_Headers.Add("Secret");

            m_CheckDate = 0;

            CWebSocketAPI::InitMethods();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::InitMethods() {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            m_pMethods->AddObject(_T("GET")    , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoGet(Connection); }));
            m_pMethods->AddObject(_T("POST")   , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoPost(Connection); }));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoOptions(Connection); }));
            m_pMethods->AddObject(_T("HEAD")   , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("PUT")    , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("DELETE") , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("TRACE")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("PATCH")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
#else
            m_pMethods->AddObject(_T("GET")    , (CObject *) new CMethodHandler(true , std::bind(&CWebSocketAPI::DoGet, this, _1)));
            m_pMethods->AddObject(_T("POST")   , (CObject *) new CMethodHandler(true , std::bind(&CWebSocketAPI::DoPost, this, _1)));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true , std::bind(&CWebSocketAPI::DoOptions, this, _1)));
            m_pMethods->AddObject(_T("HEAD")   , (CObject *) new CMethodHandler(false, std::bind(&CWebSocketAPI::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PUT")    , (CObject *) new CMethodHandler(false, std::bind(&CWebSocketAPI::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("DELETE") , (CObject *) new CMethodHandler(false, std::bind(&CWebSocketAPI::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("TRACE")  , (CObject *) new CMethodHandler(false, std::bind(&CWebSocketAPI::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PATCH")  , (CObject *) new CMethodHandler(false, std::bind(&CWebSocketAPI::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CWebSocketAPI::MethodNotAllowed, this, _1)));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        int CWebSocketAPI::CheckError(const CJSON &Json, CString &ErrorMessage, bool RaiseIfError) {
            int errorCode = 0;

            if (Json.HasOwnProperty(_T("error"))) {
                const auto& error = Json[_T("error")];

                if (error.HasOwnProperty(_T("code"))) {
                    errorCode = error[_T("code")].AsInteger();
                } else {
                    errorCode = 40000;
                }

                if (error.HasOwnProperty(_T("message"))) {
                    ErrorMessage = error[_T("message")].AsString();
                } else {
                    ErrorMessage = _T("Invalid request.");
                }

                if (RaiseIfError)
                    throw EDBError(ErrorMessage.c_str());

                if (errorCode >= 10000)
                    errorCode = errorCode / 100;

                if (errorCode < 0)
                    errorCode = 400;
            }

            return errorCode;
        }
        //--------------------------------------------------------------------------------------------------------------

        CHTTPReply::CStatusType CWebSocketAPI::ErrorCodeToStatus(int ErrorCode) {
            CHTTPReply::CStatusType Status = CHTTPReply::ok;

            if (ErrorCode != 0) {
                switch (ErrorCode) {
                    case 401:
                        Status = CHTTPReply::unauthorized;
                        break;

                    case 403:
                        Status = CHTTPReply::forbidden;
                        break;

                    case 404:
                        Status = CHTTPReply::not_found;
                        break;

                    case 500:
                        Status = CHTTPReply::internal_server_error;
                        break;

                    default:
                        Status = CHTTPReply::bad_request;
                        break;
                }
            }

            return Status;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::AfterQuery(CHTTPServerConnection *AConnection, const CString &Path, const CJSON &Payload) {

            auto pSession = CSession::FindOfConnection(AConnection);

            auto SignIn = [pSession](const CJSON &Payload) {

                const auto& session = Payload[_T("session")].AsString();
                const auto& secret = Payload[_T("secret")].AsString();

                pSession->Session() = session;
                pSession->Secret() = secret;

                pSession->Authorized(true);
            };

            auto SignOut = [pSession](const CJSON &Payload) {
                pSession->Secret().Clear();
                pSession->Authorization().Clear();
                pSession->Authorized(false);
            };

            auto Authorize = [pSession](const CJSON &Payload) {

                if (Payload.HasOwnProperty(_T("authorized"))) {
                    pSession->Authorized(Payload[_T("authorized")].AsBoolean());
                }

                if (!pSession->Authorized()) {
                    pSession->Secret().Clear();
                    pSession->Authorization().Clear();

                    const auto& message = Payload[_T("message")].AsString();
                    throw Delphi::Exception::Exception(message.IsEmpty() ? _T("Unknown error.") : message.c_str());
                }
            };

            if (Path == _T("/api/v1/sign/in")) {
                SignIn(Payload);
            } else if (Path == _T("/api/v1/sign/out")) {
                SignOut(Payload);
            } else if (Path == _T("/api/v1/authenticate")) {
                Authorize(Payload);
            } else if (Path == _T("/api/v1/authorize")) {
                Authorize(Payload);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoPostgresNotify(CPQConnection *AConnection, PGnotify *ANotify) {
#ifdef _DEBUG
            const auto& Info = AConnection->ConnInfo();

            DebugMessage("[NOTIFY] [%d] [postgresql://%s@%s:%s/%s] [PID: %d] [%s] %s\n",
                AConnection->Socket(), Info["user"].c_str(), Info["host"].c_str(), Info["port"].c_str(), Info["dbname"].c_str(),
                ANotify->be_pid, ANotify->relname, ANotify->extra);
#endif
            for (int i = 0; i < m_SessionManager.Count(); ++i)
                Observer(m_SessionManager[i], ANotify->relname, ANotify->extra);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoPostgresQueryExecuted(CPQPollQuery *APollQuery) {

            auto pResult = APollQuery->Results(0);

            if (pResult->ExecStatus() != PGRES_TUPLES_OK) {
                QueryException(APollQuery, Delphi::Exception::EDBError(pResult->GetErrorMessage()));
                return;
            }

            auto pConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->Binding());

            if (pConnection != nullptr && !pConnection->ClosedGracefully()) {

                auto pWSReply = pConnection->WSReply();

                CWSMessage wsmResponse;

                wsmResponse.MessageTypeId = mtCallResult;
                wsmResponse.UniqueId = APollQuery->Data()[_T("UniqueId")];
                wsmResponse.Action = APollQuery->Data()[_T("Action")];

                const auto bDataArray = wsmResponse.Action.Find(_T("/list")) != CString::npos;

                CHTTPReply::CStatusType status = CHTTPReply::bad_request;

                try {
                    CString jsonString;
                    PQResultToJson(pResult, jsonString, bDataArray ? "array" : "object");

                    wsmResponse.Payload << jsonString;

                    if (pResult->nTuples() == 1) {
                        wsmResponse.ErrorCode = CheckError(bDataArray ? wsmResponse.Payload[0] : wsmResponse.Payload, wsmResponse.ErrorMessage);
                        if (wsmResponse.ErrorCode == 0) {
                            status = CHTTPReply::unauthorized;
                            AfterQuery(pConnection, wsmResponse.Action, wsmResponse.Payload);
                        } else {
                            wsmResponse.MessageTypeId = mtCallError;
                        }
                    }
                } catch (Delphi::Exception::Exception &E) {
                    wsmResponse.MessageTypeId = mtCallError;
                    wsmResponse.ErrorCode = status;
                    wsmResponse.ErrorMessage = E.what();

                    Log()->Error(APP_LOG_ERR, 0, "[WebSocketAPI] Error: %s", E.what());
                }

                CString sResponse;
                CWSProtocol::Response(wsmResponse, sResponse);

                pWSReply->SetPayload(sResponse);
                pConnection->SendWebSocket(true);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::QueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {

            auto pConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->Binding());

            if (pConnection != nullptr && !pConnection->ClosedGracefully()) {
                auto pWSRequest = pConnection->WSRequest();
                auto pWSReply = pConnection->WSReply();

                const CString csRequest(pWSRequest->Payload());

                CWSMessage wsmRequest;
                CWSProtocol::Request(csRequest, wsmRequest);

                CWSMessage wsmResponse;
                CString sResponse;

                CWSProtocol::PrepareResponse(wsmRequest, wsmResponse);

                wsmResponse.MessageTypeId = mtCallError;
                wsmResponse.ErrorCode = CHTTPReply::internal_server_error;
                wsmResponse.ErrorMessage = E.what();

                CWSProtocol::Response(wsmResponse, sResponse);

                pWSReply->SetPayload(sResponse);
                pConnection->SendWebSocket(true);
            }

            Log()->Error(APP_LOG_ERR, 0, "[WebSocketAPI] Query exception: %s", E.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
            QueryException(APollQuery, E);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::UnauthorizedFetch(CHTTPServerConnection *AConnection, const CString &UniqueId,
                const CString &Action, const CString &Payload, const CString &Agent, const CString &Host) {

            CStringList SQL;

            const auto &caPayload = Payload.IsEmpty() ? "null" : PQQuoteLiteral(Payload);

            SQL.Add(CString()
                            .MaxFormatSize(256 + Action.Size() + caPayload.Size() + Agent.Size())
                            .Format("SELECT * FROM daemon.unauthorized_fetch('POST', %s, %s::jsonb, %s, %s);",
                                    PQQuoteLiteral(Action).c_str(),
                                    caPayload.c_str(),
                                    PQQuoteLiteral(Agent).c_str(),
                                    PQQuoteLiteral(Host).c_str()
            ));

            AConnection->Data().Values("authorized", "false");
            AConnection->Data().Values("signature", "false");

            try {
                auto pQuery = ExecSQL(SQL, AConnection);
                pQuery->Data().Values(_T("UniqueId"), UniqueId);
                pQuery->Data().Values(_T("Action"), Action);
            } catch (Delphi::Exception::Exception &E) {
                DoError(AConnection, UniqueId, Action, CHTTPReply::service_unavailable, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::AuthorizedFetch(CHTTPServerConnection *AConnection, const CAuthorization &Authorization,
                const CString &UniqueId, const CString &Action, const CString &Payload, const CString &Agent, const CString &Host) {

            CStringList SQL;

            if (Authorization.Schema == CAuthorization::asBearer) {

                const auto &caPayload = Payload.IsEmpty() ? "null" : PQQuoteLiteral(Payload);

                SQL.Add(CString()
                                .MaxFormatSize(256 + Authorization.Token.Size() + Action.Size() + caPayload.Size() + Agent.Size())
                                .Format("SELECT * FROM daemon.fetch(%s, 'POST', %s, %s::jsonb, %s, %s);",
                                        PQQuoteLiteral(Authorization.Token).c_str(),
                                        PQQuoteLiteral(Action).c_str(),
                                        caPayload.c_str(),
                                        PQQuoteLiteral(Agent).c_str(),
                                        PQQuoteLiteral(Host).c_str()
                ));

            } else if (Authorization.Schema == CAuthorization::asBasic) {

                const auto &caPayload = Payload.IsEmpty() ? "null" : PQQuoteLiteral(Payload);

                SQL.Add(CString()
                                .MaxFormatSize(256 + Action.Size() + caPayload.Size() + Agent.Size())
                                .Format("SELECT * FROM daemon.%s_fetch(%s, %s, 'POST', %s, %s::jsonb, %s, %s);",
                                        Authorization.Type == CAuthorization::atSession ? "session" : "authorized",
                                        PQQuoteLiteral(Authorization.Username).c_str(),
                                        PQQuoteLiteral(Authorization.Password).c_str(),
                                        PQQuoteLiteral(Action).c_str(),
                                        caPayload.c_str(),
                                        PQQuoteLiteral(Agent).c_str(),
                                        PQQuoteLiteral(Host).c_str()
                ));

            } else {

                return UnauthorizedFetch(AConnection, UniqueId, Action, Payload, Agent, Host);

            }

            AConnection->Data().Values("authorized", "true");
            AConnection->Data().Values("signature", "false");

            try {
                auto pQuery = ExecSQL(SQL, AConnection);
                pQuery->Data().Values(_T("UniqueId"), UniqueId);
                pQuery->Data().Values(_T("Action"), Action);
            } catch (Delphi::Exception::Exception &E) {
                DoError(AConnection, UniqueId, Action, CHTTPReply::service_unavailable, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::PreSignedFetch(CHTTPServerConnection *AConnection, const CString &UniqueId,
                const CString &Action, const CString &Payload, CSession *ASession) {

            CString sData;

            const auto& caNonce = LongToString(MsEpoch() * 1000);

            sData = Action;
            sData << caNonce;
            sData << (Payload.IsEmpty() ? _T("null") : Payload);

            const auto& caSignature = ASession->Secret().IsEmpty() ? _T("") : hmac_sha256(ASession->Secret(), sData);

            SignedFetch(AConnection, UniqueId, Action, Payload, ASession->Session(), caNonce, caSignature, ASession->Agent(), ASession->IP());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::SignedFetch(CHTTPServerConnection *AConnection, const CString &UniqueId,
                const CString &Action, const CString &Payload, const CString &Session, const CString &Nonce,
                const CString &Signature, const CString &Agent, const CString &Host, long int ReceiveWindow) {

            CStringList SQL;

            const auto &caPayload = Payload.IsEmpty() ? "null" : PQQuoteLiteral(Payload);

            SQL.Add(CString()
                            .MaxFormatSize(256 + Action.Size() + caPayload.Size() + Session.Size() + Nonce.Size() + Signature.Size() + Agent.Size())
                            .Format("SELECT * FROM daemon.signed_fetch('POST', %s, %s::json, %s, %s, %s, %s, %s, INTERVAL '%d milliseconds');",
                                    PQQuoteLiteral(Action).c_str(),
                                    caPayload.c_str(),
                                    PQQuoteLiteral(Session).c_str(),
                                    PQQuoteLiteral(Nonce).c_str(),
                                    PQQuoteLiteral(Signature).c_str(),
                                    PQQuoteLiteral(Agent).c_str(),
                                    PQQuoteLiteral(Host).c_str(),
                                    ReceiveWindow
            ));

            AConnection->Data().Values("authorized", "true");
            AConnection->Data().Values("signature", "true");

            try {
                auto pQuery = ExecSQL(SQL, AConnection);
                pQuery->Data().Values(_T("UniqueId"), UniqueId);
                pQuery->Data().Values(_T("Action"), Action);
            } catch (Delphi::Exception::Exception &E) {
                DoError(AConnection, UniqueId, Action, CHTTPReply::service_unavailable, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoSessionDisconnected(CObject *Sender) {
            auto pConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (pConnection != nullptr) {

                auto pSession = m_SessionManager.FindByConnection(pConnection);
                if (pSession != nullptr) {
                    auto pSocket = pConnection->Socket()->Binding();
                    if (pSocket != nullptr) {
                        Log()->Message(_T("[WebSocketAPI] [%s:%d] Session %s closed connection."),
                                       pSocket->PeerIP(), pSocket->PeerPort(),
                                       pSession->Session().IsEmpty() ? "(empty)" : pSession->Session().c_str()
                        );
                    }

                    if (pSession->UpdateCount() == 0) {
                        delete pSession;
                    }
                } else {
                    auto pSocket = pConnection->Socket()->Binding();
                    if (pSocket != nullptr) {
                        Log()->Message(_T("[WebSocketAPI] [%s:%d] Unknown session closed connection."),
                                       pSocket->PeerIP(), pSocket->PeerPort()
                        );
                    }
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CWebSocketAPI::CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization) {

            const auto &caHeaders = ARequest->Headers;
            const auto &caAuthorization = caHeaders.Values(_T("Authorization"));

            if (caAuthorization.IsEmpty()) {

                const auto &headerSession = caHeaders.Values(_T("Session"));
                const auto &headerSecret = caHeaders.Values(_T("Secret"));

                Authorization.Username = headerSession;
                Authorization.Password = headerSecret;

                if (Authorization.Username.IsEmpty() || Authorization.Password.IsEmpty())
                    return false;

                Authorization.Schema = CAuthorization::asBasic;
                Authorization.Type = CAuthorization::atSession;

            } else {
                Authorization << caAuthorization;
            }

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CWebSocketAPI::CheckTokenAuthorization(CHTTPServerConnection *AConnection, const CString &Session,
                CAuthorization &Authorization) {

            auto pRequest = AConnection->Request();

            try {
                if (CheckAuthorizationData(pRequest, Authorization)) {
                    if (Authorization.Schema == CAuthorization::asBearer) {
                        if (Session != VerifyToken(Authorization.Token))
                            throw Delphi::Exception::Exception(_T("Token for another session."));
                        return true;
                    }
                }

                if (Authorization.Schema == CAuthorization::asBasic)
                    AConnection->Data().Values("Authorization", "Basic");

                ReplyError(AConnection, CHTTPReply::unauthorized, "Unauthorized.");
            } catch (jwt::token_expired_exception &e) {
                ReplyError(AConnection, CHTTPReply::forbidden, e.what());
            } catch (jwt::token_verification_exception &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            } catch (CAuthorizationError &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        int CWebSocketAPI::CheckSessionAuthorization(CSession *ASession) {

            auto pConnection = ASession->Connection();
            auto pRequest = pConnection->Request();
            auto &Authorization = ASession->Authorization();

            try {
                if (CheckAuthorizationData(pRequest, Authorization)) {
                    if (Authorization.Schema == CAuthorization::asBearer) {
                        if (ASession->Session() != VerifyToken(Authorization.Token))
                            throw Delphi::Exception::Exception(_T("Token for another session."));
                    } else {
                        if (ASession->Session() != Authorization.Username)
                            throw Delphi::Exception::Exception(_T("Invalid session header value."));
                    }
                    return 1;
                }
                return -1;
            } catch (jwt::token_expired_exception &e) {
                ReplyError(pConnection, CHTTPReply::forbidden, e.what());
            } catch (jwt::token_verification_exception &e) {
                ReplyError(pConnection, CHTTPReply::bad_request, e.what());
            } catch (CAuthorizationError &e) {
                ReplyError(pConnection, CHTTPReply::bad_request, e.what());
            } catch (std::exception &e) {
                ReplyError(pConnection, CHTTPReply::bad_request, e.what());
            }

            return 0;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoError(const Delphi::Exception::Exception &E) {
            Log()->Error(APP_LOG_ERR, 0, "[WebSocketAPI] Error: %s", E.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoCall(CHTTPServerConnection *AConnection, const CString &Action, const CString &Payload) {
            auto pWSReply = AConnection->WSReply();

            CWSMessage wsmMessage;

            wsmMessage.MessageTypeId = mtCall;
            wsmMessage.UniqueId = GetUID(42).Lower();
            wsmMessage.Action = Action;
            wsmMessage.Payload << Payload;

            CString sResponse;
            CWSProtocol::Response(wsmMessage, sResponse);

            pWSReply->SetPayload(sResponse);
            AConnection->SendWebSocket(true);

            Log()->Message("[WebSocketAPI] [CALL] [%s] [%s] %s", wsmMessage.UniqueId.c_str(), wsmMessage.Action.c_str(), Payload.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoError(CHTTPServerConnection *AConnection, const CString &UniqueId, const CString &Action,
                CHTTPReply::CStatusType Status, const std::exception &e) {

            if (AConnection->ClosedGracefully())
                return;

            auto pWSReply = AConnection->WSReply();

            CWSMessage wsmMessage;

            wsmMessage.MessageTypeId = mtCallError;

            wsmMessage.UniqueId = UniqueId.IsEmpty() ? GetUID(42).Lower() : UniqueId;
            wsmMessage.Action = Action.IsEmpty() ? _T("/error") : Action;

            wsmMessage.ErrorCode = Status;
            wsmMessage.ErrorMessage = e.what();

            CString sResponse;
            CWSProtocol::Response(wsmMessage, sResponse);

            pWSReply->SetPayload(sResponse);
            AConnection->SendWebSocket(true);

            Log()->Error(APP_LOG_ERR, 0, "[WebSocketAPI] [ERROR] [%s] [%s] [%d] %s", wsmMessage.UniqueId.c_str(), wsmMessage.Action.c_str(), wsmMessage.ErrorCode, e.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoWS(CHTTPServerConnection *AConnection, const CString &Action) {

            auto pReply = AConnection->Reply();

            pReply->ContentType = CHTTPReply::json;

            try {
                if (Action == "list") {
                    CJSONValue jsonArray(jvtArray);

                    for (int i = 0; i < m_SessionManager.Count(); i++) {
                        CJSONValue jsonSession(jvtObject);
                        CJSONValue jsonConnection(jvtObject);

                        auto pSession = m_SessionManager[i];

                        jsonSession.Object().AddPair("session", pSession->Session());
                        jsonSession.Object().AddPair("identity", pSession->Identity());
                        jsonSession.Object().AddPair("authorized", pSession->Authorized());

                        if (pSession->Connection() != nullptr && !pSession->Connection()->ClosedGracefully()) {
                            jsonConnection.Object().AddPair("socket", pSession->Connection()->Socket()->Binding()->Handle());
                            jsonConnection.Object().AddPair("host", pSession->Connection()->Socket()->Binding()->PeerIP());
                            jsonConnection.Object().AddPair("port", pSession->Connection()->Socket()->Binding()->PeerPort());

                            jsonSession.Object().AddPair("connection", jsonConnection);
                        } else {
                            jsonSession.Object().AddPair("connection", CJSONValue());
                        }

                        jsonArray.Array().Add(jsonSession);
                    }

                    pReply->Content = jsonArray.ToString();

                    AConnection->SendReply(CHTTPReply::ok);
                } else {
                    AConnection->SendStockReply(CHTTPReply::not_found);
                }
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoPost(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            pReply->ContentType = CHTTPReply::json;

            CStringList slRouts;
            SplitColumns(pRequest->Location.pathname, slRouts, '/');

            if (slRouts.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            CSession *pSession;
            bool bSent = false;

            const auto& caSession = slRouts[1];
            const auto& caIdentity = slRouts.Count() == 3 ? slRouts[2] : CString();

            CAuthorization Authorization;
            if (CheckTokenAuthorization(AConnection, caSession, Authorization)) {
                for (int i = 0; i < m_SessionManager.Count(); ++i) {
                    pSession = m_SessionManager[i];
                    if ((pSession->Session() == caSession) && (caIdentity.IsEmpty() ? true : pSession->Identity() == caIdentity) && pSession->Authorized()) {
                        DoCall(pSession->Connection(), "/ws", pRequest->Content);
                        bSent = true;
                    }
                }

                pReply->Content.Clear();

                if (bSent)
                    pReply->Content = R"({"sent": true, "status": "Success"})";
                else
                    pReply->Content = R"({"sent": false, "status": "Session not found"})";

                AConnection->SendReply(CHTTPReply::ok, nullptr, true);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoGet(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            pReply->ContentType = CHTTPReply::html;

            CStringList slRouts;
            SplitColumns(pRequest->Location.pathname, slRouts, '/');

            if (slRouts.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            if (slRouts[0] == _T("ws")) {
                DoWS(AConnection, slRouts[1]);
                return;
            }

            if (slRouts[0] != _T("session")) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& caSession = slRouts[1];
            const auto& caIdentity = slRouts.Count() == 3 ? slRouts[2] : _T("main");

            const auto& caSecWebSocketKey = pRequest->Headers.Values(_T("Sec-WebSocket-Key"));
            const auto& caSecWebSocketProtocol = pRequest->Headers.Values(_T("Sec-WebSocket-Protocol"));

            if (caSecWebSocketKey.IsEmpty()) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            const CString csAccept(SHA1(caSecWebSocketKey + _T("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
            const CString csProtocol(caSecWebSocketProtocol.IsEmpty() ? "" : caSecWebSocketProtocol.SubString(0, caSecWebSocketProtocol.Find(',')));

            auto pSession = m_SessionManager.Find(caSession, caIdentity);

            if (pSession == nullptr) {
                pSession = m_SessionManager.Add(AConnection);
                pSession->Session() = caSession;
                pSession->Identity() = caIdentity;
            } else {
                pSession->SwitchConnection(AConnection);
            }

            pSession->IP() = GetRealIP(AConnection);
            pSession->Agent() = GetUserAgent(AConnection);

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            AConnection->OnDisconnected([this](auto && Sender) { DoSessionDisconnected(Sender); });
#else
            AConnection->OnDisconnected(std::bind(&CWebSocketAPI::DoSessionDisconnected, this, _1));
#endif

            const auto checkAuth = CheckSessionAuthorization(pSession);
            if (checkAuth == 1) {
                pSession->Authorized(true);
            } else if (checkAuth == 0) {
                AConnection->Disconnect();
                return;
            }

            AConnection->SwitchingProtocols(csAccept, csProtocol);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::DoWebSocket(CHTTPServerConnection *AConnection) {

            auto pWSRequest = AConnection->WSRequest();
            const CString csRequest(pWSRequest->Payload());

            try {
                if (!AConnection->Connected())
                    return;

                CWSMessage wsmRequest;
                CWSMessage wsmResponse;

                auto pSession = CSession::FindOfConnection(AConnection);

                try {
                    CWSProtocol::Request(csRequest, wsmRequest);

                    if (wsmRequest.MessageTypeId == mtOpen) {
                        if (wsmRequest.Payload.HasOwnProperty(_T("secret"))) {
                            wsmRequest.Action = _T("/api/v1/authenticate");

                            const auto &session = wsmRequest.Payload[_T("session")].AsString();
                            const auto &secret = wsmRequest.Payload[_T("secret")].AsString();

                            if (!session.IsEmpty() && session != pSession->Session()) // check session value
                                throw Delphi::Exception::Exception(_T("Invalid session value."));

                            if (secret.IsEmpty())
                                throw Delphi::Exception::Exception(_T("Secret cannot be empty."));

                            pSession->Secret() = secret;
                            pSession->Authorization().Clear();
                            pSession->Authorized(false);

                            wsmRequest.Payload.Clear();
                            wsmRequest.Payload.Object().AddPair(_T("session"), pSession->Session());
                            wsmRequest.Payload.Object().AddPair(_T("secret"), pSession->Secret());
                            wsmRequest.Payload.Object().AddPair(_T("agent"), pSession->Agent());
                            wsmRequest.Payload.Object().AddPair(_T("host"), pSession->IP());

                        } else if (wsmRequest.Payload.HasOwnProperty(_T("token"))) {
                            wsmRequest.Action = _T("/api/v1/authorize");

                            const auto& token = wsmRequest.Payload[_T("token")].AsString();

                            if (pSession->Session() != VerifyToken(token))
                                throw Delphi::Exception::Exception(_T("Token for another session."));

                            pSession->Secret().Clear();
                            pSession->Authorization() << _T("Bearer ") + token;
                            pSession->Authorized(false);

                            wsmRequest.Payload.Clear();
                            wsmRequest.Payload.Object().AddPair(_T("session"), pSession->Session());
                            wsmRequest.Payload.Object().AddPair(_T("agent"), pSession->Agent());
                            wsmRequest.Payload.Object().AddPair(_T("host"), pSession->IP());
                        } else {
                            throw Delphi::Exception::Exception(_T("Bad request."));
                        }

                        wsmRequest.MessageTypeId = mtCall;
                        UnauthorizedFetch(AConnection, wsmRequest.UniqueId, wsmRequest.Action, wsmRequest.Payload.ToString(), pSession->Agent(), pSession->IP());

                        return;
                    } else if (wsmRequest.MessageTypeId == mtClose) {
                        wsmRequest.Action = _T("/api/v1/sign/out");
                        wsmRequest.MessageTypeId = mtCall;
                    }

                    if (wsmRequest.MessageTypeId == mtCall) {
                        const auto& caAuthorization = pSession->Authorization();

                        if (caAuthorization.Schema == CAuthorization::asBasic && caAuthorization.Username != pSession->Session()) {
                            throw Delphi::Exception::Exception(_T("Invalid session header value."));
                        }

                        if (!pSession->Authorized())
                            throw CAuthorizationError(_T("Unauthorized."));

                        if (wsmRequest.Action.SubString(0, 8) != _T("/api/v1/"))
                            wsmRequest.Action = _T("/api/v1") + wsmRequest.Action;

                        if (caAuthorization.Schema != CAuthorization::asUnknown) {
                            AuthorizedFetch(AConnection, caAuthorization, wsmRequest.UniqueId, wsmRequest.Action, wsmRequest.Payload.ToString(), pSession->Agent(), pSession->IP());
                        } else {
                            PreSignedFetch(AConnection, wsmRequest.UniqueId, wsmRequest.Action, wsmRequest.Payload.ToString(), pSession);
                        }
                    }
                } catch (jwt::token_expired_exception &e) {
                    DoError(AConnection, wsmRequest.UniqueId, wsmRequest.Action, CHTTPReply::forbidden, e);
                } catch (CAuthorizationError &e) {
                    DoError(AConnection, wsmRequest.UniqueId, wsmRequest.Action, CHTTPReply::unauthorized, e);
                } catch (std::exception &e) {
                    DoError(AConnection, wsmRequest.UniqueId, wsmRequest.Action, CHTTPReply::bad_request, e);
                }
            } catch (std::exception &e) {
                AConnection->SendWebSocketClose();
                AConnection->CloseConnection(true);

                Log()->Error(APP_LOG_ERR, 0, "[WebSocketAPI] %s", e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::Initialization(CModuleProcess *AProcess) {
            CApostolModule::Initialization(AProcess);
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CWebSocketAPI::Execute(CHTTPServerConnection *AConnection) {
            if (AConnection->Protocol() == pWebSocket) {
#ifdef _DEBUG
                WSDebugConnection(AConnection);
#endif
                DoWebSocket(AConnection);

                return true;
            } else {
                return CApostolModule::Execute(AConnection);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CString CWebSocketAPI::VerifyToken(const CString &Token) {

            auto decoded = jwt::decode(Token);

            const auto& aud = CString(decoded.get_audience());
            const auto& alg = CString(decoded.get_algorithm());
            const auto& iss = CString(decoded.get_issuer());

            const auto& Providers = Server().Providers();

            CString Application;
            const auto Index = OAuth2::Helper::ProviderByClientId(Providers, aud, Application);
            if (Index == -1)
                throw COAuth2Error(_T("Not found provider by Client ID."));

            const auto& Provider = Providers[Index].Value();
            const auto& Secret = OAuth2::Helper::GetSecret(Provider, Application);

            CStringList Issuers;
            Provider.GetIssuers(Application, Issuers);
            if (Issuers[iss].IsEmpty())
                throw jwt::token_verification_exception("Token doesn't contain the required issuer.");

            if (alg == "HS256") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{Secret});
                verifier.verify(decoded);
            } else if (alg == "HS384") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs384{Secret});
                verifier.verify(decoded);
            } else if (alg == "HS512") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs512{Secret});
                verifier.verify(decoded);
            }

            return decoded.get_payload_claim("sub").as_string();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::Observer(CSession *ASession, const CString &Publisher, const CString &Data) {

            auto OnExecuted = [ASession](CPQPollQuery *APollQuery) {

                if (ASession == nullptr)
                    return;

                auto pConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->Binding());

                if (pConnection == nullptr)
                    return;

                if (pConnection->ClosedGracefully())
                    return;

                CHTTPReply::CStatusType status = CHTTPReply::internal_server_error;

                try {
                    auto pResult = APollQuery->Results(0);

                    if (pResult->ExecStatus() != PGRES_TUPLES_OK) {
                        throw Delphi::Exception::EDBError(pResult->GetErrorMessage());
                    }

                    if (pResult->nTuples() == 1) {
                        const CJSON Payload(pResult->GetValue(0, 0));
                        CString errorMessage;

                        status = ErrorCodeToStatus(CheckError(Payload, errorMessage));
                        if (status == CHTTPReply::unauthorized) {
                            ASession->Session().Clear();
                            ASession->Secret().Clear();
                            ASession->Authorization().Clear();
                            ASession->Authorized(false);
                        }

                        if (status != CHTTPReply::ok) {
                            throw Delphi::Exception::EDBError(errorMessage.c_str());
                        }
                    }

                    if (pResult->nTuples() != 0) {
                        const auto& publisher = APollQuery->Data()["publisher"];
                        CString jsonString;
                        PQResultToJson(pResult, jsonString);
                        DoCall(pConnection, "/" + publisher, jsonString);
                    }
                } catch (Delphi::Exception::Exception &E) {
                    DoError(pConnection, CString(), CString(), status, E);
                }
            };

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->Binding());
                if (pConnection != nullptr && !pConnection->ClosedGracefully())
                    DoError(pConnection, CString(), CString(), CHTTPReply::service_unavailable, E);
            };

            if (ASession->Authorized()) {

                CStringList SQL;

                SQL.Add(CString().Format("SELECT * FROM daemon.observer('%s', '%s', %s, %s::jsonb, %s, %s);",
                                         Publisher.c_str(),
                                         ASession->Session().c_str(),
                                         PQQuoteLiteral(ASession->Identity()).c_str(),
                                         PQQuoteLiteral(Data).c_str(),
                                         PQQuoteLiteral(ASession->Agent()).c_str(),
                                         PQQuoteLiteral(ASession->IP()).c_str()
                ));

                try {
                    auto pQuery = ExecSQL(SQL, ASession->Connection(), OnExecuted, OnException);
                    pQuery->Data().Values(_T("publisher"), Publisher);
                } catch (Delphi::Exception::Exception &E) {
                    DoError(E);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::InitListen() {

            auto OnExecuted = [this](CPQPollQuery *APollQuery) {
                try {
                    auto pResult = APollQuery->Results(0);

                    if (pResult->ExecStatus() != PGRES_TUPLES_OK) {
                        throw Delphi::Exception::EDBError(pResult->GetErrorMessage());
                    }

                    APollQuery->Connection()->Listener(true);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                    APollQuery->Connection()->OnNotify([this](auto && APollQuery, auto && ANotify) { DoPostgresNotify(APollQuery, ANotify); });
#else
                    APollQuery->Connection()->OnNotify(std::bind(&CWebSocketAPI::DoPostgresNotify, this, _1, _2));
#endif
                } catch (Delphi::Exception::Exception &E) {
                    DoError(E);
                }
            };

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                DoError(E);
            };

            CStringList SQL;

            SQL.Add("SELECT daemon.init_listen();");

            try {
                ExecSQL(SQL, nullptr, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                DoError(E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::CheckListen() {
            int Index = 0;
            while (Index < PQServer().PollManager()->Count() && !PQServer().Connections(Index)->Listener())
                Index++;

            if (Index == PQServer().PollManager()->Count())
                InitListen();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CWebSocketAPI::Heartbeat() {
            CApostolModule::Heartbeat();
            const auto now = Now();
            if ((now >= m_CheckDate)) {
                m_CheckDate = now + (CDateTime) 5 / MinsPerDay; // 5 min
                CheckListen();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CWebSocketAPI::Enabled() {
            if (m_ModuleStatus == msUnknown)
                m_ModuleStatus = Config()->IniFile().ReadBool("worker/WebSocketAPI", "enable", true) ? msEnabled : msDisabled;
            return m_ModuleStatus == msEnabled;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CWebSocketAPI::CheckLocation(const CLocation &Location) {
            return Location.pathname.SubString(0, 9) == _T("/session/") || Location.pathname.SubString(0, 4) == _T("/ws/");
        }
        //--------------------------------------------------------------------------------------------------------------
    }
}
}