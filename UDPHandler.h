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

#include <coroutine>
#include <optional>
#include <deque>


namespace MBChat2
{

    enum class UDPMessageType
    {
        Notification,
        Request,
        Response,
        TCPData,
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
            Stream & Value.Header;
            Stream & Value.CloserPeers;
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
            Stream & Value.ResourceID;
            Stream & Value.DatabaseID;
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
    struct InitConnection : public Request<5,InitConnection_Response>
    {
        PeerInfo HostInfo;
        uint16_t HostPort = 0;
        uint64_t ConnectionID = 0;
        template<typename T>
        friend void Parse(T& Stream,InitConnection& Value)
        {
            Stream & type;
            Parse(Stream,Value.HostInfo);
            Stream & Value.HostPort;
            Stream & Value.ConnectionID;
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
    typedef MBUtility::StaticVariant<FindPeerRequest,GetPeerKey,InitConnection,GetResourceHeader,GetResourceContent> UDPRequest;
    typedef MBUtility::StaticVariant<NoResponse,FindPeer_Response,GetPeerKey_Response,GetResourceHeader_Response,GetResourceContent_Response,InitConnection_Response,Ack> UDPResponse;


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
    void ReadVariant(StreamType& Stream,MBUtility::StaticVariant<Args...>& Value)
    {
        uint8_t Type;
        Stream & Type;
        ParseVariant(Stream,Value,Type,MBUtility::TypeList<Args...>());
    }
    template<typename StreamType,typename... Args>
    void WriteVariant(StreamType& Stream,uint8_t Type,MBUtility::StaticVariant<Args...>& Value)
    {
        Stream & Type;
        ParseVariant(Stream,Value,Type,MBUtility::TypeList<Args...>());
    }


    template<typename T>
    class UDPTaskQueue;

    typedef std::function<void (MessageLocation,UDPNotification const&)> UDPNotificationHandler;
    typedef std::function<UDPResponse (MessageLocation PeerLocation,UDPRequest const&)> UDPRequestHandler;
    class UDPHandler
    {
        template<typename T>
        friend class UDPTaskQueue;

        std::thread m_ListenThread;

        std::mutex m_StateMutex;
        template<typename T>
        class UDPTask
        {
            friend UDPHandler;
            template<typename V>
            friend class UDPTaskQueue;
            uint32_t m_ID = 0;
            struct SharedState
            {
                std::mutex StateMutex;
                std::atomic<bool> Done{false};
                MBUtility::Future<std::optional<std::pair<PeerInfo,T>>> Result;
                std::coroutine_handle<> Continuation;
            };
            UDPHandler& m_AssociatedHandler;
            std::shared_ptr<SharedState> m_SharedState = std::make_shared<SharedState>();

            UDPTask(UDPHandler& Handler) : m_AssociatedHandler(Handler)
            {
                   
            }
        public:
            bool await_ready()
            {
                return m_SharedState->Done.load();
            }
            bool await_suspend(std::coroutine_handle<> Handle)
            {
                std::lock_guard Lock(m_SharedState->StateMutex);
                if(m_SharedState->Done.load())
                {
                    return false;
                }
                m_SharedState->Continuation = Handle;
                return true;
            }
            std::optional<std::pair<PeerInfo,T>> await_resume()
            {
                std::lock_guard Lock(m_SharedState->StateMutex);
                return std::move(m_SharedState->Result.Get());
            }
        };

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
            MBUtility::MOFunction<void (UDPResponse const& Response)> Callback;
        };
        struct TCPListener {
            uint32_t ID = 0;
            uint32_t ClientIP = 0;
            MBUtility::MOFunction<void(std::string_view)> PacketRecievedCallback;
        };

        MBUtility::ThreadPool m_RequestResponseRunner = MBUtility::ThreadPool(5);

        std::vector<StoredMessage> m_SentMessages;
        std::atomic<bool> m_Stopping = false;
        std::atomic<uint32_t> m_NextMessageID;
        std::unordered_map<uint32_t,std::unordered_map<uint32_t,StoredCallback>> m_ResponseCallbacks;
        std::unordered_map<uint32_t,std::unordered_map<uint32_t,TCPListener>> m_ActivePacketListeners;
        MBSockets::UDPSocket m_Socket;
        MBUtility::ThreadPool m_ThreadPool = MBUtility::ThreadPool(5);

        UDPNotificationHandler m_NotificationHandler;
        UDPRequestHandler m_RequestHandler;
        void p_SendMessage(StoredMessage const& MessageToSend);
        void p_SendResponse(uint32_t IP,uint16_t Port,uint32_t ID,std::string const& Content);
        void p_ReadThread();
        //inefficient, but hashing tuples is 
        //std::unordered_map<uint32_t,std::uno
    public:

        UDPHandler(uint16_t ListenPort,UDPRequestHandler RequestHandler,UDPNotificationHandler NotificationHandler);
        UDPHandler();
        
        template<typename MessageType>
        std::enable_if_t<std::is_base_of_v<UDPNotification_,MessageType>> 
            SendNotification(PeerInfo Peer,MessageType Message,float Timeout = 5)
        {
            StoredMessage NewMessage;
            NewMessage.ID = m_NextMessageID.fetch_add(1);
            MBUtility::MBStringOutputStream OutStream(NewMessage.SerializedContent);
            OutStream & UDPMessageType::Notification;
            OutStream & NewMessage.ID;
            OutStream & Message.type;
            Parse(OutStream,Message);
            NewMessage.IP = Peer.IP;
            NewMessage.Port = Peer.ListeningPort;
            std::lock_guard Lock(m_StateMutex);
            m_ResponseCallbacks[Peer.IP][NewMessage.ID].Callback = [](UDPResponse const& Response){};
            NewMessage.RetryTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            p_SendMessage(NewMessage);
            m_SentMessages.push_back(std::move(NewMessage));
        }
        template<typename MessageType>
        std::enable_if_t<std::is_base_of_v<UDPRequest_,MessageType>,UDPTask<typename MessageType::ResponseType>> 
            SendRequest(PeerInfo const& Peer,MessageType Message,float Timeout=5)
        {
            typedef typename MessageType::ResponseType ResponseType;
            StoredMessage NewMessage;
            NewMessage.ID = m_NextMessageID.fetch_add(1);
            MBUtility::MBStringOutputStream OutStream(NewMessage.SerializedContent);
            OutStream & UDPMessageType::Request;
            OutStream & NewMessage.ID;
            OutStream & Message.type;
            Parse(OutStream,Message);
            NewMessage.IP = Peer.IP;
            NewMessage.Port = Peer.ListeningPort;
            UDPTask<ResponseType> Result(*this);
            Result.m_ID = NewMessage.ID;
            {
                std::lock_guard Lock(m_StateMutex);
                auto Promise = MBUtility::Promise<std::optional<std::pair<PeerInfo,ResponseType>>>();
                Result.m_SharedState->Result = Promise.GetFuture();
                m_ResponseCallbacks[Peer.IP][NewMessage.ID].Callback =
                    [this,ResultState = Result.m_SharedState,Promise=std::move(Promise),Peer=Peer] 
                            (UDPResponse const& Response) mutable
                {
                    m_RequestResponseRunner.AddTask(
                            [Response=std::move(Response),Promise=std::move(Promise),Peer=Peer,ResultState=std::move(ResultState)]() mutable
                            {
                                std::coroutine_handle<> Handle;
                                {
                                    std::lock_guard Lock(ResultState->StateMutex);
                                    ResultState->Done.store(true);
                                    if(Response.IsType<ResponseType>())
                                    {
                                        Promise.SetValue(std::move(std::make_pair(Peer,Response.GetType<ResponseType>())));
                                    }
                                    else
                                    {
                                        Promise.SetValue(std::optional<std::pair<PeerInfo,ResponseType>>());
                                    }
                                    Handle = ResultState->Continuation;
                                }
                                if(Handle)
                                {
                                    Handle.resume();
                                }
                            });
                    
                };
                p_SendMessage(NewMessage);
                m_SentMessages.push_back(std::move(NewMessage));
                   
            }
            return Result;
        }
        void SetNotificationHandler(UDPNotificationHandler Handler);
        void SetRequestHandler(UDPRequestHandler Handler);
        void RegisterTCPListener(uint32_t ConnectionID,uint32_t ClientIP,MBUtility::MOFunction<void(std::string_view)>);
        void RemoveTCPListener(uint32_t ConnectionID,uint32_t ClientIP);
        void SendTCPPacket(uint32_t ConnectionID,uint32_t ClientIP,uint16_t Port,std::string_view Data);
    };
    class UDPHandlerPacketStream : public MBUtility::BidirectionalPacketStream
    {
        UDPHandler& m_Handler;
        uint32_t m_ConnectionID = 0;
        uint32_t m_ClientIP = 0;
        uint32_t m_Port = 0;
        std::mutex m_RecieveMutex;
        std::condition_variable m_RecieveConditional;
        std::deque<std::string> m_RecievedMessages;
        std::atomic<bool> m_Stopping{false};
    public:
        UDPHandlerPacketStream(UDPHandlerPacketStream&&) = delete;
        UDPHandlerPacketStream(UDPHandlerPacketStream const&) = delete;
        UDPHandlerPacketStream(UDPHandler& Handler,uint32_t ConnectionID, uint32_t ClientIP,uint16_t Port)
            : m_Handler(Handler)
        {
            m_ConnectionID = ConnectionID;
            m_ClientIP = ClientIP;
            m_Port = Port;
            m_Handler.RegisterTCPListener(m_ConnectionID,m_ClientIP,[this](std::string_view Data)
                    {
                        std::lock_guard Lock(m_RecieveMutex);
                        m_RecievedMessages.push_back(std::string(Data));
                        m_RecieveConditional.notify_all();
                    });
        }
        virtual size_t ReadMaxPacketSize() override
        {
            return 500-8;
        }
        virtual size_t WriteMaxPacketSize() override
        {
            return 500-8;
        }
        virtual size_t ReadPacket(void* OutBuffer,size_t BufferSize,double Timeout = -1) override
        {
            std::unique_lock Lock(m_RecieveMutex);
            while(m_RecievedMessages.size() == 0 && !m_Stopping.load())
            {
                m_RecieveConditional.wait(Lock);   
            }
            if(m_Stopping.load())
            {
                return 0;   
            }
            auto& CurrentPacket = m_RecievedMessages.front();
            auto PacketSize = CurrentPacket.size();
            std::memcpy(OutBuffer,CurrentPacket.data(),std::min(BufferSize,CurrentPacket.size()));
            m_RecievedMessages.pop_front();
            return std::min(BufferSize,PacketSize);
        }
        virtual void WritePacket(const void* InBuffer,size_t BufferSize,double Timeout = -1) override
        {
            m_Handler.SendTCPPacket(m_ConnectionID,m_ClientIP,m_Port,std::string_view((const char*)InBuffer,((const char*)InBuffer)+BufferSize));
        }
        ~UDPHandlerPacketStream()
        {
            m_Handler.RemoveTCPListener(m_ConnectionID,m_ClientIP);
            m_Stopping.store(true);
            {
                std::lock_guard Lock(m_RecieveMutex);
                m_RecieveConditional.notify_all();
            }
        }
    };


    template<typename T>
    class UDPTaskQueue
    {
        typedef UDPHandler::UDPTask<T> TaskType;
        struct UDPTaskQueueState
        {
            std::mutex m_InternalsMutex;
            std::atomic<int> m_Size{0};
            std::atomic<int> m_FinishedCount{0};
            std::unordered_map<uint32_t,TaskType> m_StoredTasks;
            std::vector<uint32_t> m_FinishedTasks;
            std::coroutine_handle<> m_Continuation;
        };
        std::shared_ptr<UDPTaskQueueState> m_State = std::make_shared<UDPTaskQueueState>();

    public:
        size_t size() const
        {
            return m_State->m_Size.load();
        }
        void AddTask(TaskType NewTask)
        {
            m_State->m_Size.fetch_add(1);
            NewTask.m_SharedState->Result.Then([&Handler=NewTask.m_AssociatedHandler,State=m_State,Id = NewTask.m_ID](auto const& obj)
                    {
                        std::lock_guard Lock(State->m_InternalsMutex);
                        State->m_FinishedTasks.push_back(Id);
                        State->m_FinishedCount.fetch_add(1);
                        if(State->m_Continuation)
                        {
                            Handler.m_RequestResponseRunner.AddTask([Continuation=State->m_Continuation]()
                                    {
                                        Continuation.resume();
                                    });
                            State->m_Continuation = std::coroutine_handle<>();
                        } });
            std::lock_guard Lock(m_State->m_InternalsMutex);
            auto ID = NewTask.m_ID;
            //m_StoredTasks[ID] = std::move(NewTask);
            m_State->m_StoredTasks.emplace(ID,std::move(NewTask));
        }


        bool await_ready()
        {
            return m_State->m_FinishedCount.load() > 0;
        }
        bool await_suspend(std::coroutine_handle<> Handle)
        {
            std::lock_guard Lock(m_State->m_InternalsMutex);
            if(m_State->m_FinishedTasks.size() > 0)
            {
                return false;
            }
            m_State->m_Continuation = Handle;
            return true;
        }
        std::optional<std::pair<PeerInfo,T>> await_resume()
        {
            std::lock_guard Lock(m_State->m_InternalsMutex);
            if(m_State->m_FinishedTasks.size() == 0)
            {
                throw std::runtime_error("Invariant broken: await_resume called on UDPTaskQueue with no finished tasks");   
            }
            auto ID = m_State->m_FinishedTasks.back();
            m_State->m_FinishedTasks.pop_back();
            m_State->m_FinishedCount.fetch_add(-1);
            m_State->m_Size.fetch_add(-1);

            auto It = m_State->m_StoredTasks.find(ID);
            if(It == m_State->m_StoredTasks.end())
            {
                throw std::runtime_error("Error in UDPHandler: Task with invalid ID finished");
            }
            std::optional<std::pair<PeerInfo,T>> ReturnValue = It->second.await_resume();
            m_State->m_StoredTasks.erase(It);
            return ReturnValue;
        }
        ~UDPTaskQueue()
        {
        }
    };
}
