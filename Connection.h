#pragma once
#include <condition_variable>
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
#include <numeric>
#include <algorithm>

#include "Task.h"
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
    typedef MBUtility::MOFunction<void(PeerInfo const&,uint64_t ConnectionID, Message const&)> MessageCallback;
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
            uint64_t ConnectionID = 0;
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

       Connection(std::unique_ptr<MBUtility::BidirectionalPacketStream> PacketStream,ConnectionParameters Params,PeerInfo Peer,uint64_t ConnectionID,MessageCallback MessageHandler,MBUtility::MOFunction<void(ConnectionParameters const&)> QuitHandler);


        void SendResponse(MessageHeader const& RecievedMessage,Message Response);
        void SendStreamedResponse(MessageHeader const& RecievedMessage,MessageType Type,StreamedResponseHandler Handler);

        void SendMessage(Message MessageToSend,MessageCallback Callback);
        MBUtility::Future<std::optional<std::pair<PeerInfo,Message>>> SendMessage(Message MessageToSend);

        uint16_t GetLocalPort();

        template<typename ResponseType,typename FuncType>
        MBUtility::Future<bool> SendStreamingMessage(MessageContent MessageToSend,FuncType Func)
        {
            MBUtility::Promise<bool> Promise;
            auto ReturnValue = Promise.GetFuture();
            std::lock_guard Lock(m_SharedState->SendMutex);
            QueuedMessage& NewMessage = m_SharedState->QueuedMessages.emplace_back();
            Message& RawMessage = NewMessage.MessageToSend.GetOrAssign<Message>();
            RawMessage.Header.MessageID = m_SharedState->NextSendID.fetch_add(1);
            RawMessage.Header.ResponseID = 0;
            RawMessage.Header.Type = MessageToSend.Visit([](auto& member){return member.type;});
            RawMessage.Content = std::move(MessageToSend);
            NewMessage.ResponseCallback =  
                StreamedHandler([Func=std::move(Func),State=m_SharedState,Promise=std::move(Promise)](MBUtility::IndeterminateInputStream& StreamReader) mutable
                {
                    ResponseType NewMessage;
                    uint16_t Size = 0;
                    StreamReader & Size;
                    if(Size == 0)
                    {
                        Promise.SetValue(true);
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
                        Promise.SetValue(false);
                        return false;
                    }
                    return true;
                });
            m_SharedState->SendConditional.notify_one();
            return ReturnValue;
        }
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
        void p_FillClosest(IDSorter const& Sorter,TreeNode const& Node,std::vector<PeerInfo>& OutInfo,ID const& TargetID,int k,Func&& AcceptFunc) 
        {
                   
            if(OutInfo.size() >= k)
            {
                return;
            }

            //Node.Left == nullptr <-> Node.Right == nullptr
            if(Node.Left == nullptr)
            {
                std::vector<int> SortedIndecies;
                SortedIndecies.resize(Node.Content.size());
                std::iota(SortedIndecies.begin(),SortedIndecies.end(),0);
                std::sort(SortedIndecies.begin(),SortedIndecies.end(),[&](int lhs,int rhs)
                        {
                            return Sorter(Node.Content[lhs],Node.Content[rhs]);
                        });
                for(size_t i = 0; i < std::min(Node.Content.size(),size_t(OutInfo.size()-k));i++)
                {
                    if(AcceptFunc(Node.Content[SortedIndecies[ i]]))
                    {
                        OutInfo.push_back(Node.Content[SortedIndecies[i]]);
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
        std::vector<PeerInfo> FindClosest(ID const& Key,int k,T&& Func)
        {
            std::vector<PeerInfo> ReturnValue;
            auto Sorter = IDSorter(Key);
            p_FillClosest(Sorter,m_RootNode,ReturnValue,Key,k,Func);
            return ReturnValue;
        }
        void AddPeer(PeerInfo const& NewPeer);
    };

    class AsyncUDPMapper
    {
        struct Mapping
        {
            uint16_t MappedPort = 0;   
            std::chrono::time_point<std::chrono::steady_clock> LastMapped;
            double LeaseDuration = 0;

            bool operator<(Mapping const& Rhs) const
            {
                return LastMapped < Rhs.LastMapped;
            }
            bool operator>(Mapping const& Rhs) const
            {
                return LastMapped > Rhs.LastMapped;
            }
        };
        struct SharedState
        {
            std::mutex InternalsMutex;
            std::thread NotifyThread;
            std::atomic<bool> Stopping;
            std::condition_variable NewMappingConditional;
            std::vector<uint16_t> RequestedMappings;
            std::vector<uint16_t> RequestedRemovals;
        };
        std::shared_ptr<SharedState> m_State;

        static void p_NotifyThread(std::shared_ptr<SharedState> State);
    public:
        AsyncUDPMapper(AsyncUDPMapper const&) = delete;
        AsyncUDPMapper();
        bool AddPortMapping(uint16_t Port);
        bool RemovePortMapping(uint16_t Port);
        ~AsyncUDPMapper()
        {
            m_State->Stopping.store(true);
            std::lock_guard Lock(m_State->InternalsMutex);
            m_State->NewMappingConditional.notify_one();
            m_State->NotifyThread.join();
        }
    };

    class ConnectionManager;
    class ResourceStateHandle
    {
        friend class ConnectionManager;
        ConnectionManager* m_ConnectionManager = nullptr;
        ID m_ResourceID;
        ID m_DatabaseID;
        uint32_t m_ObserverID = 0;
        //std::atomic<bool> m_StateHandled{false};
        std::optional<ResourceHeader> m_CompleteHeader;

    public:
        ResourceStateHandle() = default;
        ResourceStateHandle(ResourceStateHandle&&) = default;
        ResourceStateHandle& operator=(ResourceStateHandle&&) = default;
        ResourceStateHandle(ResourceStateHandle const&) = default;

        ID const& GetResourceID()
        {
            return m_ResourceID;   
        }
        ID const& GetDatabaseID()
        {
            return m_DatabaseID;   
        }
        //void SetStateHandled()
        //{
        //    //m_StateHandled.store(false);   
        //}
        //bool StateHandled() const
        //{
        //    return m_StateHandled.load();   
        //}
        float DownloadPercent();
        std::string LocalPath();
        ResourceHeader GetHeader();
        bool HeaderAvailable();
        Task<int> StartDownload();
        Task<std::optional<ResourceHeader>> FetchHeader();
        void SetOnStateChanged(MBUtility::MOFunction<void()> OnStateChangedCallback);
        ~ResourceStateHandle();
    };


    struct DatabaseSettings
    {
        std::string DatabasePath;
        std::filesystem::path ResourceDirectory;
    };

    class ConnectionManager
    {
        struct SharedState;
        friend class ResourceStateHandle;

        template<typename T>
        struct OnErrorTag {};
            
        struct SharedState : public std::enable_shared_from_this<SharedState>
        {
            std::mutex StateMutex;
            Hash HostID;
            PeerInfo HostInfo;
            KademliaTree Peers;
            std::atomic<uint64_t> NextConnectionID{0};
            std::unordered_map<ID,std::unordered_map<uint64_t,std::shared_ptr<Connection>>> ActiveConnections;
            std::unordered_map<ID,std::unordered_set<PeerInfo>> PeerSubscriptions;

            //not needing mutex
            MBDB::MrBoboDatabase DB;
            DatabaseSettings Settings;
            MBUtility::ThreadPool ThreadPool;
            std::unique_ptr<UDPHandler> UDP;

            ResourceCallback ResourceRecievedCallback;
            RPCHandler RPCCallback;
            AsyncUDPMapper PortMapper;

            struct ResourceStateObserver
            {
                MBUtility::MOFunction<void()> Callback;
            };

            struct ActiveDownloadState
            {
                std::atomic<uint64_t> TotalBytes = 0;
                std::atomic<uint64_t> RecievedBytes = 0;
            };
            std::unordered_map<std::string,std::shared_ptr<ActiveDownloadState>> ActiveDownloads;

            std::mutex ObserverMutex;
            std::atomic<uint32_t> CurrentObserverID{1};
            std::unordered_map<std::string,std::unordered_map<uint32_t,std::shared_ptr<ResourceStateObserver>>> m_StateObservers;


            std::vector<PeerInfo> GetClosestPeers(ID const&,int k);
            std::vector<PeerInfo> GetClosestPeers(ID const& TargetID,ID const& DatabaseID,int k);

            ///PeerInfo FindPeer(ID const&);
            //std::vector<PeerInfo> FindDBPeers(ID const&,int k);


            void NotificationHandler(MessageLocation Location,UDPNotification const& Notification);
            UDPResponse RequestHandler(MessageLocation Location,UDPRequest const& Notification);

            void StoreContent(ResourceHeader const& Header,std::string const& Content);
            void AddResourceToDB(ResourceHeader const& Header,std::string const& Content);
            void RemoveResource(ID const& DBID,ID const& ResourceID);
            void RemoveResource(ID const& DBID,std::vector<std::string> const& Path);

            void GetPathID(ID const& DBID,std::vector<std::string> const& Path,MBDB::IntType& OutParent,MBDB::IntType& OutID);
            std::vector<std::string> GetAbsoluteResourcePath(ID const& DB,ID const& Resource,MBDB::IntType& OutParent,MBDB::IntType& OutID);
            bool ResourceInDB(Hash const& Resource);
            bool SubscribedToDB(Hash const& DBID);
            void AddPeerSubscriptions(PeerInfo const& Peer,std::vector<Hash> const& Subscriptions);
            bool GetResourceById(ID const& DatabaseID,ID const& ResourceID,ResourceHeader& OutHeader);

            void AddSubscription(ID const& DatabaseID,PeerInfo const& PeerInfo);
            //
            void AddJoinDBTask(ID const& DBID); 
            std::filesystem::path GetLocalResourcePath(ID const& ResourceID);

            MessageCallback GetTCPMessageHandler();


            SharedState(DatabaseSettings NewSettings)
                : DB(NewSettings.DatabasePath,MBDB::DBOpenOptions::ReadWrite)
            {
                Settings = std::move(NewSettings);
            }
        };

        Task<std::optional<std::shared_ptr<Connection>>> InitializeConnection(PeerInfo TargetPeer)
        {
            InitConnection m_Request;
            {
                std::lock_guard Lock(m_State->StateMutex);
                auto It = m_State->ActiveConnections.find(TargetPeer.ID.Content);
                if(It != m_State->ActiveConnections.end() && It->second.size() > 0)
                {
                    co_return It->second.begin()->second;
                }
            }
            m_Request.HostInfo = m_State->HostInfo;
            std::unique_ptr<MBSockets::UDPSocket> m_UDPConnection = std::make_unique<MBSockets::UDPSocket>(TargetPeer.IP,0,0);
            m_Request.HostPort = m_UDPConnection->GetBoundPort();
            m_Request.ConnectionID = m_State->NextConnectionID.fetch_add(1);

            std::shared_ptr<Connection> m_ResultConnection;
            auto Result = co_await m_State->UDP->SendRequest(TargetPeer,m_Request);
            if(!Result.has_value())
            {
                co_return nullptr;
            }
            auto const& AcceptedConnection = Result.value();
            ConnectionParameters Params;
            Params.IP = TargetPeer.IP;
            Params.PeerPort = AcceptedConnection.second.HostPort;
            Params.LocalPort = m_Request.HostPort;
            Params.PeerID.Content = IDToString(TargetPeer.ID.Content);
            m_UDPConnection->SetDstPort(AcceptedConnection.second.HostPort);
            auto NewConnection = std::make_shared<Connection>(
                    std::unique_ptr<MBUtility::BidirectionalPacketStream>(std::move(m_UDPConnection)) ,
                    std::move(Params),
                    TargetPeer,
                    m_Request.ConnectionID,
                    m_State->GetTCPMessageHandler(),
                    [State=m_State,ID=m_Request.HostInfo.ID.Content,ConnectionID = m_Request.ConnectionID](ConnectionParameters const& Params) mutable {
                        std::lock_guard Lock(State->StateMutex);
                        auto It = State->ActiveConnections.find(ID);
                        if(It != State->ActiveConnections.end())
                        {
                            It->second.erase(It->second.find(ConnectionID));
                        }
                    });


            {
                std::lock_guard StateLock = std::lock_guard(m_State->StateMutex);
                m_State->ActiveConnections[TargetPeer.ID.Content][m_Request.ConnectionID] = NewConnection;
            }
            co_return NewConnection;
        }
        Task<std::vector<PeerInfo>> GetClosestPeers(ID const& ID)
        {
            FindPeerRequest Request;
            Request.HostPeer = m_State->HostInfo;
            Request.PeerID.Content = ID;
            return GetClosestPeers(std::move(Request));
        }
        Task<std::vector<PeerInfo>> GetClosestPeers(FindPeerRequest PeerRequest)
        {
            int k = 30;
            size_t ReplicationCount = 3;
            IDSorter m_MinHeapSorter(PeerRequest.PeerID.Content,false);
            IDSorter m_IDSorter(PeerRequest.PeerID.Content);
            std::unordered_set<std::string> m_ContactedPeers;
            std::vector<PeerInfo> m_ClosestPeers;
            if(PeerRequest.DBID.IsInitalized())
            {
                 m_ClosestPeers = m_State->GetClosestPeers(PeerRequest.PeerID.Content,PeerRequest.DBID.Value().Content,10);
            }
            else
            {
                 m_ClosestPeers = m_State->GetClosestPeers(PeerRequest.PeerID.Content,10);
            }

            UDPTaskQueue<FindPeer_Response> ActiveTasks;
            for(size_t i = 0; i < std::min(m_ClosestPeers.size(),ReplicationCount);i++)
            {
                ActiveTasks.AddTask(m_State->UDP->SendRequest(m_ClosestPeers[i],PeerRequest));
            }
            std::make_heap(m_ClosestPeers.begin(),m_ClosestPeers.end(),m_MinHeapSorter);
            while(ActiveTasks.size() > 0)
            {
                auto Result = co_await ActiveTasks;
                if(!Result.has_value())
                {
                    continue;   
                }
                auto& Response = Result.value().second;
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
                            if(NewRequests < ReplicationCount && m_ClosestPeers.front().ID.Content != PeerRequest.HostPeer.ID.Content)
                            {
                                ActiveTasks.AddTask(m_State->UDP->SendRequest(Peer,PeerRequest));
                            }
                        }
                    }
                }
                if(m_ClosestPeers.size() > 0 && m_ClosestPeers.front().ID.Content == PeerRequest.HostPeer.ID.Content)
                {
                    co_return m_ClosestPeers;
                }
            }
            co_return m_ClosestPeers;
        }
        struct FetchHeaderResult
        {
            ResourceHeader Header;
            PeerInfo Peer;
        };
        Task<std::optional<FetchHeaderResult>> FetchResourceHeader(GetResourceHeader PeerRequest)
        {
            int k = 30;
            size_t ReplicationCount = 3;
            IDSorter m_MinHeapSorter(PeerRequest.ResourceID.Content,false);
            IDSorter m_IDSorter(PeerRequest.ResourceID.Content);
            std::unordered_set<std::string> m_ContactedPeers;
            std::vector<PeerInfo> m_ClosestPeers;
            m_ClosestPeers = m_State->GetClosestPeers(PeerRequest.ResourceID.Content,PeerRequest.DatabaseID.Content,10);

            UDPTaskQueue<GetResourceHeader_Response> ActiveTasks;
            for(size_t i = 0; i < std::min(m_ClosestPeers.size(),ReplicationCount);i++)
            {
                ActiveTasks.AddTask(m_State->UDP->SendRequest(m_ClosestPeers[i],PeerRequest));
            }
            std::make_heap(m_ClosestPeers.begin(),m_ClosestPeers.end(),m_MinHeapSorter);
            while(ActiveTasks.size() > 0)
            {
                auto Result = co_await ActiveTasks;
                if(!Result.has_value())
                {
                    continue;   
                }
                auto& Response = Result.value();
                //TODO verify header...
                if(Response.second.Header.IsInitalized())
                {
                    FetchHeaderResult ReturnValue;
                    ReturnValue.Header = std::move(Response.second.Header.Value());
                    ReturnValue.Peer = Result.value().first;
                    co_return ReturnValue;
                }
                if(Response.second.CloserPeers.size()  > 0)
                {
                    std::sort(Response.second.CloserPeers.begin(),Response.second.CloserPeers.end(),m_IDSorter);
                    auto PreviousClosest = m_ClosestPeers.front();
                    int NewRequests = 0;
                    for(auto const& Peer : Response.second.CloserPeers)
                    {
                        if(!m_IDSorter(m_ClosestPeers.front().ID.Content,Peer.ID.Content))
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
                            if(NewRequests < ReplicationCount)
                            {
                                ActiveTasks.AddTask(m_State->UDP->SendRequest(Peer,PeerRequest));
                            }
                        }
                    }
                }
            }
            co_return {};
        }
        Task<bool> SyncDB(std::vector<PeerInfo> Peers,ID DBID)
        {
            //take some arbitrary peer for the current version
            if(Peers.size() == 0)
            {
                co_return true;   
            }
            auto const& SyncPeer = Peers.front();
            auto ConnectionResult = co_await InitializeConnection(SyncPeer);
            if(!ConnectionResult.has_value() || ConnectionResult.value() == nullptr)
            {
                co_return false;
            }
            auto Connection = ConnectionResult.value();
            MessageContent Content;
            GetResources& Request = Content.emplace<GetResources>();
            Request.DBHash = DBID;
            Request.StartTime = 0;
            Request.EndTime = std::numeric_limits<TimestampType>::max();
            auto Handler = [State=m_State](ResourceResponse const& Response)
            {
                State->AddResourceToDB(Response.Header,Response.Content);
            };
            co_await Connection->SendStreamingMessage<ResourceResponse>(std::move(Content),Handler);
            co_return false;
        }
        Task<bool> JoinDBTask(ID DBID)
        {
            FindPeerRequest Request;
            Request.HostPeer = m_State->HostInfo;
            Request.PeerID  = m_State->HostInfo.ID;
            Request.k = 20;
            Request.DBID = Hash();
            Request.DBID.Value().Content = DBID;
            m_State->AddSubscription(DBID,m_State->HostInfo);
            auto ClosestPeers = co_await GetClosestPeers(std::move(Request));
            ClosestPeers.erase(std::remove_if(ClosestPeers.begin(),ClosestPeers.end(),
                        [&](PeerInfo const& Peer){return Peer.ID == m_State->HostInfo.ID; } ),ClosestPeers.end());
            auto SyncResult = co_await SyncDB(std::move(ClosestPeers),DBID);
            co_return true;
        }

        Task<bool> DownloadResource(ResourceHeader Resource,PeerInfo DownloadPeer);
        std::shared_ptr<SharedState> m_State;
    public:
        ConnectionManager(IDParameters Params,uint16_t ListenPort,DatabaseSettings Settings,ResourceCallback Callback,RPCHandler RPCCallback);


        //MBUtility::Future<std::shared_ptr<Connection>> 
        //    EstablishDirectConnection(ID const& PeerID);
        
          
        
        std::vector<std::string> GetAbsoluteResourcePath(ID const& DB,ID const& Resource,MBDB::IntType& OutParent,MBDB::IntType& OutID);
        void AddConnections(std::vector<PeerInfo> const& Peers);
        void AddConnection(PeerInfo Peer);
        ID PublishMessage(PublishableResourceHeader Message);
        ID PublishFile(PublishableResourceHeader Message,std::filesystem::path const& Path,bool Copy=true);
        void RemoveResource(ID const& DBID,ID const& ResourceID);
        void RemoveResource(ID const& DBID,std::vector<std::string> const& Path);
        bool GetResource(ID const& DBID,ID const& ResourceID,ResourceHeader& OutHeader);
        ResourceStateHandle GetResourceStateHandle(ID const& DBID,ID const& ResourceID);
        std::filesystem::path GetLocalResourcePath(ResourceHeader const& Header);
        std::filesystem::path GetLocalResourcePath(ID const& ResourceID);
        //

        Task<std::optional<MBParsing::JSONObject>> SendPeerRPC(ID const& PeerID,MBParsing::JSONObject Object);

        //MBUtility::Future<MBParsing::JSONObject> SendPeerRPC(ID const& PeerID,
        //        MBParsing::JSONObject ObjectToSend);
       
        void AddDBPeer(ID const& PeerID,ID const& DatabaseID);
        void AddDBPeer(PeerInfo const& Peer,ID const& DatabaseID);
        void CreateDB(DatabaseDefinition const& Definition);
        bool HasDB(ID const& DatabaseID);

        //called once after all of the initial parameters are set
        void JoinNetwork();
        Task<bool> JoinDB(ID const& DBID);

        template<typename T>
        void AddTask(T Task)
        {
            m_State->ThreadPool.AddTask([Task=std::move(Task)]()
                    {
                        Task.resume();
                    });
        }
    };
}

