#include "Connection.h"
#include "OverlayProtocol.h"

#include <MrBoboSockets/MrBoboSockets.h>

#include <unordered_set>
#include <chrono>

#include <MBUPNP/MBUPNP.h>
#include <MBUtility/Async.h>
#include <chrono>

namespace MBChat2
{
    bool Connection::p_VerifyAuthenticity(State const& SharedState,Message const& NewMessage)
    {
        return true;
    }
    void Connection::p_ReadThread(std::shared_ptr<State> State)
    {
        try
        {
            State->Transport->Open();
            while(!State->Stopping.load())
            {
                Message NewMessage;
                NewMessage.ReadMessageHeader(*State->Transport);
                if(!p_VerifyAuthenticity(*State,NewMessage))
                {
                    //message silently dropped, should reduce trust for peer or something
                    continue;
                }
                //std::cout<<"TCP message recieved"<<std::endl;
                ResponseHandler Handler;
                {
                    std::lock_guard Lock(State->SendMutex);
                    auto It = State->MessageCallbacks.find(NewMessage.Header.ResponseID);
                    if(It != State->MessageCallbacks.end())
                    {
                        Handler = std::move(It->second);
                        State->MessageCallbacks.erase(It);
                    }
                }
                if(Handler.IsType<MessageCallback>())
                {
                    NewMessage.ReadBody(*State->Transport);
                    Handler.GetType<MessageCallback>()(State->Peer,NewMessage);
                }
                else if(Handler.IsType<StreamedHandler>())
                {
                    auto& Callback= Handler.GetType<StreamedHandler>();
                    while(Callback( (MBUtility::IndeterminateInputStream&)*State->Transport)) {

                    }
                }
                else
                {
                    NewMessage.ReadBody(*State->Transport);
                    State->GenericMessageHandler(State->Peer,NewMessage);
                }
            }
        }
        catch(...)
        {
            int hej = 2;
        }
        State->Stopping.store(true);
        State->Transport->Abort();
        if(State->CloseHandler)
        {
            State->CloseHandler(State->Peer);
        }
    }
    void Connection::p_SendThread(std::shared_ptr<State> State)
    {
        try
        {
            while(!State->Stopping.load())
            {
                {
                    std::unique_lock Lock(State->SendMutex);
                    while(!State->Stopping.load() && State->QueuedMessages.size() == 0)
                    {
                        State->SendConditional.wait(Lock);
                    }
                    if(State->Stopping.load())
                    {
                        break;
                    }
                    auto QueuedMessage = std::move(State->QueuedMessages.front());
                    State->QueuedMessages.pop_front();
                    if(QueuedMessage.MessageToSend.IsType<Message>())
                    {
                        Message MessageToSend;
                        auto& NewMessage = QueuedMessage.MessageToSend.GetType<Message>();
                        NewMessage.Header.MessageID = State->NextSendID;
                        State->NextSendID.fetch_add(1);
                        State->MessageCallbacks[NewMessage.Header.MessageID] = std::move(QueuedMessage.ResponseCallback);
                        MessageToSend = std::move(NewMessage);

                        std::string OutputMessage;
                        MBUtility::MBStringOutputStream ContentStream(OutputMessage);
                        MessageToSend.Header.Version = 0;
                        ParseBody(static_cast<MBUtility::MBOctetOutputStream&>(ContentStream),MessageToSend);
                        MessageToSend.Header.MessageLength = OutputMessage.size();
                        ParseHeader( static_cast<MBUtility::MBOctetOutputStream&>(*State->Transport),MessageToSend);
                        *State->Transport << OutputMessage;
                    }
                    else if(QueuedMessage.MessageToSend.IsType<StreamedResponseMessage>())
                    {
                        //uint16_t Version = 0;
                        //MessageType Type = MessageType::Handshake;
                        //uint32_t MessageID = 0;
                        //uint32_t ResponseID = 0;
                        //uint64_t MessageLength = 0;
                        auto& StreamedMessage = QueuedMessage.MessageToSend.GetType<StreamedResponseMessage>();
                        ((MBUtility::MBOctetOutputStream&) *State->Transport) & StreamedMessage.Header;
                        StreamedMessage.Handler( (MBUtility::MBOctetOutputStream&)*State->Transport);
                    }
                }
            }
        }
        catch(...)
        {
        }
        State->Stopping.store(true);
        State->Transport->Abort();
    }
    Connection::~Connection()
    {
        m_SharedState->Stopping.store(true);
        m_SharedState->Transport->Abort();

        {
            std::lock_guard Lock(m_SharedState->SendMutex);
            m_SharedState->SendConditional.notify_one();
        }
        m_SharedState->SendThread.detach();
        m_SharedState->ReadThread.detach();
    }
    void Connection::SendMessage(Message MessageToSend,MessageCallback Callback)
    {
        std::lock_guard Lock(m_SharedState->SendMutex);
        QueuedMessage& NewMessage = m_SharedState->QueuedMessages.emplace_back();
        NewMessage.MessageToSend = std::move(MessageToSend);
        NewMessage.ResponseCallback = std::move(Callback);
        m_SharedState->SendConditional.notify_one();
    }
    MBUtility::Future<std::optional<std::pair<PeerInfo,Message>>> Connection::SendMessage(Message MessageToSend)
    {
        MBUtility::Promise<std::optional<std::pair<PeerInfo,Message>>> Promise;
        auto ReturnValue = Promise.GetFuture();

        std::lock_guard Lock(m_SharedState->SendMutex);
        QueuedMessage& NewMessage = m_SharedState->QueuedMessages.emplace_back();
        NewMessage.MessageToSend = std::move(MessageToSend);
        NewMessage.ResponseCallback = MessageCallback([Promise=std::move(Promise)](PeerInfo const& Peer, Message const& NewMessage) mutable
        {
            Promise.SetValue( std::optional(std::pair<PeerInfo,Message>(Peer,std::move(NewMessage))));
        });
        m_SharedState->SendConditional.notify_one();

        return ReturnValue;
    }
    uint16_t Connection::GetLocalPort()
    {
        return m_Params.LocalPort;
    }
    void Connection::SendResponse(MessageHeader const& RecievedMessage,Message Response)
    {
        std::lock_guard Lock(m_SharedState->SendMutex);
        QueuedMessage& NewMessage = m_SharedState->QueuedMessages.emplace_back();
        auto& RawMessage = NewMessage.MessageToSend.GetOrAssign<Message>();
        RawMessage.Header.Type = Response.Header.Type;
        RawMessage.Header.MessageID = 0;
        RawMessage.Header.ResponseID = RecievedMessage.MessageID;
        RawMessage.Header.Version = RecievedMessage.Version;
        RawMessage.Content = std::move(Response.Content);
        m_SharedState->SendConditional.notify_one();
    }
    void Connection::SendStreamedResponse(MessageHeader const& RecievedMessage,MessageType Type,StreamedResponseHandler Handler)
    {
        std::lock_guard Lock(m_SharedState->SendMutex);
        QueuedMessage& NewMessage = m_SharedState->QueuedMessages.emplace_back();
        auto& RawMessage = NewMessage.MessageToSend.GetOrAssign<StreamedResponseMessage>();
        RawMessage.Header.Type = Type;
        RawMessage.Header.MessageID = 0;
        RawMessage.Header.ResponseID = RecievedMessage.MessageID;
        RawMessage.Header.Version = RecievedMessage.Version;
        RawMessage.Handler = std::move(Handler);
        m_SharedState->SendConditional.notify_one();
    }
    Connection::Connection(std::unique_ptr<MBUtility::BidirectionalPacketStream> UDPStream,ConnectionParameters ConnectionParams,PeerInfo Peer,MessageCallback MessageHandler,MBUtility::MOFunction<void(ConnectionParameters const&)> QuitHandler)
    {
        m_SharedState = std::make_shared<State>();
        MBTCP::TCPConnectionParameters Params;
        Params.DestinationPort = 1337;
        Params.HostPort = 1337;
        m_Params = ConnectionParams;

        m_SharedState->Peer =  std::move(Peer);
        m_SharedState->Transport = std::make_unique<MBTCP::MBTCP>(Params, MBUtility::SmartPtr(std::move(UDPStream)));
        m_SharedState->GenericMessageHandler = std::move(MessageHandler);
        m_SharedState->ReadThread = std::thread(Connection::p_ReadThread,m_SharedState);
        m_SharedState->SendThread = std::thread(Connection::p_SendThread,m_SharedState);
    }

    //kademlia
    int KademliaTree::p_FirstSetBit(ID const& Distance)
    {
        int ReturnValue = 0;


        return ReturnValue;
    }
    //KademliaTree::ID const* KademliaTree::p_GetTreeRepresentative(TreeNode const& Node)
    //{
    //    ID const* ReturnValue = nullptr;
    //    if(Node.Content.size() > 0)
    //    {
    //        return &Node.Content[0].ID.Content;
    //    }
    //    else
    //    {
    //        ReturnValue = Node.Left != nullptr ? p_GetTreeRepresentative(*Node.Left) : nullptr;
    //        ReturnValue = (ReturnValue != nullptr && Node.Right != nullptr) ? p_GetTreeRepresentative(*Node.Right) : ReturnValue;
    //    }
    //    return ReturnValue;
    //}
    ID KademliaTree::p_StringToID(std::string const& String)
    {
        ID ReturnValue;
        ReturnValue.resize(String.size());
        std::memcpy(ReturnValue.data(),String.data(),String.size());
        return ReturnValue;
    }
    //void KademliaTree::p_FillClosest(TreeNode const& Node,std::vector<PeerInfo>& OutInfo,ID const& TargetID,int k)
    //{
    //    if(OutInfo.size() >= k)
    //    {
    //        return;
    //    }

    //    //Node.Left == nullptr <-> Node.Right == nullptr
    //    if(Node.Left == nullptr)
    //    {
    //        OutInfo.insert(OutInfo.end(),Node.Content.begin(),Node.Content.begin()+std::min(Node.Content.size(),size_t(k-OutInfo.size())));
    //    }
    //    else
    //    {
    //        auto const& LeftID = Node.Left->CommonPrefix;
    //        auto const& RightID = Node.Right->CommonPrefix;
    //        auto LeftDistance = Distance(LeftID,TargetID);
    //        auto RightDistance = Distance(LeftID,TargetID);
    //        auto LesserNode = LeftDistance < RightDistance ? Node.Left.get() : Node.Right.get();
    //        auto GreaterNode = LeftDistance < RightDistance ? Node.Right.get() : Node.Left.get();
    //        p_FillClosest(*LesserNode,OutInfo,TargetID,k);
    //        if(OutInfo.size() < k)
    //        {
    //            p_FillClosest(*GreaterNode,OutInfo,TargetID,k);
    //        }
    //    }
    //}
    std::vector<PeerInfo> KademliaTree::FindClosest(ID const& Key,int k)
    {
        std::vector<PeerInfo> ReturnValue;
        //auto TargetID = p_StringToID(Key);
        auto Sorter = IDSorter(Key);
        p_FillClosest(Sorter,m_RootNode,ReturnValue,Key,k,[](PeerInfo const& Peer){return true;});
        return ReturnValue;
    }
    void KademliaTree::AddPeer(PeerInfo const& NewPeer)
    {
        m_RootNode.AddPeer(NewPeer);
    }
    void KademliaTree::TreeNode::Split()
    {
        Left =  std::make_unique<TreeNode>();
        Right =  std::make_unique<TreeNode>();
        Left->CommonPrefix  = CommonPrefix;
        Right->CommonPrefix  = CommonPrefix;
        Left->CommonPrefix[NextBitPosition / 8] &= (~(1<<(NextBitPosition % 8)));
        Right->CommonPrefix[NextBitPosition / 8] |= (1<<(NextBitPosition % 8));
        Left->NextBitPosition-= 1;
        Right->NextBitPosition-= 1;
        for(auto& Peer : Content)
        {
            if(Peer.ID.Content < Right->CommonPrefix)
            {
                Left->Content.push_back(std::move(Peer));
            }
            else
            {
                Right->Content.push_back(std::move(Peer));
            }
        }
        Content.clear();
    }
    void KademliaTree::TreeNode::AddPeer(PeerInfo const& NewPeer)
    {
        if(Left == nullptr && Content.size() < k)
        {
            Content.push_back(NewPeer);
            return;
        }
        if(Left == nullptr)
        {
            Split();
        }
        if(NewPeer.ID.Content < Right->CommonPrefix)
        {
            Left->AddPeer(NewPeer);
        }
        else
        {
            Right->AddPeer(NewPeer);
        }
    }
    //
    void ConnectionManager::SharedState::GetPathID(ID const& DBID,std::vector<std::string> const& Path,MBDB::IntType& OutParent,MBDB::IntType& OutID)
    {
        auto DB = &this->DB;
        auto GetChildStatement = DB->GetSQLStatement(
                "SELECT ID,ParentID FROM ActiveTree WHERE Name = :Name AND ParentID = :ParentID AND DatabaseID = :DatabaseID");
        MBDB::IntType CurrentParent = -1;
        MBDB::IntType CurrentID = -1;
        for(int i = Path.size()-1; i >= 0; i--)
        {
            GetChildStatement.Reset();
            GetChildStatement.BindValue("Name",Path[i]);
            GetChildStatement.BindValue("ParentID",CurrentID);
            GetChildStatement.BindValue("DatabaseID",DBID);
            {
                auto Rows = DB->GetAllRows(GetChildStatement);
                if(Rows.size() != 1)
                {
                    if(Rows.size() == 0 && i == 0)
                    {
                        CurrentParent = CurrentID;
                        CurrentID = -1;
                        break;   
                    }
                    throw std::runtime_error(
                            "Database invariant broken when getting absolute path for resource: Amount of children for specified path is "+
                            std::to_string(Rows.size()));

                }
                CurrentParent = std::get<MBDB::IntType>(Rows[0]["ParentID"]);
                CurrentID = std::get<MBDB::IntType>(Rows[0]["ID"]);
            }

        }
        OutID = CurrentID;
        OutParent = CurrentParent;
    }
    std::vector<std::string> ConnectionManager::SharedState::GetAbsoluteResourcePath
        (ID const& DBID,ID const& Resource,MBDB::IntType& OutParent,MBDB::IntType& OutID)
    {
        std::vector<std::string> ReturnValue;
        std::vector<std::string> Parents;
        auto DB = &this->DB;
        auto GetParentStmt = DB->GetSQLStatement(
                "SELECT ParentHash,Name FROM Resources WHERE Hash = :Hash AND DatabaseHash = :DatabaseID");
        auto CurrentResource = Resource;
        while(true)
        {
            GetParentStmt.Reset();
            GetParentStmt.BindBlob("DatabaseID",DBID);
            GetParentStmt.BindBlob("Hash",CurrentResource);

            auto Rows = DB->GetAllRows(GetParentStmt);
            if(Rows.size() == 0)
            {
                if(CurrentResource != DBID)
                {
                    throw std::runtime_error("Database invariant broken when getting path for resource: Resource has no parent");   
                }
                break;   
            }
            auto const& ResourceRow = Rows[0];
            Parents.push_back(std::get<std::string>(ResourceRow["Name"]));
            CurrentResource = StringToID(std::get<std::string>(ResourceRow["ParentHash"]));
        }
        GetPathID(DBID,Parents,OutParent,OutID);
        return ReturnValue;
    }
    std::vector<std::string> ConnectionManager::GetAbsoluteResourcePath(ID const& DB,ID const& Resource,MBDB::IntType& OutParent,MBDB::IntType& OutID)
    {
        return m_State->GetAbsoluteResourcePath(DB,Resource,OutParent,OutID);
    }
    void ConnectionManager::AddConnections(std::vector<PeerInfo> const& Peers)
    {
        std::lock_guard StateLock(m_State->StateMutex);
        for(auto const& Peer : Peers)
        {
            m_State->Peers.AddPeer(Peer);
        }
    }
    void ConnectionManager::AddConnection(PeerInfo Peer)
    {

        {
            std::lock_guard StateLock(m_State->StateMutex);
            m_State->Peers.AddPeer(std::move(Peer));
        }
        auto AddPeerStmt = m_State->DB.GetSQLStatement("INSERT INTO Peers (ID,IP,Port) VALUES (:ID,:IPxd,:Port) ON CONFLICT(ID)"
                " DO UPDATE SET IP=excluded.IP,Port=Excluded.Port;");
        AddPeerStmt.BindValue("ID",Peer.ID.Content);
        AddPeerStmt.BindValue("IPxd",Peer.IP);
        AddPeerStmt.BindValue("Port",Peer.ListeningPort);

        m_State->DB.GetAllRows(AddPeerStmt);
    }
    void ConnectionManager::AddDBPeer(ID const& PeerID,ID const& DatabaseID)
    {
        std::vector<PeerInfo> ClosestPeers;
        {
            std::lock_guard StateLock(m_State->StateMutex);
            ClosestPeers = m_State->Peers.FindClosest(PeerID,1);
        }
        if(ClosestPeers.size() > 0 && ClosestPeers[0].ID == PeerID)
        {
            m_State->AddSubscription(DatabaseID,ClosestPeers[0]);
        }
    }
    void ConnectionManager::AddDBPeer(PeerInfo const& Peer,ID const& DatabaseID)
    {
        m_State->AddSubscription(DatabaseID,Peer);
    }
    void ConnectionManager::CreateDB(DatabaseDefinition const& Definition)
    {
        auto CreateDBStatement = m_State->DB.GetSQLStatement("INSERT INTO Databases(Hash,Type,Time) VALUES (:Hash,:Type,:Timestamp);");
        CreateDBStatement.BindValue("Hash",Definition.DatabaseID.Content);
        CreateDBStatement.BindValue("Type",Definition.Type);
        CreateDBStatement.BindValue("Timestamp",Definition.Timestamp);
        m_State->DB.GetAllRows(CreateDBStatement);

        auto InsertPeerStatement = m_State->DB.GetSQLStatement("INSERT INTO DatabaseParticipants(DatabaseID,ParticipantID) VALUES (:DatabaseID,"
             ":ParticipantID);");
        for(auto const& Participant : Definition.Participants)
        {
            InsertPeerStatement.BindValue("DatabaseID",Definition.DatabaseID.Content);
            InsertPeerStatement.BindValue("ParticipantID",Definition.DatabaseID.Content);
            m_State->DB.GetAllRows(InsertPeerStatement);
            InsertPeerStatement.Reset();
        }
        auto InsertForkStatement = m_State->DB.GetSQLStatement("INSERT INTO DatabaseForks(OriginalDB,DBFork) VALUES (:OriginalDB,"
             ":DBFork);");
        for(auto const& Forks : Definition.ForkedDatabase)
        {
            InsertForkStatement.BindValue("OriginalDB",Forks.Content);
            InsertForkStatement.BindValue("DBFork",Definition.DatabaseID.Content);
            m_State->DB.GetAllRows(InsertForkStatement);
            InsertForkStatement.Reset();
        }
    }
    bool ConnectionManager::HasDB(ID const& DatabaseID)
    {
        try
        {
            auto Statement = m_State->DB.GetSQLStatement("SELECT Hash FROM Databases WHERE Hash = :Hash");
            Statement.BindValue("Hash",DatabaseID);
            auto Rows = m_State->DB.GetAllRows(Statement);
            return Rows.size() > 0;
        }
        catch(...)
        {
        }
        return false;
    }
    //void CreateDB(DatabaseDefinition const& Definition)
    //{
    //       
    //}
    ID ConnectionManager::PublishMessage(PublishableResourceHeader PublishedMessage)
    {
        NewMessage NotificationToSend;
        NotificationToSend.Header.Type = PublishedMessage.Type;
        NotificationToSend.Header.UpType = PublishedMessage.UpType;
        NotificationToSend.Header.TimeStamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        NotificationToSend.Header.ContentSize = PublishedMessage.Content.size();
        NotificationToSend.Content = std::move(PublishedMessage.Content);
        NotificationToSend.Header.OriginalDatabaseHash = std::move(PublishedMessage.DatabaseHash);
        NotificationToSend.Header.ParentHash = std::move(PublishedMessage.ParentHash);
        NotificationToSend.Header.Name = std::move(PublishedMessage.Name);
        NotificationToSend.Header.ContentHash = std::move(PublishedMessage.ContentHash);
        NotificationToSend.Header.Uploader = m_State->HostID;
        NotificationToSend.Header.HeaderHash = NotificationToSend.Header.CalculateHeaderHash();

        m_State->AddResourceToDB(NotificationToSend.Header,NotificationToSend.Content);

        std::vector<PeerInfo> DBPeers;
        {
            std::lock_guard Lock(m_State->StateMutex);
            auto PeerIt = m_State->PeerSubscriptions.find(NotificationToSend.Header.OriginalDatabaseHash.Content);
            if(PeerIt != m_State->PeerSubscriptions.end())
            {
                for(auto const& Peer : PeerIt->second)
                {
                    DBPeers.push_back(Peer);
                }
            }
        }
        for(auto const& Peer : DBPeers)
        {
            if(Peer.IP != 0)
            {
                m_State->UDP->SendNotification(Peer,NotificationToSend);
            }
        }
        return NotificationToSend.Header.HeaderHash.Content;
    }
    void ConnectionManager::RemoveResource(ID const& DBID,ID const& ResourceID)
    {
        m_State->RemoveResource(DBID,ResourceID);
    }
    void ConnectionManager::RemoveResource(ID const& DBID,std::vector<std::string> const& Path)
    {
        m_State->RemoveResource(DBID,Path);
    }
    bool ConnectionManager::GetResource(ID const& DBID,ID const& ResourceID,ResourceHeader& OutHeader)
    {
        return m_State->GetResourceById(DBID,ResourceID,OutHeader);
    }



    Task<std::optional<MBParsing::JSONObject>> 
        ConnectionManager::SendPeerRPC(ID const& PeerID, MBParsing::JSONObject ObjectToSend)
    {
        std::optional<MBParsing::JSONObject> ReturnValue;
        Message NewMessage;
        JSONRPC& RPCMessage = NewMessage.Content.GetOrAssign<JSONRPC>();
        NewMessage.Header.Type = MessageType::JSONRPC;
        RPCMessage.Object = std::move(ObjectToSend);

        std::shared_ptr<Connection> PeerConnection = nullptr;
        
        {
            std::lock_guard Lock(m_State->StateMutex);
            auto It = m_State->ActiveConnections.find(PeerID);
            if(It != m_State->ActiveConnections.end())
            {
                PeerConnection = It->second;
            }
        }
        if(PeerConnection == nullptr)
        {
            PeerInfo TargetPeer;
            auto ClosestPeers = m_State->Peers.FindClosest(PeerID,1);
            if(ClosestPeers.size() == 0)
            {
                co_return ReturnValue;
            }
            if(ClosestPeers[0].ID.Content == PeerID)
            {
                TargetPeer = std::move(ClosestPeers[0]);
            }
            else
            {
                auto NewPeers = co_await GetClosestPeers(ClosestPeers[0].ID.Content);
                if(NewPeers.size() > 0 && NewPeers[0].ID.Content == PeerID)
                {
                    TargetPeer = NewPeers[0];
                }
            }
            if(TargetPeer.ID != PeerID)
            {
                co_return ReturnValue;
            }
            auto ConnectionResult = co_await InitializeConnection(TargetPeer);
            if(!ConnectionResult.has_value())
            {
                co_return ReturnValue;
            }
            PeerConnection = ConnectionResult.value();
        }
        if(PeerConnection != nullptr)
        {
            //ReturnValue = (co_await PeerConnection->SendMessage(NewMessage)).second.Content.GetType<JSONRPC>().Object;
            auto Response = (co_await PeerConnection->SendMessage(NewMessage));
            if(Response)
            {
                ReturnValue = std::move(Response.value().second.Content.GetType<JSONRPC>().Object);
            }
        }
        co_return ReturnValue;
    }
    void ConnectionManager::JoinNetwork()
    {
        FindPeerRequest Request;
        Request.HostPeer = m_State->HostInfo;
        Request.PeerID  = m_State->HostInfo.ID;
        Request.k = 20;
        GetClosestPeers(std::move(Request)).resume();
        //m_State->AddTask<FindClosestPeers>(std::move(Request),[](auto const&){});
    }
    Task<bool> ConnectionManager::JoinDB(ID const& DBID)
    {
        //m_State->AddJoinDBTask(DBID);
        auto Task = JoinDBTask(DBID);
        return Task;
    }

    std::vector<PeerInfo> ConnectionManager::SharedState::GetClosestPeers(ID const& ID,int k)
    {
        std::lock_guard Lock(StateMutex);
        return Peers.FindClosest(ID,k);
    }
    std::vector<PeerInfo> ConnectionManager::SharedState::GetClosestPeers(ID const& TargetID,ID const& DatabaseID,int k)
    {
        std::lock_guard Lock(StateMutex);
        auto It = PeerSubscriptions.find(DatabaseID);
        if(It == PeerSubscriptions.end())
        {
            return {};
        }
        else
        {
            return Peers.FindClosest(TargetID,k,[&](PeerInfo const& Peer){return It->second.find(Peer) != It->second.end();});
        }
        return {};
    }
    //PeerInfo ConnectionManager::SharedState::FindPeer(ID const& TargetID)
    //{
    //    //ASSUMPTION: Closest peers is sorted with the first element being closes to TargetID
    //    auto NextRequestPeers = GetClosestPeers(TargetID,3);
    //    if(NextRequestPeers.size() == 0)
    //    {
    //        throw std::runtime_error("Error finding peer: no peers to initiate routing with");
    //    }
    //    MBUtility::WaitContext WaitContext;
    //    std::unordered_map<size_t,MBUtility::Future<FindPeer_Response>> ActiveResponses;
    //    PeerInfo ReturnValue;
    //    size_t CurrentRequestID = 0;
    //    FindPeerRequest MessageToSend;
    //    std::unordered_set<std::string> ContactedPeers;

    //    ID CurrentClosestID = NextRequestPeers[0].ID.Content;
    //    IDSorter Sorter(TargetID);
    //    IDSorter MinHeapSorter(TargetID,false);
    //    std::make_heap(NextRequestPeers.begin(),NextRequestPeers.begin(),MinHeapSorter);
    //    while(NextRequestPeers.size() > 0)
    //    {
    //        for(size_t i = 0; i < std::min(NextRequestPeers.size(),size_t(3));i++)
    //        {
    //            auto const& Peer = NextRequestPeers[0];
    //            ActiveResponses[CurrentRequestID] = UDP.SendOneshotRequest(Peer,MessageToSend);
    //            WaitContext.AddFuture(CurrentRequestID,ActiveResponses[CurrentRequestID]);
    //            ContactedPeers.insert(IDToString(Peer.ID.Content));
    //            CurrentRequestID++;
    //            std::pop_heap(NextRequestPeers.begin(),NextRequestPeers.end(),MinHeapSorter);
    //            NextRequestPeers.resize(NextRequestPeers.size()-1);
    //        }
    //        size_t NextResponse = WaitContext.GetNextUpdate(std::chrono::seconds(5));
    //        if(NextResponse == -1)
    //        {
    //            throw std::runtime_error("Error finding peer: no responses from routing peers");
    //        }
    //        auto It = ActiveResponses.find(NextResponse);
    //        assert(It != ActiveResponses.end());
    //        if(!It->second.ValueAvailable())
    //        {
    //            ActiveResponses.erase(It);
    //            continue;
    //        }
    //        auto Response = It->second.Get();

    //        if(Response.Peers.size()  > 0)
    //        {
    //            std::sort(Response.Peers.begin(),Response.Peers.end(),Sorter);
    //            if(Response.Peers.front().ID.Content == TargetID)
    //            {
    //                return Response.Peers.front();
    //            }
    //            for(auto const& Peer : Response.Peers)
    //            {
    //                if(!Sorter(Peer.ID.Content,Response.HostID.Content) || Sorter(CurrentClosestID,Peer.ID.Content))
    //                {
    //                    break;
    //                }

    //                CurrentClosestID = Peer.ID.Content;

    //                auto StringID = IDToString(Peer.ID.Content);
    //                if(ContactedPeers.find(StringID) == ContactedPeers.end())
    //                {
    //                    ContactedPeers.insert(std::move(StringID));
    //                    NextRequestPeers.push_back(Peer);
    //                    std::push_heap(NextRequestPeers.begin(),NextRequestPeers.end(),MinHeapSorter);
    //                }
    //            }
    //        }
    //        ActiveResponses.erase(It);
    //    }
    //    return  ReturnValue;
    //}
    void ConnectionManager::SharedState::NotificationHandler(MessageLocation Location,UDPNotification const& Notification)
    {
        if(Notification.IsType<NewMessage>())
        {
            auto const& MessageNotification = Notification.GetType<NewMessage>();
            if(!SubscribedToDB(MessageNotification.Header.OriginalDatabaseHash))
            {
                return;
            }
            {
                //TODO Verify content
                if(MessageNotification.Content.size() == 0 || !ResourceInDB(MessageNotification.Header.HeaderHash))
                {
                    //AddTask<SyncDBTask>(Location,MessageNotification.Header);
                    AddResourceToDB(MessageNotification.Header,MessageNotification.Content);
                    ResourceRecievedCallback(MessageNotification);
                }
                else
                {
                    if(!ResourceInDB(MessageNotification.Header.HeaderHash))
                    {
                        AddResourceToDB(MessageNotification.Header,MessageNotification.Content);
                        //TODO how should message publication be handled...
                        auto It = PeerSubscriptions.find(MessageNotification.Header.OriginalDatabaseHash.Content);
                        if(It != PeerSubscriptions.end())
                        {
                            for(auto const& Peer :  It->second)
                            {
                                UDP->SendNotification(Peer,MessageNotification);
                            }
                        }
                        ResourceRecievedCallback(MessageNotification);
                    }
                }
            }
        }
        else if(Notification.IsType<DBSubscriber>())
        {
            auto const& SubscriptionNotification = Notification.GetType<DBSubscriber>();
            AddPeerSubscriptions(SubscriptionNotification.Peer,SubscriptionNotification.SubscribedDatabases);
        }
    }
    UDPResponse ConnectionManager::SharedState::RequestHandler(MessageLocation Location,UDPRequest const& Request)
    {
        UDPResponse Response;
        //if(Request.IsType<GetDBPeers>())
        //{
        //    auto const& PeerRequest = Request.GetType<GetDBPeers>();
        //    auto& PeerResponse = Response.GetOrAssign<GetDBPeers_Response>();
        //    {
        //        std::lock_guard InternalsLock(StateMutex);
        //        auto It = PeerSubscriptions.find(IDToString(PeerRequest.DBID.Content));
        //        if(It != PeerSubscriptions.end())
        //        {
        //            PeerResponse.Peers.insert(PeerResponse.Peers.end(),
        //                    It->second.begin(),It->second.begin() + std::min(It->second.size(),size_t(PeerRequest.k)));
        //        }
        //    }
        //}
        if(Request.IsType<FindPeerRequest>())
        {
            auto const& PeerRequest = Request.GetType<FindPeerRequest>();
            auto& PeerResponse = Response.GetOrAssign<FindPeer_Response>();
            if(PeerRequest.DBID)
            {
                PeerResponse.Peers = GetClosestPeers(PeerRequest.PeerID.Content,PeerRequest.DBID.Value().Content,std::min(PeerRequest.k,20));
            }
            else
            {
                PeerResponse.Peers = GetClosestPeers(PeerRequest.PeerID.Content,std::min(PeerRequest.k,20));
            }
            {
                std::lock_guard InternalsLock(StateMutex);
                PeerResponse.HostID = HostID;
            }
        }
        else if(Request.IsType<GetPeerKey>())
        {
            auto const& KeyRequest = Request.GetType<GetPeerKey>();
            GetPeerKey_Response& KeyResponse = Response.GetOrAssign<GetPeerKey_Response>();
            auto Statement = DB.GetSQLStatement("SELECT Key FROM Peers WHERE ID = :Hash");
            Statement.BindBlob("Hash",KeyRequest.PeerID.Content);
            auto Result = DB.GetAllRows(Statement);
            if(Result.size() > 0)
            {
                KeyResponse.PeerKey.Content = Result[0].GetColumnData<std::string>(0);
            }
        }
        else if(Request.IsType<InitConnection>())
        {
            auto const& ConnectionRequset = Request.GetType<InitConnection>();
            ConnectionParameters Params;
            Params.IP = Location.IP;
            Params.PeerPort = ConnectionRequset.HostPort;
            Params.LocalPort = 1338;
            Params.PeerID.Content = IDToString(ConnectionRequset.HostInfo.ID.Content);

            auto& ConnectionResponse = Response.GetOrAssign<InitConnection_Response>();
            ConnectionResponse.Accepted = true;
            ConnectionResponse.HostPort = 1338;
            Params.PeerRegularPort = Location.Port;

            {
                std::lock_guard StateLock = std::lock_guard(StateMutex);
                auto It = ActiveConnections.find(ConnectionRequset.HostInfo.ID.Content);
                if(It == ActiveConnections.end())
                {
                    std::unique_ptr<MBSockets::UDPSocket> UDPStream = std::make_unique<MBSockets::UDPSocket>(Params.IP,Params.PeerPort,0);
                    Params.LocalPort = UDPStream->GetBoundPort();
                    ConnectionResponse.HostPort = UDPStream->GetBoundPort();
                    PeerInfo Peer;
                    Peer.ListeningPort = Params.PeerRegularPort;
                    Peer.ID = ConnectionRequset.HostInfo.ID.Content;
                    Peer.IP = Params.IP;
                    auto NewConnection = std::make_shared<Connection>(std::unique_ptr<MBUtility::BidirectionalPacketStream>(std::move(UDPStream)),Params  ,std::move(Peer),GetTCPMessageHandler(),
                            [LocalPort=Params.LocalPort,ID=ConnectionRequset.HostInfo.ID.Content,ThisTask=shared_from_this()](ConnectionParameters const& Params) mutable {
                                std::lock_guard Lock(ThisTask->StateMutex);
                                ThisTask->PortMapper.RemovePortMapping(LocalPort);
                                ThisTask->ActiveConnections.erase(ThisTask->ActiveConnections.find(ID));
                            });
                    PortMapper.AddPortMapping(Params.LocalPort);
                    ActiveConnections[ConnectionRequset.HostInfo.ID.Content] = NewConnection;
                }
                else
                {
                    ConnectionResponse.HostPort = It->second->GetLocalPort();
                }
            }
        }
        return Response;
    }
    
    void ConnectionManager::SharedState::StoreContent(ResourceHeader const& Header,std::string const& Content)
    {
        
    }
    void ConnectionManager::SharedState::AddResourceToDB(ResourceHeader const& Header,std::string const& Content)
    {
        auto InDBStmt = DB.GetSQLStatement(
                "SELECT StoredLocaly,StoredInline FROM Resources "
                " WHERE Hash =:Hash AND DatabaseHash = :DBHash");
        InDBStmt.BindBlob("Hash",Header.HeaderHash.Content);
        InDBStmt.BindBlob("DBHash",Header.OriginalDatabaseHash.Content);
        auto Resources = DB.GetAllRows(InDBStmt);
        if(Resources.size() > 0)
        {
            auto const& Row = Resources.front();
            auto StoredInline = std::get<MBDB::IntType>(Row["StoredInline"]) != 0;
            auto StoredLocaly = std::get<MBDB::IntType>(Row["StoredLocaly"]) != 0;
            if(!StoredLocaly)
            {
                //always store inline...
                auto StoreStmt = DB.GetSQLStatement(
                        "UPDATE Resources SET StoredLocaly = 1,StoredInlien = 1,Content=:Content WHERE Hash = :Hash AND DatabaseHash = :DBHash");
                StoreStmt.BindBlob("Hash",Header.HeaderHash.Content);
                StoreStmt.BindBlob("DBHash",Header.OriginalDatabaseHash.Content);
                StoreStmt.BindBlob("Content",Content);
                DB.GetAllRows(StoreStmt);
            }
            return;
        }

        auto Stmt = DB.GetSQLStatement(
                "INSERT INTO Resources(Hash,ContentType,UpType,Time,ContentSize,DatabaseHash,ParentHash,ContentHash, "
            "UploaderID,Signature,StoredLocaly,StoredInline,Content,RecievedTimestamp,Name) VALUES (:Hash,:ContentType,:UpType,:Timestamp,:ContentSize,:DatabaseHash, "
            ":ParentHash,:ContentHash,:UploaderID,:Signature,:StoredLocaly,:StoredInline,:Content,:LocalTimestamp,:Name)");
        //stmt.bind
        Stmt.BindBlob("Hash",Header.HeaderHash.Content);
        Stmt.BindValue("ContentType",Header.Type);
        Stmt.BindValue("Name",Header.Name);
        Stmt.BindValue("UpType",Header.UpType);
        Stmt.BindValue("Timestamp",Header.TimeStamp);
        Stmt.BindValue("ContentSize",Header.ContentSize);
        Stmt.BindBlob("DatabaseHash",Header.OriginalDatabaseHash.Content);
        Stmt.BindBlob("ParentHash",Header.ParentHash.Content);
        Stmt.BindBlob("ContentHash",Header.ContentHash.Content);
        Stmt.BindBlob("UploaderID",Header.Uploader.Content);
        Stmt.BindBlob("Signature",Header.Sig.Content);
        //
        if(Header.ContentSize == Content.size())
        {
            Stmt.BindValue("StoredLocaly",1);
            Stmt.BindValue("StoredInline",1);
            Stmt.BindValue("Content",Content);
        }
        else
        {
            Stmt.BindValue("StoredLocaly",0);
            Stmt.BindValue("StoredInline",0);
            Stmt.BindNull("Content");
        }
        Stmt.BindInt("LocalTimestamp",std::chrono::steady_clock::now().time_since_epoch().count());
        auto Result = DB.GetAllRows(Stmt);


        //update tree
        MBDB::IntType ParentID = -1;
        MBDB::IntType RowID = -1;
        auto Path = GetAbsoluteResourcePath(Header.OriginalDatabaseHash.Content,Header.HeaderHash.Content,ParentID,RowID);
        if(RowID == -1)
        {
            auto InsertStatement = DB.GetSQLStatement(
                    " INSERT INTO ActiveTree(ID,Name,ParentID,ResourceID,DatabaseID) "
                    " VALUES(NULL,:Name,:ParentID,:ResourceID,:DatabaseID); ");
            InsertStatement.BindValue("Name",Header.Name);
            InsertStatement.BindValue("ParentID",ParentID);
            InsertStatement.BindValue("ResourceID",Header.HeaderHash.Content);
            InsertStatement.BindValue("DatabaseID",Header.OriginalDatabaseHash.Content);
            DB.GetAllRows(InsertStatement);
        }
        else 
        {
            auto InsertStatement = DB.GetSQLStatement(
                    "UPDATE ActiveTree SET ResourceID = :ResourceID WHERE ID = :ID;");
            InsertStatement.BindValue("ResourceID",Header.HeaderHash.Content);
            InsertStatement.BindValue("ID",RowID);
            DB.GetAllRows(InsertStatement);
        }
    }
    void ConnectionManager::SharedState::RemoveResource(ID const& DBID,ID const& ResourceID)
    {
        MBDB::IntType ParentID = -1;
        MBDB::IntType RowID = -1;
        auto Path = GetAbsoluteResourcePath(DBID,ResourceID,ParentID,RowID);
        if(RowID != -1)
        {
            auto InsertStatement = DB.GetSQLStatement(
                    " DELETE FROM ActiveTree WHERE ID = :ID ");
            InsertStatement.BindValue("ID",RowID);
            DB.GetAllRows(InsertStatement);
        }
    }
    void ConnectionManager::SharedState::RemoveResource(ID const& DBID,std::vector<std::string> const& Path)
    {
        MBDB::IntType ParentID = -1;
        MBDB::IntType RowID = -1;
        std::vector<std::string> Reversed;
        Reversed.insert(Reversed.end(),Path.rbegin(),Path.rend());
        GetPathID(DBID,Reversed,ParentID,RowID);
        if(RowID != -1)
        {
            auto InsertStatement = DB.GetSQLStatement(
                    " DELETE FROM ActiveTree WHERE ID = :ID ");
            InsertStatement.BindValue("ID",RowID);
            DB.GetAllRows(InsertStatement);
        }
    }


    bool ConnectionManager::SharedState::ResourceInDB(Hash const& Resource)
    {
        bool ReturnValue = true;
        auto stmt = DB.GetSQLStatement("SELECT Hash FROM Resources WHERE Hash = :Hash");
        stmt.BindValue("Hash",Resource.Content);
        auto Result = DB.GetAllRows(stmt);
        return Result.size() > 0;
    }
    //void ConnectionManager::SharedState::AddPeerSubscriptions(PeerInfo const& Peer,std::vector<Hash> const& Subscriptions)
    //{
    //       
    //}
    bool ConnectionManager::SharedState::SubscribedToDB(Hash const& DBID)
    {
        std::lock_guard Lock(StateMutex);
        bool ReturnValue = true;
        auto It = PeerSubscriptions.find(DBID.Content);
        if(It == PeerSubscriptions.end())
        {
            return false;
        }
        auto PeerIt = It->second.find(HostInfo);
        if(PeerIt == It->second.end())
        {
            return false;   
        }
        return ReturnValue;
    }
    void ConnectionManager::SharedState::AddSubscription(ID const& DatabaseID,PeerInfo const& PeerInfo)
    {
        {
            std::lock_guard Lock(StateMutex);
            PeerSubscriptions[DatabaseID].insert(PeerInfo);
        }
        auto Stmt = DB.GetSQLStatement("INSERT INTO Subscriptions(PeerID,DatabaseID) VALUES (:PeerID,:DatabaseID)");
        Stmt.BindBlob("PeerID",PeerInfo.ID.Content);
        Stmt.BindBlob("DatabaseID",DatabaseID);
        DB.GetAllRows(Stmt);
    }
    void ConnectionManager::SharedState::AddPeerSubscriptions(PeerInfo const& Peer,std::vector<Hash> const& Subscriptions)
    {
        for(auto const& Subscription : Subscriptions)
        {
            AddSubscription(Subscription.Content,Peer);
        }
    }
    bool ConnectionManager::SharedState::GetResourceById(ID const& DatabaseID,ID const& ResourceID,ResourceHeader& OutHeader)
    {
        bool Result = false;
        auto Stmt = DB.GetSQLStatement("SELECT Name,ContentType,UpType,Time,ContentSize,ParentHash,ContentHash,UploaderID,Signature FROM Resources WHERE DatabaseHash = :DBHash AND Hash = :ID");
        Stmt.BindBlob("DBHash",DatabaseID);
        Stmt.BindBlob("ID",ResourceID);
        auto Rows = DB.GetAllRows(Stmt);
        if(Rows.size() > 1)
        {
            throw std::runtime_error("Database invariant broken: multiple rows with the same hash and database ID");
        }
        for(auto const& Row : Rows)
        {
            OutHeader.HeaderHash = ResourceID;
            OutHeader.Type = std::get<std::string>(Row["ContentType"]);
            OutHeader.UpType = UploadType(std::get<MBDB::IntType>(Row["UpType"]));
            OutHeader.TimeStamp = std::get<MBDB::IntType>(Row["Time"]);
            OutHeader.ContentSize = std::get<MBDB::IntType>(Row["ContentSize"]);
            OutHeader.OriginalDatabaseHash = DatabaseID;
            OutHeader.ParentHash.Content = StringToID(std::get<std::string>(Row["ParentHash"]));
            OutHeader.Name = std::get<std::string>(Row["Name"]);
            OutHeader.ContentHash.Content = StringToID(std::get<std::string>(Row["ContentHash"]));
            OutHeader.Uploader.Content = StringToID(std::get<std::string>(Row["UploaderID"]));
            OutHeader.Sig.Content = std::get<std::string>(Row["Signature"]);


            Result = true;
            break;
        }
        return Result;
    }
    MessageCallback ConnectionManager::SharedState::GetTCPMessageHandler()
    {
        return [State=shared_from_this()](PeerInfo const& Peer,Message const& Message)
        {
            if(Message.Content.IsType<GetResources>())
            {
                auto const& Request = Message.Content.GetType<GetResources>();
                std::lock_guard Lock(State->StateMutex);
                auto It = State->ActiveConnections.find(Peer.ID.Content);
                if(It != State->ActiveConnections.end())
                {
                    It->second->SendStreamedResponse(Message.Header,MessageType::RPC,
                            [State=State,DBID= Request.DBHash,Start=Request.StartTime,
                                End=Request.EndTime,MaxInlineSize=Request.MaxInlineSize](MBUtility::MBOctetOutputStream& OutStream) mutable
                            {
                                constexpr size_t MaxTransferSize = 10000000;
                                //TODO make into a true row iterator instead
                                auto Stmt = State->DB.GetSQLStatement("SELECT * FROM Resources WHERE DatabaseHash = :Hash "
                                        " AND Time BETWEEN :Start AND :End  ORDER BY TIME ASC");
                                Stmt.BindBlob("Hash",DBID.Content);
                                Stmt.BindInt("Start",Start);
                                Stmt.BindInt("End",End);
                                auto Rows = State->DB.GetAllRows(Stmt);
                                for(auto const& Row : Rows)
                                {
                                    ResourceResponse Content;
                                    Content.Header.HeaderHash = StringToID(std::get<std::string>(Row["Hash"]));
                                    Content.Header.Type =  std::get<int64_t>(Row["ContentType"]);
                                    Content.Header.UpType = UploadType( std::get<int64_t>(Row["UpType"]));
                                    Content.Header.TimeStamp =  std::get<int64_t>(Row["Timestamp"]);
                                    Content.Header.ContentSize =  std::get<int64_t>(Row["ContentSize"]);
                                    Content.Header.OriginalDatabaseHash = StringToID(std::get<std::string>(Row["DatabaseHash"]));
                                    Content.Header.ContentHash = StringToID(std::get<std::string>(Row["ContentHash"]));
                                    Content.Header.Uploader.Content = StringToID(std::get<std::string>(Row["UploaderID"]));
                                    Content.Header.Sig.Content = std::get<std::string>(Row["Signature"]);

                                    //arbitrary number
                                    if(std::get<int64_t>(Row["StoredLocaly"]) && 
                                            std::get<int64_t>(Row["StoredInline"]) && Content.Header.ContentSize < MaxInlineSize && Content.Header.ContentSize < MaxTransferSize)
                                    {
                                        Content.Content = std::get<std::string>(Row["Content"]);
                                    }
                                    
                                    std::string OutString;
                                    MBUtility::MBStringOutputStream StringStream(OutString);
                                    Parse(StringStream,Content.Header);
                                    StringStream & Content.Content;
                                    OutStream & uint16_t(OutString.size());
                                    OutStream << OutString;
                                }
                                OutStream & uint16_t(0);
                            }
                    );
                }
            }
            else if(Message.Content.IsType<JSONRPC>())
            {
                MBUtility::Promise<MBParsing::JSONObject> Promise;
                auto Future = Promise.GetFuture();
                Future.Then( [State=State,Peer = Peer,Header = Message.Header](MBParsing::JSONObject const& Response)
                        {
                            std::lock_guard Lock(State->StateMutex);
                            auto It = State->ActiveConnections.find(Peer.ID.Content);
                            if(It != State->ActiveConnections.end())
                            {
                                struct Message JSONResponse;
                                auto& Content = JSONResponse.Content.GetOrAssign<JSONRPC>();
                                Content.Object = Response;
                                It->second->SendResponse(Header,std::move(JSONResponse));
                            }
                        });
                State->RPCCallback(Peer,Message.Content.GetType<JSONRPC>().Object,std::move(Promise));
            }
        };
    }
    ConnectionManager::ConnectionManager(IDParameters Params,uint16_t ListenPort,std::string const& DatabasePath,ResourceCallback Callback,RPCHandler RPCCallback)
    {
        m_State = std::make_shared<SharedState>(DatabasePath);
        m_State->ResourceRecievedCallback = std::move(Callback);
        m_State->RPCCallback = std::move(RPCCallback);
        m_State->HostID.Content = Params.LocalID;
        m_State->HostInfo.ID = Params.LocalID;

        //Retrieve all subscriptions
        auto Subscriptions = m_State->DB.GetAllRows(
                "SELECT DatabaseID,PeerID,IP,Port FROM Subscriptions LEFT JOIN Peers "
                " ON PeerID=ID");
        m_State->PortMapper.AddPortMapping(ListenPort);
        for(auto& Row : Subscriptions)
        {
            PeerInfo NewInfo;
            NewInfo.ID = StringToID(std::get<std::string>(Row["PeerID"]));
            if(std::holds_alternative<MBDB::IntType>(Row["Port"]))
            {
                NewInfo.ListeningPort = std::get<MBDB::IntType>(Row["Port"]);
            }
            if(std::holds_alternative<MBDB::IntType>(Row["IP"]))
            {
                NewInfo.IP = std::get<MBDB::IntType>(Row["IP"]);
            }
            m_State->PeerSubscriptions[StringToID(std::get<std::string>(Row["DatabaseID"]))].insert(std::move(NewInfo));
        }
        auto WeakStatePtr = std::weak_ptr<SharedState>(m_State);
        m_State->UDP = std::make_unique<UDPHandler>(ListenPort,[This=WeakStatePtr](MessageLocation Location,UDPRequest const& Request)
                {
                    auto Shared = This.lock();
                    if(Shared != nullptr)
                    {
                        return Shared->RequestHandler(Location,Request);
                    }
                    return UDPResponse();
                },
                [This=WeakStatePtr](MessageLocation Location,UDPNotification const& Request)
                {
                    auto Shared = This.lock();
                    if(Shared != nullptr)
                    {
                        return Shared->NotificationHandler(Location,Request);
                    }
                });
    }



    //

    AsyncUDPMapper::AsyncUDPMapper()
    {
        m_State = std::make_shared<SharedState>();
        m_State->NotifyThread = std::thread(p_NotifyThread,m_State);
    }
    void AsyncUDPMapper::p_NotifyThread(std::shared_ptr<SharedState> State)
    {
        try{
            uint32_t LocalIP = 0;
            {
                //arbitrary IP, just need to be connected in order to get local interface IP
                MBSockets::UDPSocket NewSocket(MBSockets::StringToIP("8.8.8.8"),1337,1337);
                NewSocket.Connect();
                LocalIP = NewSocket.GetLocalIP();
            }
            auto Discoverer = std::make_shared<MBUPNP::UDPDiscoverer>();
            auto Devices = Discoverer->FindDevices(5);
            MBUPNP::Device WanDevice;
            for(auto const& Device : Devices)
            {
                for(auto const& Service : Device.Services)
                {
                    if(Service.Info.ServiceName == "WANIPConnection")
                    {
                        //IPService = std::make_unique<MBUPNP::WANIPConnectionService>(Device);
                        WanDevice = Device;
                        break;
                    }
                }
                if(WanDevice.DeviceType != "")
                {
                    break;   
                }
            }
            if(WanDevice.DeviceType == "")
            {
                //do some error handling...
                //
                State->Stopping.store(true);
                return;
            }
            MBUtility::Async<MBUPNP::WANIPConnectionService> IPService(WanDevice);
            std::vector<Mapping> ActiveMappings;
            std::unordered_set<uint16_t> RemovedMappings;
            std::unordered_set<uint16_t> MappedPorts;
            double SleepDuration = 10;
            double LeaseDuration = 120;
            double LeaseMargin = 10;

            auto Heapify = [](std::vector<Mapping>& Target)
            {
                std::make_heap(Target.begin(),Target.end(),std::greater<Mapping>());
            };
            auto Pop = [](std::vector<Mapping>& Target)
            {
                if(Target.size() > 0)
                {
                    std::pop_heap(Target.begin(),Target.end(),std::greater<Mapping>());
                    Target.resize(Target.size()-1);
                }
            };
            while(!State->Stopping.load())
            {
                std::unique_lock Lock(State->InternalsMutex);
                State->NewMappingConditional.wait_for(Lock,std::chrono::milliseconds(
                            int(SleepDuration*1000)));
                if(State->Stopping.load())
                {
                    break;
                }
                for(auto NewMapping : State->RequestedMappings)
                {
                    //uint32_t ExternalHost,uint16_t ExternalPort,Protocol Protocol,uint16_t InternalPort,uint32_t InternalClient,bool Enabled,std::string const& Description,int Duration);
                    if(MappedPorts.find(NewMapping) == MappedPorts.end())
                    {
                        IPService.AddTask(&MBUPNP::WANIPConnectionService::AddPortMapping,0,NewMapping,MBUPNP::Protocol::UDP,NewMapping,LocalIP,true,"MBChat2 - direct connection",LeaseDuration);
                        Mapping& CurrentMapping = ActiveMappings.emplace_back();
                        CurrentMapping.MappedPort = NewMapping;
                        CurrentMapping.LastMapped = std::chrono::steady_clock::now();
                        CurrentMapping.LeaseDuration = LeaseDuration;
                        MappedPorts.insert(NewMapping);
                        Heapify(ActiveMappings);
                    }
                }
                State->RequestedMappings.clear();
                for(auto Mapping : State->RequestedRemovals)
                {
                    if(MappedPorts.find(Mapping) == MappedPorts.end())
                    {
                        RemovedMappings.insert(Mapping);
                    }
                }
                State->RequestedRemovals.clear();
                auto Now = std::chrono::steady_clock::now();
                while(ActiveMappings.size() > 0)
                {
                    auto& CurrentMapping = ActiveMappings.front();
                    if(RemovedMappings.find(CurrentMapping.MappedPort) != RemovedMappings.end())
                    {
                        RemovedMappings.erase(CurrentMapping.MappedPort);
                        MappedPorts.erase(CurrentMapping.MappedPort);
                        Pop(ActiveMappings);
                        continue;
                    }
                    double ElapsedTime = std::chrono::duration_cast<std::chrono::seconds>(Now-CurrentMapping.LastMapped).count();
                    if(ElapsedTime + LeaseMargin >= CurrentMapping.LeaseDuration)
                    {
                        IPService.AddTask(&MBUPNP::WANIPConnectionService::AddPortMapping,0,CurrentMapping.MappedPort,MBUPNP::Protocol::UDP,CurrentMapping.MappedPort,LocalIP,true,"MBChat2 - direct connection",LeaseDuration);
                        CurrentMapping.LastMapped = Now;
                        Heapify(ActiveMappings);
                    }
                    else
                    {
                        SleepDuration = CurrentMapping.LeaseDuration-(ElapsedTime+LeaseMargin);
                        break;
                    }
                }
                if(ActiveMappings.size() == 0)
                {
                    SleepDuration = 0;
                }
            }
        }
        catch(std::exception const& e)
        {
               
        }
        State->Stopping.store(true);
    }
    bool AsyncUDPMapper::AddPortMapping(uint16_t Port)
    {
        std::lock_guard Lock(m_State->InternalsMutex);
        m_State->RequestedMappings.push_back(Port);
        m_State->NewMappingConditional.notify_one();
        return true;
    }
    bool AsyncUDPMapper::RemovePortMapping(uint16_t Port)
    {
        std::lock_guard Lock(m_State->InternalsMutex);
        m_State->RequestedRemovals.push_back(Port);
        m_State->NewMappingConditional.notify_one();
        return true;
    }
}
