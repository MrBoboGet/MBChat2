#pragma once
#include <functional>
#include "OverlayProtocol.h"
#include <MBTCP/MBTCP.h>

#include <deque>

#include <MrBoboDatabase/MrBoboDatabase.h>
#include <MBUtility/ThreadPool.h>

#include "UDPHandler.h"

#include <MBUtility/TSQueue.h>

#include <unordered_set>

#include <unordered_set>
#include <MBUtility/IndeterminateInputStream.h>

#include <MBParsing/StreamSerialize.h>
namespace MBChat2
{
    struct ConnectionParameters
    {
        Key PeerID;
        uint32_t IP = 0;
        uint16_t PeerPort = 0;
        uint16_t PeerRegularPort = 0;
        uint16_t LocalPort = 0;
    };
    class Connection;
    typedef MBUtility::MOFunction<void(PeerInfo const&, Message const&)> MessageCallback;
    typedef MBUtility::MOFunction<void(NewMessage const&)> ResourceCallback;
    typedef MBUtility::MOFunction<void(PeerInfo const&,MBParsing::JSONObject,MBUtility::Promise<MBParsing::JSONObject>)> RPCHandler;
    typedef MBUtility::MOFunction<void (MBUtility::MBOctetOutputStream&)> StreamedResponseHandler;
    class Connection
    {
        typedef MBUtility::MOFunction<bool(MBUtility::IndeterminateInputStream&)> StreamedHandler;
        typedef MBUtility::StaticVariant<std::monostate,MessageCallback,StreamedHandler> 
            ResponseHandler;


        struct StreamedResponseMessage
        {
            MessageHeader Header;
            StreamedResponseHandler Handler;
        };

        struct QueuedMessage
        {
            MBUtility::StaticVariant<Message,StreamedResponseMessage> MessageToSend;
            ResponseHandler ResponseCallback;
        };

        struct State
        {
            PeerInfo Peer;

            std::unique_ptr<MBTCP::MBTCP> Transport;
            std::atomic<bool> Stopping;
            std::thread ReadThread;
            std::thread SendThread;
            std::atomic<uint64_t> NextSendID = 1;
            std::atomic<uint16_t> Version = 0;

            //Not changed after they have been set by the constructor
            MBUtility::MOFunction<void(PeerInfo const&)> CloseHandler;
            MessageCallback GenericMessageHandler;

            std::mutex SendMutex;
            std::condition_variable SendConditional;
            std::unordered_map<uint64_t,ResponseHandler> MessageCallbacks;
            std::deque<QueuedMessage> QueuedMessages;
        };

        static bool p_VerifyAuthenticity(State const& SharedState,Message const& NewMessage);
        static void p_ReadThread(std::shared_ptr<State> State);
        static void p_SendThread(std::shared_ptr<State> State);

        std::shared_ptr<State> m_SharedState;
        ConnectionParameters m_Params;
    public:
        ~Connection();

        Connection(std::unique_ptr<MBUtility::BidirectionalPacketStream> PacketStream,ConnectionParameters Params,PeerInfo Peer,MessageCallback MessageHandler,MBUtility::MOFunction<void(ConnectionParameters const&)> QuitHandler);


        void SendResponse(MessageHeader const& RecievedMessage,Message Response);
        void SendStreamedResponse(MessageHeader const& RecievedMessage,MessageType Type,StreamedResponseHandler Handler);

        void SendMessage(Message MessageToSend,MessageCallback Callback);
        uint16_t GetLocalPort();

        template<typename MessageType,typename FuncType>
        void SendStreamingMessage(MessageContent MessageToSend,FuncType Func)
        {
            std::lock_guard Lock(m_SharedState->SendMutex);
            QueuedMessage& NewMessage = m_SharedState->QueuedMessages.emplace_back();
            Message& RawMessage = NewMessage.MessageToSend.GetOrAssign<Message>();
            RawMessage.Header.MessageID = m_SharedState->NextSendID.fetch_add(1);
            RawMessage.Header.ResponseID = 0;
            RawMessage.Content = std::move(MessageToSend);
            NewMessage.ResponseCallback =  StreamedHandler([Func=std::move(Func),State=m_SharedState](MBUtility::IndeterminateInputStream& StreamReader) mutable
                {
                    MessageType NewMessage;
                    uint16_t Size = 0;
                    StreamReader & Size;
                    if(Size == 0)
                    {
                        return false;
                    }
                    try
                    {
                        //StreamReader & NewMessage;
                        Parse(StreamReader,NewMessage,State->Version.load());
                        Func(NewMessage);
                    }
                    catch(...)
                    {
                        return false;
                    }
                    return true;
                });
            m_SharedState->SendConditional.notify_one();
        }

        //handles messages not sent from a response, such as peer requets and notifications
        //void SetMessageHandler(MessageCallback Callback);
        //void AddOnCloseHandler(std::function<void(PeerParameters const&)> Callback);
    };

    struct  IDParameters
    {
        ID LocalID;
    };

    struct ListenParameters
    {
        uint16_t UDPPort = 0;

    };





    class IDSorter
    {
        ID m_TargetID;
        bool m_Min = true;
    public:


        IDSorter(ID SortID,bool Min = true)
        {
            m_TargetID = std::move(SortID);
            m_Min = Min;
        }
        bool operator()(ID const& Lhs,ID const& Rhs) const
        {
            if(Lhs.size() != Rhs.size())
            {
                throw std::runtime_error("Error when comparing distances between ID's: ID's need to be the same size in order to be comparable: "
                        "Lhs  was of size " +std::to_string(Lhs.size()) + " and rhs was of size "+std::to_string(Rhs.size()));
            }
            if(m_TargetID.size() != Lhs.size())
            {
                throw std::runtime_error("Error when comparing distances between ID's: ID's need to be the same as the target id: "
                        "Lhs  was of size " +std::to_string(Lhs.size()) + " and target ID was of size "+std::to_string(m_TargetID.size()));
            }
            bool ReturnValue = m_Min;
            for(size_t i = 0; i < Lhs.size();i++)
            {
                if(m_Min)
                {
                    if(!( (Lhs[i] ^ m_TargetID[i]) < (Rhs[i]  ^ m_TargetID[i])))
                    {
                        return false;
                    }
                }
                else
                {
                    if( (Lhs[i] ^ m_TargetID[i]) > (Rhs[i]  ^ m_TargetID[i]))
                    {
                        return true;
                    }
                }
            }
            return ReturnValue;
        }
        bool operator()(PeerInfo const& Lhs,PeerInfo const& Rhs) const
        {
            return (*this)(Lhs.ID.Content,Rhs.ID.Content);
        }
        bool operator()(ID const& Lhs,PeerInfo const& Rhs) const
        {
            return (*this)(Lhs,Rhs.ID.Content);
        }
        bool operator()(PeerInfo const& Lhs,ID const& Rhs) const
        {
            return (*this)(Lhs.ID.Content,Rhs);
        }
    };

    class KademliaTree
    {

        struct KBucket
        {
            std::vector<PeerInfo> Peers;
        };

        struct TreeNode
        {
            ID CommonPrefix;
            int NextBitPosition = 32*8-1;

            int k = 20;

            std::vector<PeerInfo> Content;
            std::unique_ptr<TreeNode> Left;
            std::unique_ptr<TreeNode> Right;
            void Split();
            void AddPeer(PeerInfo const& NewPeer);
        };

        int p_FirstSetBit(ID const& Distance);

        std::string m_HostID;
        TreeNode m_RootNode;
        std::vector<KBucket> m_Buckets;

        ID  p_StringToID(std::string const& String);

        template<typename Func>
        void p_FillClosest(IDSorter const& Sorter,TreeNode const& Node,std::vector<PeerInfo>& OutInfo,ID const& TargetID,int k,Func AcceptFunc) 
        {
                   
            if(OutInfo.size() >= k)
            {
                return;
            }

            //Node.Left == nullptr <-> Node.Right == nullptr
            if(Node.Left == nullptr)
            {
                for(size_t i = 0; i < std::min(Node.Content.size(),size_t(OutInfo.size()-k));i++)
                {
                    if(AcceptFunc(Node.Content[i]))
                    {
                        OutInfo.push_back(Node.Content[i]);
                    }
                }
            }
            else
            {
                auto const& LeftID = Node.Left->CommonPrefix;
                auto const& RightID = Node.Right->CommonPrefix;
                auto LesserNode = Sorter(LeftID,RightID) ? Node.Left.get() : Node.Right.get();
                auto GreaterNode = Node.Left.get() == LesserNode ? Node.Right.get() : Node.Left.get();
                p_FillClosest(Sorter,*LesserNode,OutInfo,TargetID,k,AcceptFunc);
                if(OutInfo.size() < k)
                {
                    p_FillClosest(Sorter,*GreaterNode,OutInfo,TargetID,k,AcceptFunc);
                }
            }
        }

    public:
        std::vector<PeerInfo> FindClosest(ID const& Key,int k);
        template<typename T>
        std::vector<PeerInfo> FindClosest(ID const& Key,int k,T Func)
        {
            std::vector<PeerInfo> ReturnValue;
            auto Sorter = IDSorter(Key);
            p_FillClosest(Sorter,m_RootNode,ReturnValue,Key,k,Func);
            return ReturnValue;
        }
        void AddPeer(PeerInfo const& NewPeer);
    };

    class ConnectionManager
    {
        struct SharedState;

        template<typename T>
        struct OnErrorTag {};
            
        template<typename T>
        class Task : public  std::enable_shared_from_this<T>
        {
            std::shared_ptr<SharedState> m_State;
            std::mutex m_InternalLock;
        protected:
            std::unique_lock<std::mutex> GetLock()
            {
                return std::unique_lock<std::mutex>(m_InternalLock);
            }
        public:
            SharedState& GetState()
            {
                return *m_State;   
            }
            void SetSharedState(std::shared_ptr<SharedState> State)
            {
                m_State = std::move(State);   
            }

            template<typename R>
            std::enable_if_t<std::is_base_of_v<UDPRequest_,R>>
                SendRequest(PeerInfo const& Peer,R Message)
            {
                typedef typename R::ResponseType Result;
                auto Future = GetState().UDP->SendRequest(Peer,Message);
                Future.Then([Obj=this->shared_from_this()](Result Res)
                        {
                            (*Obj)(Res);
                        });
                Future.OnError([Obj=this->shared_from_this(),Msg = std::move(Message)]()
                        {
                            (*Obj)(Msg);
                        });
            }

            template<typename... Args>
            MBUtility::MOFunction<void(Args...)> MakeCallback()
            {
                return [This=this->shared_from_this()](Args... Arguments) mutable
                {
                    (*This)(std::forward<Args>(Arguments)...);
                };
            }
        };

        //advertices these subscriptions with regular intervalls
        struct SharedState : public std::enable_shared_from_this<SharedState>
        {
            std::mutex StateMutex;
            Hash HostID;
            PeerInfo HostInfo;
            KademliaTree Peers;
            std::unordered_map<ID,std::shared_ptr<Connection>> ActiveConnections;
            std::unordered_map<ID,std::unordered_set<PeerInfo>> PeerSubscriptions;

            //not needing mutex
            MBDB::MrBoboDatabase DB;
            MBUtility::ThreadPool ThreadPool;
            std::unique_ptr<UDPHandler> UDP;

            ResourceCallback ResourceRecievedCallback;
            RPCHandler RPCCallback;

            std::vector<PeerInfo> GetClosestPeers(ID const&,int k);
            std::vector<PeerInfo> GetClosestPeers(ID const& TargetID,ID const& DatabaseID,int k);

            ///PeerInfo FindPeer(ID const&);
            //std::vector<PeerInfo> FindDBPeers(ID const&,int k);


            void NotificationHandler(MessageLocation Location,UDPNotification const& Notification);
            UDPResponse RequestHandler(MessageLocation Location,UDPRequest const& Notification);
            void AddResourceToDB(ResourceHeader const& Header,std::string const& Content);
            bool ResourceInDB(Hash const& Resource);
            bool SubscribedToDB(Hash const& DBID);
            void AddPeerSubscriptions(PeerInfo const& Peer,std::vector<Hash> const& Subscriptions);
            //
            void AddJoinDBTask(ID const& DBID); 

            MessageCallback GetTCPMessageHandler();

            template<typename T,typename... ArgTypes>
            std::enable_if_t<std::is_constructible_v<T,ArgTypes...>> AddTask(ArgTypes&&... Args)
            {
                auto Task = std::make_shared<T>(std::forward<ArgTypes>(Args)...);
                Task->SetSharedState(shared_from_this());
                Task->Init();
            }
            template<typename ReturnType,typename... ArgTypes>
            MBUtility::MOFunction<ReturnType(ArgTypes...)> MakeCallback(ReturnType (SharedState::* Func)(ArgTypes...))
            {
                return [Func=Func,Obj = shared_from_this()](ArgTypes... Args) {return (*Obj).*Func(Args...);};
            }

            SharedState(std::string const& DatabasePath)
                : DB(DatabasePath,MBDB::DBOpenOptions::ReadWrite)
            {
                   
            }
        };


        class InitializeConnection : public Task<InitializeConnection>
        {
            typedef MBUtility::MOFunction<void(std::shared_ptr<Connection>)> ConnectionCallback;
            typedef MBUtility::MOFunction<void(PeerInfo const& )> ConnectionFailedCallback;
            InitConnection m_Request;
            PeerInfo m_TargetPeer;
            std::shared_ptr<Connection> m_ResultConnection;
            std::unique_ptr<MBSockets::UDPSocket> m_UDPConnection;
            std::atomic<bool> m_Active = true;

            ConnectionCallback m_ConnectionSucceedCallback;
            ConnectionFailedCallback m_ConnectionFailedCallback;
        public:

            InitializeConnection(PeerInfo TargetPeer,PeerInfo HostInfo,ConnectionCallback Succeeded,ConnectionFailedCallback Failed)
            {
                m_TargetPeer = TargetPeer;
                m_Request.HostInfo = std::move(HostInfo);
                m_UDPConnection = std::make_unique<MBSockets::UDPSocket>(TargetPeer.IP,0,0);
                m_Request.HostPort = m_UDPConnection->GetBoundPort();

                m_ConnectionSucceedCallback = std::move(Succeeded);
                m_ConnectionFailedCallback = std::move(Failed);
            }

            void Init()
            {
                SendRequest(m_TargetPeer,m_Request);
            }

            void operator()(InitConnection const& FailedConnection)
            {
                if(!m_Active)
                {
                    return; 
                }
                m_Active = false;
                if(m_ConnectionFailedCallback)
                {
                    m_ConnectionFailedCallback(m_TargetPeer);
                }
            }
            void operator()(InitConnection_Response const& AcceptedConnection)
            {
                if(!m_Active || !AcceptedConnection.Accepted)
                {
                    return; 
                }
                ConnectionParameters Params;
                Params.IP = m_TargetPeer.IP;
                Params.PeerPort = AcceptedConnection.HostPort;
                Params.LocalPort = m_Request.HostPort;
                Params.PeerID.Content = IDToString(m_TargetPeer.ID.Content);
                m_UDPConnection->SetDstPort(AcceptedConnection.HostPort);
                auto NewConnection = std::make_shared<Connection>(
                        std::unique_ptr<MBUtility::BidirectionalPacketStream>(std::move(m_UDPConnection)) ,
                        std::move(Params),
                        m_TargetPeer,
                        GetState().GetTCPMessageHandler(),
                        [ID=m_Request.HostInfo.ID.Content,ThisTask=shared_from_this()](ConnectionParameters const& Params) mutable {
                            std::lock_guard Lock(ThisTask->GetState().StateMutex);
                            ThisTask->GetState().
                            ActiveConnections.erase(ThisTask->GetState().ActiveConnections.find(ID));
                        });


                {
                    std::lock_guard StateLock = std::lock_guard(GetState().StateMutex);
                    GetState().ActiveConnections[m_Request.HostInfo.ID.Content] = NewConnection;
                }
                m_ConnectionSucceedCallback(NewConnection);
            }
        };

        class SyncDBTask : public Task<SyncDBTask>
        {
            ResourceHeader m_MessageToDownload;
            //try to send message to the peer  that send it in the first place,
            //and fall back to sending it directly 
            ID m_DBID;
            PeerInfo m_TargetPeer;
            PeerInfo m_HostInfo;
        public:
            //SyncDBTask(MessageLocation InitialLocation,ResourceHeader HeaderToDownload)
            //{

            //}
            SyncDBTask(std::vector<PeerInfo> const& Peers,ID DBID)
            {
                int PeerSyncCount = std::min(size_t(1),Peers.size());
                if(Peers.size() > 0)
                {
                    m_TargetPeer = Peers[0];   
                }
                std::swap(DBID,m_DBID);
                //random selection
            }
            void Init()
            {
                PeerInfo HostInfo;
                {
                    std::lock_guard Lock(GetState().StateMutex);
                    HostInfo = GetState().HostInfo;
                }
                GetState().AddTask<InitializeConnection>(
                        m_TargetPeer,
                        HostInfo,
                        MakeCallback<std::shared_ptr<Connection>>(),
                        MakeCallback<PeerInfo const&>());
            }

            void operator()(PeerInfo const&)
            {
                   
            }

            void operator()(std::shared_ptr<Connection> NewConnection)
            {
                GetResources Message;
                Message.DBHash.Content = m_DBID;
                Message.EndID.Content = ID(m_DBID.size(),std::numeric_limits<uint8_t>::max());
                Message.StartID.Content = ID(m_DBID.size(),0);
                NewConnection->SendStreamingMessage<ResourceResponse>(std::move(Message),MakeCallback<ResourceResponse const&>());
            }
            void operator()(ResourceResponse const& NewHeader)
            {
                GetState().AddResourceToDB(NewHeader.Header,NewHeader.Content);
            }

            void operator()(GetResourceHeader const& FailedRequest)
            {
                   
            }
            void operator()(GetResourceContent const& FailedRequest)
            {
                   
            }
            void operator()(GetResourceHeader_Response const& Response)
            {

            }
            void operator()(GetResourceContent_Response const& Response)
            {
            }
        };
        class FindClosestPeers : public Task<FindClosestPeers>
        {
            typedef MBUtility::MOFunction<void(std::vector<PeerInfo> const&)> PeersFoundCallback;
            int k = 30;
            size_t ReplicationCount = 3;
            FindPeerRequest m_PeerRequest;
            PeersFoundCallback m_Callback;
            IDSorter m_MinHeapSorter;
            IDSorter m_IDSorter;

            std::atomic<bool> m_Finished = false;
            std::atomic<int> m_ActiveRequest = 0;

            std::unordered_set<std::string> m_ContactedPeers;
            std::vector<PeerInfo> m_ClosestPeers;

            //try to send message to the peer  that send it in the first place,
            //and fall back to sending it directly 
            //
            
        public:
            FindClosestPeers(FindPeerRequest PeerRequest,PeersFoundCallback PeersFound)
                : m_MinHeapSorter(PeerRequest.PeerID.Content,false),m_IDSorter(PeerRequest.PeerID.Content)
            {
                m_Callback = std::move(PeersFound);
                m_PeerRequest = std::move(PeerRequest);
            }
            FindClosestPeers(FindPeerRequest PeerRequest,ID const& DBID,PeersFoundCallback PeersFound)
                : m_MinHeapSorter(PeerRequest.PeerID.Content,false),m_IDSorter(PeerRequest.PeerID.Content)
            {
                PeerRequest.PeerID.Content = DBID;
                m_PeerRequest = std::move(PeerRequest);
            }
            FindClosestPeers(ID const& TargetID,PeersFoundCallback PeersFound)
                : m_MinHeapSorter(TargetID,false),m_IDSorter(TargetID)
            {
                m_PeerRequest.PeerID.Content = TargetID;
            }
            void Init()
            {
                auto Lock = GetLock();
                m_ClosestPeers = GetState().GetClosestPeers(m_PeerRequest.PeerID.Content,20);
                for(size_t i = 0; i < std::min(m_ClosestPeers.size(),ReplicationCount);i++)
                {
                    SendRequest(m_ClosestPeers[i],m_PeerRequest);
                    m_ActiveRequest.fetch_add(1);
                }
                std::make_heap(m_ClosestPeers.begin(),m_ClosestPeers.end(),m_MinHeapSorter);
            }
            void operator()(FindPeerRequest const& FailedRequest)
            {
                m_ActiveRequest.fetch_add(-1);
            }
            void operator()(FindPeer_Response Response)
            {
                if(m_Finished)
                {
                    return;   
                }
                auto Lock = GetLock();
                m_ActiveRequest.fetch_add(-1);
                if(Response.Peers.size()  > 0)
                {
                    std::sort(Response.Peers.begin(),Response.Peers.end(),m_IDSorter);
                    auto PreviousClosest = m_ClosestPeers.front();
                    int NewRequests = 0;
                    for(auto const& Peer : Response.Peers)
                    {
                        if(!m_IDSorter(Peer.ID.Content,Response.HostID.Content) || m_IDSorter(m_ClosestPeers.front().ID.Content,Peer.ID.Content))
                        {
                            break;
                        }
                        auto StringID = IDToString(Peer.ID.Content);
                        if(m_ContactedPeers.find(StringID) == m_ContactedPeers.end())
                        {
                            m_ContactedPeers.insert(std::move(StringID));
                            m_ClosestPeers.push_back(Peer);
                            std::push_heap(m_ClosestPeers.begin(),m_ClosestPeers.end(),m_MinHeapSorter);
                            //ar
                            if(m_ClosestPeers.size() > k)
                            {
                                m_ClosestPeers.resize(k);
                            }
                            if(NewRequests < ReplicationCount && m_ClosestPeers.front().ID.Content != m_PeerRequest.HostPeer.ID.Content)
                            {
                                SendRequest(Peer,m_PeerRequest);
                                m_ActiveRequest.fetch_add(1);
                            }
                        }
                    }
                }
                if(m_ClosestPeers.size() > 0 && m_ClosestPeers.front().ID.Content == m_PeerRequest.HostPeer.ID.Content)
                {
                    m_Finished = true;
                    if(m_Callback)
                    {
                        m_Callback(m_ClosestPeers);
                    }
                }
                if(m_ActiveRequest.load() == 0)
                {
                    m_Finished = true;
                    m_Callback(m_ClosestPeers);
                }
            }
        };


        std::shared_ptr<SharedState> m_State;
    public:
        ConnectionManager(IDParameters Params,std::string const& DatabasePath,ResourceCallback Callback,RPCHandler RPCCallback);


        //MBUtility::Future<std::shared_ptr<Connection>> 
        //    EstablishDirectConnection(ID const& PeerID);

        void AddConnections(std::vector<PeerInfo> const& Peers);
        void AddConnection(PeerInfo Peer);
        void PublishMessage(PublishableResourceHeader Message);
        //

        MBUtility::Future<MBParsing::JSONObject> SendPeerRPC(ID const& PeerID,
                MBParsing::JSONObject ObjectToSend);
       
        void AddDBPeer(ID const& PeerID,ID const& DatabaseID);
        void AddDBPeer(PeerInfo const& Peer,ID const& DatabaseID);
        void CreateDB(DatabaseDefinition const& Definition);
        bool HasDB(ID const& DatabaseID);

        //called once after all of the initial parameters are set
        void JoinNetwork();
        void JoinDB(ID const& DBID);
    };
}

