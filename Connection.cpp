#include "Connection.h"

#include <MrBoboSockets/MrBoboSockets.h>

#include <unordered_set>

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
            while(!State->Stopping.load())
            {
                Message NewMessage;
                NewMessage.ReadMessageHeader(State->Transport);
                if(!p_VerifyAuthenticity(*State,NewMessage))
                {
                    //message silently dropped, should reduce trust for peer or something
                    continue;
                }
                ResponseHandler Handler;
                {
                    std::lock_guard Lock(State->SendMutex);
                    auto It = State->MessageCallbacks.find(NewMessage.ResponseID);
                    if(It != State->MessageCallbacks.end())
                    {
                        Handler = std::move(It->second);
                        State->MessageCallbacks.erase(It);
                    }
                    else
                    {
                        Handler = State->GenericMessageHandler;
                    }
                }
                if(Handler.IsType<MessageCallback>())
                {
                    NewMessage.ReadBody(State->Transport);
                    Handler.GetType<MessageCallback>()(NewMessage);
                }
                else if(Handler.IsType<StreamedHandler>())
                {
                    auto& Callback= Handler.GetType<StreamedHandler>();
                    while(Callback(State->Transport)) {

                    }
                }
            }
        }
        catch(...)
        {
            
        }
        State->Stopping.store(true);
        State->Transport.Abort();
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
                Message MessageToSend;
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
                    auto NewMessage = std::move(State->QueuedMessages.front());
                    NewMessage.MessageToSend.MessageID = State->NextSendID;
                    State->NextSendID.fetch_add(1);
                    State->MessageCallbacks[NewMessage.MessageToSend.MessageID] = std::move(NewMessage.ResponseCallback);
                    MessageToSend = std::move(NewMessage.MessageToSend);
                }
                MessageToSend.WriteMessage(State->Transport);
            }
        }
        catch(...)
        {
        }
        State->Stopping.store(true);
        State->Transport.Abort();
    }
    Connection::~Connection()
    {
        m_SharedState->Stopping.store(true);
        m_SharedState->Transport.Abort();

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
    Connection::Connection(ConnectionParameters Peer,MessageCallback MessageHandler,std::function<void(ConnectionParameters const&)> QuitHandler)
    {
        std::unique_ptr<MBSockets::UDPSocket> UDPStream =  std::make_unique<MBSockets::UDPSocket>(Peer.IP,Peer.PeerPort,Peer.LocalPort);
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
    ID Distance(ID const& Lhs,ID const& Rhs)
    {
        assert(Lhs.size() == Rhs.size());
        ID ReturnValue = ID(Lhs.size(),0);
        for(size_t i = 0; i < Lhs.size();i++)
        {
            ReturnValue[i] = Lhs[i] ^ Rhs[i];
        }
        return  ReturnValue;
    }
    std::string IDToString(ID const& IDToConvert)
    {
        std::string ReturnValue(IDToConvert.size(),0);
        std::memcpy(ReturnValue.data(),IDToConvert.data(),IDToConvert.size());

        return  ReturnValue;
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
        p_FillClosest(m_RootNode,ReturnValue,Key,k,[](PeerInfo const& Peer){return true;});
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
        if(Left != nullptr && Content.size() < k)
        {
            Content.push_back(NewPeer);
            return;
        }
        if(Left != nullptr)
        {
            Split();
        }
        if(NewPeer.ID.Content < Right->CommonPrefix)
        {
            Right->AddPeer(NewPeer);
        }
        else
        {
            Left->AddPeer(NewPeer);
        }
    }
    //
    void ConnectionManager::AddConnections(std::vector<PeerInfo> const& Peers)
    {
        std::lock_guard StateLock(m_State->StateMutex);
        for(auto const& Peer : Peers)
        {
            m_State->Peers.AddPeer(Peer);
        }
    }
    void ConnectionManager::PublishMessage(PublishableResourceHeader const& Message)
    {
        

    }
    void ConnectionManager::JoinNetwork()
    {
        FindPeerRequest Request;
        Request.HostPeer = m_State->HostInfo;
        Request.PeerID  = m_State->HostInfo.ID;
        Request.k = 20;
        m_State->AddTask<FindClosestPeers>(std::move(Request),[](auto const&){});
    }
    void ConnectionManager::JoinDB(ID const& DBID)
    {
        m_State->AddJoinDBTask(DBID);
    }

    std::vector<PeerInfo> ConnectionManager::SharedState::GetClosestPeers(ID const& ID,int k)
    {
        std::lock_guard Lock(StateMutex);
        return Peers.FindClosest(ID,k);
    }
    std::vector<PeerInfo> ConnectionManager::SharedState::GetClosestPeers(ID const& TargetID,ID const& DatabaseID,int k)
    {
        std::lock_guard Lock(StateMutex);
        auto It = PeerSubscriptions.find(IDToString(TargetID));
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
                if(MessageNotification.Content.size() == 0 || !ResourceInDB(MessageNotification.Header.ParentHash))
                {
                    //AddTask<SyncDBTask>(Location,MessageNotification.Header);
                    AddResourceToDB(MessageNotification.Header,MessageNotification.Content);
                }
                else
                {
                    if(!ResourceInDB(MessageNotification.Header.HeaderHash))
                    {
                        AddResourceToDB(MessageNotification.Header,MessageNotification.Content);
                        //TODO how should message publication be handled...
                        auto It = PeerSubscriptions.find(IDToString(MessageNotification.Header.OriginalDatabaseHash.Content));
                        if(It != PeerSubscriptions.end())
                        {
                            for(auto const& Peer :  It->second)
                            {
                                UDP.SendNotification(Peer,MessageNotification);
                            }
                        }
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
    UDPResponse ConnectionManager::SharedState::RequestHandler(UDPRequest const& Request)
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
            auto Statement = DB.GetSQLStatement("SELECT Key FROM Peers WHERE ID = ?");
            Statement.BindString(IDToString(KeyRequest.PeerID.Content),1);
            auto Result = DB.GetAllRows(Statement);
            if(Result.size() > 0)
            {
                KeyResponse.PeerKey.Content = Result[0].GetColumnData<std::string>(0);
            }
        }
        return Response;
    }
    void ConnectionManager::SharedState::AddResourceToDB(ResourceHeader const& Header,std::string const& Content)
    {
        auto Stmt = DB.GetSQLStatement(
                "INSERT INTO Resources(Hash,ContentType,UpType,Timestamp,ContentSize,DatabaseHash,ParentHash,ContentHash, "
            "UploaderID,Signature,StoredLocaly,StoredInline,Content) VALUES (:Hash,:ContentType,:UpType,:Timestamp,:ContentSize,:DatabaseHash, "
            ":ParentHash,:ContentHash,:UploaderID,:Signature,:StoredLocaly,:StoredInline,:Content)");
        //stmt.bind
        Stmt.BindValue("Hash",Header.ContentHash.Content);
        Stmt.BindValue("ContentType",Header.ContentType);
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
        auto Result = DB.GetAllRows(Stmt);

        if(ResourceRecievedCallback && ResourceInDB(Header.HeaderHash) )
        {
            ResourceRecievedCallback(Header);
        }
    }
    bool ConnectionManager::SharedState::ResourceInDB(Hash const& Resource)
    {
        bool ReturnValue = true;
        auto stmt = DB.GetSQLStatement("SELECT Hash FROM Resources WHERE Hash = ?");
        stmt.BindBlob(Resource.Content,1);
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
        auto It = PeerSubscriptions.find(IDToString(DBID.Content));
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
    void ConnectionManager::SharedState::AddJoinDBTask(ID const& DBID)
    {
        FindPeerRequest Request;
        std::lock_guard  Lock(StateMutex);
        Request.HostPeer = HostInfo;
        Request.PeerID  = HostInfo.ID;
        Request.k = 20;
        Request.DBID = Hash();
        Request.DBID.Value().Content = DBID;
        AddTask<FindClosestPeers>(std::move(Request),DBID,
                [State=shared_from_this(),ID=DBID](std::vector<PeerInfo> const& Peers){ State->AddTask<SyncDBTask>(Peers,ID); });
    }
    //std::vector<PeerInfo> ConnectionManager::SharedState::FindDBPeers(ID const&,int k)
    //{
    //    std::vector<PeerInfo> ReturnValue;


    //    return ReturnValue;
    //}
    //void ConnectionManager::SharedState::PopulatePeersForDB(ID const& Database)
    //{
    //    auto DBPeers = FindDBPeers(Database,20);
    //    DBSubscriber Notification;
    //    {
    //        std::lock_guard InternalsLock(StateMutex);
    //        for(auto const& Peer : DBPeers)
    //        {
    //            Peers.AddPeer(Peer);
    //        }
    //        Notification.Peer = HostInfo;
    //        for(auto const& DB : Subscriptions)
    //        {
    //            Notification.SubscribedDatabases.push_back(DB.DatabaseID);
    //        }
    //    }
    //    //advertise existence, and display subscriptions
    //    for(auto const& Peer : DBPeers)
    //    {
    //        UDP.SendNotification(Peer,Notification);
    //    }
    //}
}
