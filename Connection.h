#pragma once
#include <functional>
#include "OverlayProtocol.h"
#include <MBTCP/MBTCP.h>

#include <deque>

namespace MBChat2
{
    struct ConnectionParameters
    {
        Key PeerID;
        uint32_t IP = 0;
        uint16_t PeerPort = 0;
        uint16_t LocalPort = 0;
    };
    class Connection
    {
        typedef std::function<void(Message const&)> MessageCallback;


        struct State
        {
            ConnectionParameters Peer;

            MBTCP::MBTCP Transport;
            std::atomic<bool> Stopping;
            std::thread ReadThread;
            std::thread SendThread;
            std::atomic<uint64_t> NextSendID = 1;

            //Not changed after they have been set by the constructor
            std::function<void(ConnectionParameters const&)> CloseHandler;
            MessageCallback GenericMessageHandler;

            std::mutex SendMutex;
            std::condition_variable SendConditional;
            std::unordered_map<uint64_t,MessageCallback> MessageCallbacks;
            std::deque<std::pair<Message,MessageCallback>> QueuedMessages;
        };

        static bool p_VerifyAuthenticity(State const& SharedState,Message const& NewMessage);
        static void p_ReadThread(std::shared_ptr<State> State);
        static void p_SendThread(std::shared_ptr<State> State);

        std::shared_ptr<State> m_SharedState;
    public:
        ~Connection();

        Connection(ConnectionParameters Peer,MessageCallback MessageHandler,std::function<void(ConnectionParameters const&)> QuitHandler);

        void SendMessage(Message MessageToSend,MessageCallback Callback);
        //handles messages not sent from a response, such as peer requets and notifications
        //void SetMessageHandler(MessageCallback Callback);
        //void AddOnCloseHandler(std::function<void(PeerParameters const&)> Callback);
    };

    

    class ConnectionManager
    {
           
    };
}
