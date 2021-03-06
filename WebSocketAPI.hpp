/*++

Program name:

  Apostol Web Service

Module Name:

  WebSocketAPI.hpp

Notices:

  Module: Web Socket API

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_WEBSOCKETAPI_HPP
#define APOSTOL_WEBSOCKETAPI_HPP
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Workers {

        //--------------------------------------------------------------------------------------------------------------

        //-- CWebSocketAPI -----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CWebSocketAPI: public CApostolModule {
        private:

            CDateTime m_CheckDate;

            CSessionManager m_SessionManager;

            void InitListen();
            void CheckListen();

            void Observer(CSession *ASession, const CString &Publisher, const CString &Data);

            void InitMethods() override;

            static void AfterQuery(CHTTPServerConnection *AConnection, const CString &Path, const CJSON &Payload);

            static void QueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E);

            static bool CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization);

            static int CheckError(const CJSON &Json, CString &ErrorMessage, bool RaiseIfError = false);
            static CHTTPReply::CStatusType ErrorCodeToStatus(int ErrorCode);

        protected:

            static void DoError(const Delphi::Exception::Exception &E);

            static void DoCall(CHTTPServerConnection *AConnection, const CString &Action, const CString &Payload);
            static void DoError(CHTTPServerConnection *AConnection, const CString &UniqueId, const CString &Action,
                CHTTPReply::CStatusType Status, const std::exception &e);

            void DoGet(CHTTPServerConnection *AConnection) override;
            void DoPost(CHTTPServerConnection *AConnection);
            void DoWS(CHTTPServerConnection *AConnection, const CString &Action);

            void DoWebSocket(CHTTPServerConnection *AConnection);
            void DoSessionDisconnected(CObject *Sender);

            void DoPostgresNotify(CPQConnection *AConnection, PGnotify *ANotify) override;

            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) override;

        public:

            explicit CWebSocketAPI(CModuleProcess *AProcess);

            ~CWebSocketAPI() override = default;

            static class CWebSocketAPI *CreateModule(CModuleProcess *AProcess) {
                return new CWebSocketAPI(AProcess);
            }

            bool CheckTokenAuthorization(CHTTPServerConnection *AConnection, const CString &Session, CAuthorization &Authorization);
            int CheckSessionAuthorization(CSession *ASession);

            CString VerifyToken(const CString &Token);

            void UnauthorizedFetch(CHTTPServerConnection *AConnection, const CString &UniqueId, const CString &Action,
                const CString &Payload, const CString &Agent, const CString &Host);

            void AuthorizedFetch(CHTTPServerConnection *AConnection, const CAuthorization &Authorization, const CString &UniqueId,
                const CString &Action, const CString &Payload, const CString &Agent, const CString &Host);

            void PreSignedFetch(CHTTPServerConnection *AConnection, const CString &UniqueId, const CString &Action,
                const CString &Payload, CSession *ASession);

            void SignedFetch(CHTTPServerConnection *AConnection, const CString &UniqueId, const CString &Action,
                const CString &Payload, const CString &Session, const CString &Nonce, const CString &Signature,
                const CString &Agent, const CString &Host, long int ReceiveWindow = 5000);

            void Initialization(CModuleProcess *AProcess) override;

            bool Execute(CHTTPServerConnection *AConnection) override;

            void Heartbeat() override;

            bool Enabled() override;

            bool CheckLocation(const CLocation &Location) override;

        };
    }
}

using namespace Apostol::Workers;
}
#endif //APOSTOL_WEBSOCKETAPI_HPP
