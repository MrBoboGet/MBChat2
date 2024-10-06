#include "Client.h"

#include <MBTUI/BufferWindow.h>

#include "DBVisualiser.h"

#include <MBSystem/MBSystem.h>

#include <MBUtility/MBFiles.h>

#include "Config.h"

namespace MBChat2
{
    //
    MBCLI::Window* Client::DBWindow::p_GetActiveWindow()
    {
        if(m_AssociatedClient->m_VisualisedDB.empty())
        {
            return nullptr;   
        }
        auto It = m_AssociatedClient->m_ActiveVisualiser.find(m_AssociatedClient->m_VisualisedDB);
        if(It == m_AssociatedClient->m_ActiveVisualiser.end() || It->second.Visualiser.size() == 0)
        {
            return nullptr;   
        }
        return It->second.Visualiser.back().get();
    }
    bool Client::DBWindow::Updated()
    {
        auto ActiveWindow = p_GetActiveWindow();
        if(ActiveWindow == nullptr)
        {
            return true;   
        }
        return ActiveWindow->Updated();
    }
    void Client::DBWindow::HandleInput(MBCLI::ConsoleInput const& Input)
    {
        auto ActiveWindow = p_GetActiveWindow();
        if(ActiveWindow == nullptr)
        {
            return;   
        }
        ActiveWindow->HandleInput(Input);
    }
    void Client::DBWindow::SetDimensions(MBCLI::Dimensions NewDimensions)
    {
        auto ActiveWindow = p_GetActiveWindow();
        m_Dims = NewDimensions;
        if(ActiveWindow == nullptr)
        {
            return;   
        }
        ActiveWindow->SetDimensions(NewDimensions);
    }
    MBCLI::Dimensions Client::DBWindow::GetDimensions()
    {
        return m_Dims;
    }
    void Client::DBWindow::SetFocus(bool IsFocused)
    {
        auto ActiveWindow = p_GetActiveWindow();
        if(ActiveWindow == nullptr)
        {
            return;   
        }
        ActiveWindow->SetFocus(IsFocused);
    }
    MBCLI::CursorInfo Client::DBWindow::GetCursorInfo()
    {
        auto ActiveWindow = p_GetActiveWindow();
        if(ActiveWindow == nullptr)
        {
            return MBCLI::CursorInfo();   
        }
        return ActiveWindow->GetCursorInfo();
    }
    MBCLI::TerminalWindowBuffer Client::DBWindow::GetBuffer() 
    {
        auto ActiveWindow = p_GetActiveWindow();
        if(ActiveWindow == nullptr)
        {
            return MBCLI::TerminalWindowBuffer();   
        }
        return ActiveWindow->GetBuffer();
    }
    //


    
    void Client::p_HandleEvent(MBCLI::TTYEvent const& NewInput)
    {
        if(NewInput.IsType<MBCLI::ResizeEvent>())
        {
            m_Terminal.Clear();
            auto const& ResizeEvent = NewInput.GetType<MBCLI::ResizeEvent>();
            m_TopLayerer.SetDimensions( MBCLI::Dimensions(ResizeEvent.NewWidth,ResizeEvent.NewHeight));
        }
        else if(NewInput.IsType<MBCLI::ConsoleInput>())
        {
            auto const& Character = NewInput.GetType<MBCLI::ConsoleInput>();
            if(m_LayerHandleInput)
            {
                m_TopLayerer.HandleInput(Character);
            }
            else
            {
                if(Character.CharacterInput == ':')
                {
                    m_LayerHandleInput = true;
                    m_CommandRepl.SetMaxDims(MBCLI::Dimensions(-1,1));
                    m_CommandRepl.SetOnEnterFunc([&](std::string const& String){p_HandleCommandString(String);});
                    m_TopLayerer.AddLayer(MBUtility::SmartPtr<MBCLI::Window>(&m_CommandRepl));
                }
                else 
                {
                    m_TopWindow.HandleInput(Character);
                }
            }
        }
    }
    void Client::p_SetNewVisualiserWindow(std::shared_ptr<DBVisualiser> Visualiser)
    {
    }
    std::vector<std::string> Client::p_TokenizeCommand(std::string const& NewCommand)
    {
        std::vector<std::string> ReturnValue;
        size_t ParseOffset = 0;
        MBParsing::SkipWhitespace(NewCommand,ParseOffset,&ParseOffset);
        while(ParseOffset < NewCommand.size())
        {
            if(NewCommand[ParseOffset] != '"')
            {
                std::string& NewToken = ReturnValue.emplace_back();
                while(ParseOffset < NewCommand.size() && NewCommand[ParseOffset] != ' ')
                {
                    NewToken += NewCommand[ParseOffset];   
                    ParseOffset++;
                }
            }
            else
            {
                ReturnValue.push_back(MBParsing::ParseQuotedString(NewCommand,ParseOffset,&ParseOffset));
            }

            MBParsing::SkipWhitespace(NewCommand,ParseOffset,&ParseOffset);
        }

        return ReturnValue;
    }
    void Client::p_HandleCommandString(std::string const& NewCommand)
    {
        auto Tokens = p_TokenizeCommand(NewCommand);
        if(Tokens.size() > 0)
        {
            if(Tokens[0] == "addpeer")
            {
                if(Tokens.size() != 4)
                {
                    p_DisplayError("Error in addpeer: needs exactly 3 arguments");
                }
                else
                {
                    try
                    {
                        PeerInfo NewPeer;
                        NewPeer.IP = MBSockets::StringToIP(Tokens[1]);
                        NewPeer.ListeningPort = std::stoi(Tokens[2]);
                        NewPeer.ID = StringToID(MBUtility::HexStringToBytes(Tokens[3]));
                        m_ConnectionManager->AddConnection(std::move(NewPeer));
                    }
                    catch(std::exception const& e)
                    {
                        p_DisplayError("Error in addpeer: "+std::string(e.what()));
                    }
                }
            }
            else if(Tokens[0] == "chat")
            {
                if(Tokens.size() != 2)
                {
                    p_DisplayError("Chat requiers the DB id to initiate a chat with");
                }
                else
                {
                    try
                    {
                        p_StartChat(StringToID(MBUtility::HexStringToBytes(Tokens[1])));
                    }
                    catch(std::exception const& e)
                    {
                        p_DisplayError("Error in addpeer: "+std::string(e.what()));
                    }
                }
            }
        }

        m_TopLayerer.PopLayer();
        m_LayerHandleInput = false;
    }
    DatabaseDefinition Client::p_CreateChatDB(ID const& LocalID,ID const& PeerID)
    {
        DatabaseDefinition ReturnValue;
        ReturnValue.Participants.push_back(LocalID);
        ReturnValue.Participants.push_back(PeerID);
        std::sort(ReturnValue.Participants.begin(),ReturnValue.Participants.end());
        ReturnValue.CalculateHash();
        return ReturnValue;
    }
    void Client::p_StartChat(ID const& PeerID)
    {
        MBParsing::JSONObject Obj;
        auto RPCPromise = m_ConnectionManager->SendPeerRPC(
                PeerID,std::map<std::string,MBParsing::JSONObject>({ {"method","startChat"}}));
        RPCPromise.OnError( [&]()
                {
                    p_DisplayError("Error connecting to peer");
                });
        RPCPromise.Then( [&,PeerID=PeerID](MBParsing::JSONObject const& Response)
                {
                    try
                    {
                        auto NewDB = p_CreateChatDB(PeerID,m_LocalID);
                        m_ConnectionManager->AddDBPeer(PeerID,NewDB.DatabaseID.Content);
                        m_ConnectionManager->JoinDB(NewDB.DatabaseID.Content);
                        if(!m_ConnectionManager->HasDB(NewDB.DatabaseID.Content))
                        {
                            m_ConnectionManager->CreateDB(NewDB);
                        }
                        p_AddVisualiser(NewDB.DatabaseID.Content);
                    }
                    catch(std::exception const& e)
                    {
                        //std::cout<<e.what()<<std::endl;
                    }
                });
    }
    void Client::p_DisplayError(std::string const& ErrorMessage)
    {
           
    }
    void Client::p_AddVisualiser(ID const& DatabaseID)
    {
        std::unique_ptr<DBVisualiser> NewVisualiser = std::make_unique<DBVisualiser>();
        auto& VisualisersInfo = m_ActiveVisualiser[DatabaseID];
        if(VisualisersInfo.Connection == nullptr)
        {
            VisualisersInfo.Connection = std::make_shared<DBConnection>(m_ConnectionManager,m_LocalDB,DatabaseID);
        }
        m_VisualisedDB = DatabaseID;
        NewVisualiser->SetDBConnection(VisualisersInfo.Connection);
        auto TermInfo = m_Terminal.GetTerminalInfo();
        m_TopLayerer.SetDimensions(MBCLI::Dimensions(TermInfo.Width,TermInfo.Height));
        NewVisualiser->SetDimensions(m_DBVisualiserWindow->GetDimensions());
        NewVisualiser->SetDBID(DatabaseID);
        NewVisualiser->Init();
        VisualisersInfo.Visualiser.emplace_back(std::move(NewVisualiser));
        m_TopWindow.MoveRight();
    }
    void Client::p_ResourceRecievedHandler(NewMessage const& Header)
    {
        std::lock_guard Lock(m_InternalsMutex);
        m_RecievedEvents.push_back([&,Header=Header]()
                {
                    auto It = m_ActiveVisualiser.find(Header.Header.OriginalDatabaseHash.Content);
                    if(It != m_ActiveVisualiser.end())
                    {
                        for(auto& Visualiser : It->second.Visualiser)
                        {
                            Visualiser->ResourcePublished(Header);
                        }
                    }
                });
        m_Terminal.CancelRead();
    }
    void Client::p_RPCHandler(PeerInfo const& Peers, MBParsing::JSONObject const& Object,MBUtility::Promise<MBParsing::JSONObject> Response)
    {
        std::lock_guard Lock(m_InternalsMutex);
        m_RecievedEvents.push_back([&,Peers=Peers,Object=Object,Response=std::move(Response)] () mutable
                {
                    if(Object["method"].GetStringData() == "startChat")
                    {
                        try
                        {
                            auto NewDB = p_CreateChatDB(Peers.ID.Content,m_LocalID);
                            m_ConnectionManager->CreateDB(NewDB);
                            m_ConnectionManager->AddDBPeer(Peers,NewDB.DatabaseID.Content);
                            m_ConnectionManager->JoinDB(NewDB.DatabaseID.Content);
                            p_AddVisualiser(NewDB.DatabaseID.Content);
                        }
                        catch(...)
                        {
                                
                        }
                        Response.SetValue(MBParsing::JSONObject(MBParsing::JSONObjectType::Aggregate));
                    }
                });
        m_Terminal.CancelRead();
    }
    int Client::Run()
    {
        MBSockets::Init();
        int ReturnValue = 0;


        std::filesystem::path ConfigPath = MBSystem::GetUserHomeDirectory()/".mbchat/";
        std::filesystem::path DBPath = MBSystem::GetUserHomeDirectory()/".mbchat/localdb.db";
        if(!std::filesystem::exists(DBPath))
        {
            std::filesystem::create_directories(DBPath.parent_path());
            m_LocalDB = std::make_shared<MBDB::MrBoboDatabase>(MBUnicode::PathToUTF8(DBPath),MBDB::DBOpenOptions::ReadWrite);
            try
            {
                m_LocalDB->Exec(
        #include "DBDefinition.inc"
                        );
            }
            catch(std::exception const& e)
            {
                
                std::cout<<"Failed creating database: "<<e.what()<<std::endl;
                std::exit(1);
            }
        }
        else
        {
            m_LocalDB = std::make_shared<MBDB::MrBoboDatabase>(MBUnicode::PathToUTF8(DBPath),MBDB::DBOpenOptions::ReadWrite);
        }
        Config Config;
        if(std::filesystem::exists(ConfigPath/"Config.json"))
        {
            try
            {
                MBError OutError = true;
                auto ConfigJSON = MBParsing::ParseJSONObject(ConfigPath/"Config.json",&OutError);
                Config.FillObject(ConfigJSON);
                if(!OutError)
                {
                    p_DisplayError(OutError.ErrorMessage);   
                }
            }
            catch(std::exception const& e)
            {
                p_DisplayError(e.what());
            }
        }
        auto IDPath = MBUnicode::PathToUTF8(ConfigPath/"id");
        if(!std::filesystem::exists(IDPath))
        {
            std::cout<<"No DB file found"<<std::endl;
        }
        auto IDContent = MBUtility::ReadWholeFile(MBUnicode::PathToUTF8(ConfigPath/"id"));
        m_LocalID.resize(IDContent.size());
        std::memcpy(m_LocalID.data(),IDContent.data(),IDContent.size());
        IDParameters Params;
        Params.LocalID = m_LocalID;
        m_ConnectionManager = std::make_shared<ConnectionManager>(Params,
                Config.Port.Value(),
                MBUnicode::PathToUTF8(DBPath),
                [&](NewMessage const& Resource ){p_ResourceRecievedHandler(Resource);},
                [&](PeerInfo const&  Peer,MBParsing::JSONObject const& Object, MBUtility::Promise<MBParsing::JSONObject> Obj )
                    {p_RPCHandler(Peer,Object,std::move(Obj));}
                );
        std::vector<MBDB::MBDB_RowData> PreviousPeers;
        try
        {
            PreviousPeers = m_LocalDB->GetAllRows("SELECT ID,IP,Port FROM Peers ORDER BY LastSeen LIMIT 100");
        }
        catch(std::exception const& e)
        {
            std::cout<< "Error getting previous peers: "<<e.what()<<std::endl;   
            std::exit(1);
        }
        std::vector<PeerInfo> Peers;
        for(auto& Peer : PreviousPeers)
        {
            PeerInfo& NewPeer = Peers.emplace_back();
            NewPeer.IP = std::get<MBDB::IntType>(Peer["IP"]);
            NewPeer.ID.Content = MBChat2::StringToID(std::get<std::string>(Peer["ID"]));
            NewPeer.ListeningPort = std::get<MBDB::IntType>(Peer["Port"]);
        }
        m_ConnectionManager->AddConnections(Peers);

        m_Terminal.SetExitHandler([]{ std::exit(0);});
        m_Terminal.InitializeWindowMode();
        m_Terminal.SetBufferRenderMode(true);

        MBCLI::Dimensions Dims;
        Dims.Width = m_Terminal.GetTerminalInfo().Width;
        Dims.Height = m_Terminal.GetTerminalInfo().Height;

        MBCLI::TerminalWindowBuffer SideBar(12,Dims.Height);
        SideBar.WriteBorder(10,Dims.Height,0,0,MBCLI::ANSITerminalColor::Green);
        MBCLI::TerminalWindowBuffer Rest(Dims.Width,Dims.Height);

        auto SideWindow = MBTUI::BufferWindow(SideBar);
        auto RestWindow = MBTUI::BufferWindow(Rest);

        m_DBVisualiserWindow = std::make_shared<DBWindow>(this);
        //std::vector<MBCLI::WindowManager::WindowContainer> Windows;
        
        
        m_TopWindow.AddWindow((MBUtility::SmartPtr( (MBCLI::Window*)&SideWindow)),MBCLI::WindowStretchInfo(-1,-1,12));
        m_TopWindow.AddWindow((MBUtility::SmartPtr( std::static_pointer_cast<MBCLI::Window>(m_DBVisualiserWindow))));
        m_TopWindow.SetVertical(false);
        m_TopWindow.SetActiveWindowIndex(1);

        //Windows.emplace_back(MBUtility::SmartPtr( (MBCLI::Window*)&SideWindow));
        //Windows.emplace_back(MBUtility::SmartPtr( std::static_pointer_cast<MBCLI::Window>(m_DBVisualiserWindow)));
        
        //m_TopWindow = MBCLI::WindowManager(std::move(Windows) , Dims,false,1);
        m_TopLayerer.AddLayer(MBUtility::SmartPtr((MBCLI::Window*)&m_TopWindow));
        m_TopLayerer.SetDimensions(Dims);
        bool ShouldRun = true;
        //m_Terminal.PrintWindowBuffer(m_TopWindow.GetBuffer(),0,0);
        m_Terminal.PrintWindowBuffer(m_TopLayerer.GetBuffer(),0,0);
        m_Terminal.HideCursor();
        m_Terminal.Refresh();
        while(ShouldRun)
        {
            auto NewInput = m_Terminal.GetNextEvent();

            {
                std::lock_guard Lock(m_InternalsMutex);
                for(auto& Event : m_RecievedEvents)
                {
                    Event();
                }
                m_RecievedEvents.clear();
            }
            p_HandleEvent(NewInput);

            auto NewBuffer = m_TopLayerer.GetBuffer();
            ///SideWindow.SetDimensions(MBCLI::Dimensions(30,25));
            //auto NewBuffer = SideWindow.GetBuffer();
            m_Terminal.PrintWindowBuffer(NewBuffer,0,0);
            m_Terminal.SetCursorInfo(m_TopLayerer.GetCursorInfo());
            m_Terminal.Refresh();
        }
        return ReturnValue;
    }
}
