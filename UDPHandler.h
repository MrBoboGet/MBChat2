#pragma  once
#include <cstdint>
#include <thread>
#include <mutex>
#include <functional>

#include <type_traits>
#include <future>

#include <MBUtility/StaticVariant.h>
#include <MBUtility/Future.h>
#include <MBUtility/Optional.h>
#include <MrBoboSockets/MrBoboSockets.h>
#include <MBUtility/ThreadPool.h>
#include <MBUtility/TemplateUtilities.h>

#include "OverlayProtocol.h"
namespace MBChat2
{

    enum class UDPMessageType
    {
        Notification,
        Request,
        Response,
    };


    struct UDPNotification_ {
        uint32_t ID = 0;
    };
    struct UDPResponse_ {
        uint32_t ID = 0;
    };
    struct UDPRequest_ {
        uint32_t ID = 0;
    };

    template<uint8_t TypeID>
    struct Notification : public UDPNotification_
    {
        static constexpr uint8_t type = TypeID;
    };
    template<uint8_t TypeID>
    struct Response : public UDPResponse_
    {
        static constexpr uint8_t type = TypeID;
    };
    template<uint8_t TypeID,typename Response>
    struct Request : public UDPRequest_
    {
        static constexpr uint8_t type = TypeID;
        typedef Response ResponseType;
    };

    struct NoResponse : public Response<0>
    {
        template<typename T>
        friend void Parse(T& Stream,NoResponse& Value)
        {
            Stream & type;
        }
    };

    struct FindPeer_Response : public Response<1>
    {
        Hash HostID;
        bool InDb = false;
        std::vector<PeerInfo> Peers;
        template<typename T>
        friend void Parse(T& Stream,FindPeer_Response& Value)
        {
            Stream & type;
            Parse(Stream,Value.HostID);
            Stream & Value.InDb;
            Stream & Value.Peers;
        }
    };
    struct FindPeerRequest : public Request<1,FindPeer_Response>
    {
        PeerInfo HostPeer;
        Hash PeerID;
        MBUtility::Optional<Hash> DBID;
        int k = 20;
        template<typename T>
        friend void Parse(T& Stream,FindPeerRequest& Value)
        {
            Stream & type;
            Parse(Stream,Value.HostPeer);
            Parse(Stream,Value.PeerID);
        }
    };
    struct GetPeerKey_Response : public Response<2>
    {
        Key PeerKey;
        template<typename T>
        friend void Parse(T& Stream,GetPeerKey_Response& Value)
        {
            Stream & type;
            Parse(Stream,Value.PeerKey,0);
        }
    };
    struct GetPeerKey : public Request<2,GetPeerKey_Response>
    {
        Hash PeerID;
        template<typename T>
        friend void Parse(T& Stream,GetPeerKey& Value)
        {
            Stream & type;
            Parse(Stream,Value.PeerID);
        }
    };
    struct GetResourceHeader_Response : public Response<3>
    {
        MBUtility::Optional<ResourceHeader> Header;
        std::vector<PeerInfo> CloserPeers;
        template<typename T>
        friend void Parse(T& Stream,GetResourceHeader_Response& Value)
        {
            Stream & type;
            Parse(Stream,Value.Header);
            Parse(Stream,Value.CloserPeers);
        }
    };
    struct GetResourceHeader : public Request<3,GetResourceHeader_Response>
    {
        Hash ResourceID;
        Hash DatabaseID;
        template<typename T>
        friend void Parse(T& Stream,GetResourceHeader& Value)
        {
            Stream & type;
            Parse(Stream,Value.ResourceID);
            Parse(Stream,Value.DatabaseID);
        }
    };
    struct GetResourceContent_Response : public Response<4>
    {
        MBUtility::Optional<std::string> Content;
        std::vector<PeerInfo> CloserPeers;
        template<typename T>
        friend void Parse(T& Stream,GetResourceContent_Response& Value)
        {
            Stream & type;
            Stream & Value.Content;
            Stream & Value.CloserPeers;
        }
    };
    struct GetResourceContent : public Request<4,GetResourceContent_Response>
    {
        Hash ResourceID;
        Hash DatabaseID;
        template<typename T>
        friend void Parse(T& Stream,GetResourceContent& Value)
        {
            Stream & type;
            Parse(Stream,Value.ResourceID);
            Parse(Stream,Value.DatabaseID);
        }
    };
    struct InitConnection_Response : public Response<5>
    {
        bool Accepted = false;
        uint16_t HostPort = -1;
        template<typename T>
        friend void Parse(T& Stream,InitConnection_Response& Value)
        {
            Stream & type;
            Stream & Value.Accepted;
            Stream & Value.HostPort;
        }
    };
    struct InitConnection : public Request<5,GetResourceContent_Response>
    {
        PeerInfo HostInfo;
        uint16_t HostPort = 0;
        template<typename T>
        friend void Parse(T& Stream,InitConnection& Value)
        {
            Stream & type;
            Parse(Stream,Value.HostInfo);
            Stream & Value.HostPort;
        }
    };

    //
    struct Ack : public Response<6>
    {
           
        template<typename T>
        friend void Parse(T& Stream,Ack& Value)
        {
            Stream & Value.type;
        }
    };
    //Notifications
    struct NewMessage : public Notification<1>
    {
        ResourceHeader Header;
        //included if small enough to fit within a UDP packet
        std::string Content;
        template<typename T>
        friend void Parse(T& Stream,NewMessage& Value)
        {
            Parse(Stream,Value.Header);
            Stream & Value.Content;
        }
    };
    struct DBSubscriber : public Notification<2>
    {
        PeerInfo Peer;
        std::vector<Hash> SubscribedDatabases;
        template<typename T>
        friend void Parse(T& Stream,DBSubscriber& Value)
        {
            Parse(Stream,Value.Peer);
            Stream & Value.SubscribedDatabases;
        }
    };


    //UDP message header:
    //1 byte type, second byte type

    struct MessageLocation
    {
        uint32_t IP = 0;
        uint32_t Port = 0;
    };


    typedef MBUtility::StaticVariant<NewMessage,DBSubscriber> UDPNotification;
    typedef MBUtility::StaticVariant<FindPeerRequest,GetPeerKey,InitConnection,GetResourceContent> UDPRequest;
    typedef MBUtility::StaticVariant<NoResponse,FindPeer_Response,GetPeerKey_Response,GetResourceContent_Response,InitConnection_Response,Ack> UDPResponse;


    template<typename StreamType,typename ValueType,typename Head,typename... Tail>
    void ParseVariant(StreamType& Stream,ValueType& Value,uint8_t ID,MBUtility::TypeList<Head,Tail...>)
    {
        if(Head::type == ID)
        {
            Parse(Stream,Value.template GetOrAssign<Head>());
        }
        else if constexpr(sizeof...(Tail) > 0)
        {
            ParseVariant(Stream,Value,ID,MBUtility::TypeList<Tail...>());
        }
        else
        {
            throw std::runtime_error("No valid type matching ID "+std::to_string(ID));   
        }
    }
    ///template<typename StreamType,typename ValueType,typename... Args>
    ///void ParseVariant(StreamType& Stream,ValueType& Value,uint8_t ID,MBUtility::TypeList<Args...>())
    ///{
    ///    throw std::runtime_error("No valid type matching ID "+std::to_string(ID));
    ///}

    template<typename StreamType,typename... Args>
    void ParseContent(StreamType& Stream,MBUtility::StaticVariant<Args...>& Value)
    {
        uint8_t Type;
        Stream & Type;
        ParseVariant(Stream,Value,Type,MBUtility::TypeList<Args...>());
    }

    typedef std::function<void (MessageLocation,UDPNotification const&)> UDPNotificationHandler;
    typedef std::function<UDPResponse (UDPRequest const&)> UDPRequestHandler;
    class UDPHandler
    {
        std::thread m_ListenThread;

        std::mutex m_StateMutex;

        struct StoredMessage
        {
            uint32_t ID = 0;
            std::chrono::steady_clock::time_point RetryTime;
            int AttemptedRetries = 0;
            int MaxRetries = 3;

            //Resend info
            uint32_t IP = 0;
            uint16_t Port = 0;
            std::string SerializedContent;

            bool operator<(StoredMessage const& rhs) const
            {
                return ID < rhs.ID;
            }
            bool operator>(StoredMessage const& rhs) const
            {
                return ID > rhs.ID;
            }
        };
        struct StoredCallback
        {
            uint32_t IP;
            MBUtility::MOFunction<void (UDPResponse const& Response)> Callback;
        };

        std::vector<StoredMessage> m_SentMessages;
        std::atomic<bool> m_Stopping = false;
        std::atomic<uint32_t> m_NextMessageID;
        std::unordered_map<uint32_t,StoredCallback> m_ResponseCallbacks;
        MBSockets::UDPSocket m_Socket;
        MBUtility::ThreadPool m_ThreadPool = MBUtility::ThreadPool(1);

        UDPNotificationHandler m_NotificationHandler;
        UDPRequestHandler m_RequestHandler;
        void p_SendMessage(StoredMessage const& MessageToSend);
        void p_SendResponse(uint32_t IP,uint16_t Port,uint32_t ID,std::string const& Content);
        void p_ReadThread();
        //inefficient, but hashing tuples is 
        //std::unordered_map<uint32_t,std::uno
    public:

        template<typename MessageType>
        std::enable_if_t<std::is_base_of_v<UDPNotification_,MessageType>> SendNotification(PeerInfo Peer,MessageType Message,float Timeout = 5)
        {
            StoredMessage NewMessage;
            NewMessage.ID = m_NextMessageID.fetch_add(1);
            MBUtility::MBStringOutputStream OutStream(NewMessage.SerializedContent);
            OutStream & UDPMessageType::Notification;
            OutStream & NewMessage.ID;
            Parse(OutStream,Message);
            NewMessage.IP = Peer.IP;
            NewMessage.Port = Peer.ListeningPort;
            std::lock_guard Lock(m_StateMutex);
            m_ResponseCallbacks[NewMessage.ID].IP = Peer.IP;
            m_ResponseCallbacks[NewMessage.ID].Callback = [](UDPResponse const& Response){};
            NewMessage.RetryTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            p_SendMessage(NewMessage);
            m_SentMessages.push_back(std::move(NewMessage));
        }
        template<typename MessageType>
        std::enable_if_t<std::is_base_of_v<UDPRequest_,MessageType>,MBUtility::Future<typename MessageType::ResponseType>> SendRequest(PeerInfo const& Peer,MessageType Message,float Timeout=5)
        {
            typedef typename MessageType::ResponseType ResponseType;
            StoredMessage NewMessage;
            NewMessage.ID = m_NextMessageID.fetch_add(1);
            MBUtility::MBStringOutputStream OutStream(NewMessage.SerializedContent);
            OutStream & UDPMessageType::Request;
            OutStream & NewMessage.ID;
            Parse(OutStream,Message);
            NewMessage.IP = Peer.IP;
            NewMessage.Port = Peer.ListeningPort;
            MBUtility::Promise<ResponseType> Promise;
            MBUtility::Future<ResponseType> ReturnValue;
            {
                std::lock_guard Lock(m_StateMutex);
                m_ResponseCallbacks[NewMessage.ID].IP = Peer.IP;
                m_ResponseCallbacks[NewMessage.ID].Callback = [Promise=std::move(Promise)] (UDPResponse const& Response) mutable{
                    if(!Response.IsType<ResponseType>())
                    {
                        Promise.SetInvalid();
                    }
                    else
                    {
                        Promise.SetValue(std::move(Response.GetType<ResponseType>()));
                    }
                };
                p_SendMessage(NewMessage);
                m_SentMessages.push_back(std::move(NewMessage));
                   
            }
            return ReturnValue;
        }

        void SetNotificationHandler(UDPNotificationHandler Handler);
        void SetRequestHandler(UDPRequestHandler Handler);
    };
}
