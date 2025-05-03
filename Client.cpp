#include "Client.h"
#include <MBTUI/BufferWindow.h>
#include <MBSystem/MBSystem.h>
#include <MBUtility/MBFiles.h>
#include "Config.h"

#include "DBVisualiser.h"
#include "Visualisers/ChatVisualiser.h"

#include "LispModule.h"
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
    bool Client::DBWindow::HandleInput(MBCLI::ConsoleInput const& Input)
    {
        auto ActiveWindow = p_GetActiveWindow();
        if(ActiveWindow == nullptr)
        {
            return false;   
        }
        return ActiveWindow->HandleInput(Input);
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
    void Client::DBWindow::WriteBuffer(MBCLI::BufferView View,bool Redraw)
    {
        auto ActiveWindow = p_GetActiveWindow();
        if(ActiveWindow == nullptr)
        {
            return;
        }
        ActiveWindow->WriteBuffer(View.SubView(0,0),Redraw);
    }
    //


    
    void Client::p_HandleEvent(MBCLI::TTYEvent const& NewInput)
    {
        if(NewInput.IsType<MBCLI::ResizeEvent>())
        {
            m_Terminal.Clear();
            auto const& ResizeEvent = NewInput.GetType<MBCLI::ResizeEvent>();
        }
        else if(NewInput.IsType<MBCLI::ConsoleInput>())
        {
            auto const& Character = NewInput.GetType<MBCLI::ConsoleInput>();
            if(m_LayerHandleInput)
            {
                auto Result = m_TopLayerer.HandleInput(Character);
                if(!Result)
                {
                    m_TopLayerer.PopLayer();
                    m_LayerHandleInput = false;
                }
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
                    try
                    {
                        m_TopWindow.HandleInput(Character);
                    }
                    catch(std::exception const& e)
                    {
                        p_DisplayError(e.what());
                    }
                }
            }
        }
    }
    void Client::p_SetNewVisualiserWindow(std::shared_ptr<DBVisualiser> Visualiser)
    {
    }

    void Client::AddVisualiser(std::string const& DatabaseType,
            VisualiserFactory VisualiserCreator)
    {
        m_RegisteredVisualisers[DatabaseType] = std::move(VisualiserCreator);
    }
    std::vector<std::string> Client::p_CompletionFunc(MBTUI::REPL_Line  const& LineInfo)
    {
        std::vector<std::string> ReturnValue;
        if(LineInfo.Tokens.size() == 1)
        {
            for(auto const& Command : m_RegisteredCommands)
            {
                ReturnValue.emplace_back(Command.first);
            }
        }
        return ReturnValue;
    }
    void Client::AddCommand(std::string const& CommandName, MBUtility::MOFunction<void(std::vector<MBLisp::Value> const&)> Result)
    {
        m_RegisteredCommands[CommandName] = std::move(Result);
    }
    void Client::DisplayOverlay(MBUtility::SmartPtr<MBCLI::Window> TopWindow)
    {
        if(m_LayerHandleInput)
        {
            throw std::runtime_error("Windows is already being displayed!");
        }
        m_LayerHandleInput = true;
        m_TopLayerer.AddLayer(std::move(TopWindow));
    }
    DatabaseDefinition Client::p_LoadDatabase(ID const& DBID)
    {
        DatabaseDefinition ReturnValue;
        ReturnValue.DatabaseID = DBID;
        auto Statement = m_LocalDB->GetSQLStatement(
                "SELECT Type,Time FROM Databases WHERE Hash=:ID");
        Statement.BindBlob("ID",DBID);
        auto Result = m_LocalDB->GetAllRows(Statement);
        if(Result.size() == 0)
        {
            throw std::runtime_error("Could not load database: invalid database ID");
        }
        else if(Result.size() > 1)
        {
            throw std::runtime_error("Database invariant violated: multiple databases with the same ID");
        }
        ReturnValue.Timestamp = std::get<MBDB::IntType>(Result[0]["Time"]);
        ReturnValue.Type = std::get<std::string>(Result[0]["Type"]);
        return ReturnValue;
    }
    void Client::OpenDatabase(ID const& DatabaseID)
    {
        DatabaseDefinition DB = p_LoadDatabase(DatabaseID);
        p_AddVisualiser(DB);
    }
    DatabaseDefinition Client::CreateDatabase(DatabaseDefinition Definition)
    {
        Definition.Timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        Definition.CalculateHash();
        m_ConnectionManager->CreateDB(Definition);
        return Definition;
    }
    std::vector<DatabaseDefinition> Client::GetDatabases()
    {
        std::vector<DatabaseDefinition> ReturnValue;
        auto Statement = m_LocalDB->GetSQLStatement("SELECT Hash FROM Databases");
        auto Result = m_LocalDB->GetAllRows(Statement);
        for(auto const& Row : Result)
        {
            ReturnValue.emplace_back(p_LoadDatabase(StringToID(std::get<std::string>(Row["Hash"]))));
        }
        return ReturnValue;
    }
    std::shared_ptr<Client> Client::MakeClient()
    {
        return std::shared_ptr<Client>(new Client());
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
            else
            {
                m_TopLayerer.PopLayer();
                m_LayerHandleInput = false;
                auto It = m_RegisteredCommands.find(Tokens[0]);
                if(It != m_RegisteredCommands.end())
                {
                    std::vector<MBLisp::Value> Args;
                    for(size_t i = 1; i < Tokens.size();i++)
                    {
                        Args.push_back(MBLisp::Value(Tokens[i]));
                    }
                    try
                    {
                        It->second(Args);
                    }
                    catch(std::exception const& e)
                    {
                        p_DisplayError(e.what());
                    }
                }
                return;
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
        ReturnValue.Type = "Chat";
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
                        p_AddVisualiser(NewDB);
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
    void Client::p_AddVisualiser(DatabaseDefinition const& Database)
    {
        std::unique_ptr<DBVisualiser> NewVisualiser = std::make_unique<ChatVisualiser>();
        
        if(Database.Type != "")
        {
            auto FactoryIt = m_RegisteredVisualisers.find(Database.Type);
            if(FactoryIt != m_RegisteredVisualisers.end())
            {
                NewVisualiser = FactoryIt->second();
            }
        }

        auto& VisualisersInfo = m_ActiveVisualiser[Database.DatabaseID.Content];
        if(VisualisersInfo.Connection == nullptr)
        {
            std::shared_ptr<MBDB::MrBoboDatabase> NewDB = std::make_shared<MBDB::MrBoboDatabase>(":memory:",MBDB::DBOpenOptions::ReadOnly);
            NewDB->Exec("ATTACH '"+MBUnicode::PathToUTF8(m_DBPath.lexically_normal())+"' AS Parent");
            NewDB->Exec("CREATE temp View Resources AS SELECT * FROM Parent.Resources");
            VisualisersInfo.Connection = std::make_shared<DBConnection>(m_ConnectionManager,m_LocalDB,Database.DatabaseID.Content);
        }
        m_VisualisedDB = Database.DatabaseID.Content;
        NewVisualiser->SetDBConnection(VisualisersInfo.Connection);
        auto TermInfo = m_Terminal.GetTerminalInfo();
        NewVisualiser->Init();
        VisualisersInfo.Visualiser.emplace_back(std::move(NewVisualiser));
        m_DBVisualiserWindow->SetChild(*VisualisersInfo.Visualiser.back());
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
                            p_AddVisualiser(NewDB);
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


        m_CommandRepl.AddCompletionFunc([This = shared_from_this()](MBTUI::REPL_Line const& Line)
                {
                    return This->p_CompletionFunc(Line);
                } );

        m_Evaluator = MBLisp::Evaluator::CreateEvaluator();
        auto ChatModule = std::make_unique<ChatLispModule>(shared_from_this());
        m_Evaluator->DumpInternalModule("mbchat",*ChatModule->GetModuleScope(*m_Evaluator));
        m_Evaluator->AddInternalModule("mbchat",std::move(ChatModule));
        m_Evaluator->LoadStd();




        std::filesystem::path ConfigPath = MBSystem::GetUserHomeDirectory()/".mbchat/";
        std::filesystem::path DBPath = MBSystem::GetUserHomeDirectory()/".mbchat/localdb.db";
        std::filesystem::path PluginPath = ConfigPath/"plugins";


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
        m_DBPath = DBPath;
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



        if(std::filesystem::exists(PluginPath))
        {
            auto PluginIterator = std::filesystem::directory_iterator(PluginPath);
            try
            {
                for(auto const& Entry : PluginIterator)
                {
                    if(Entry.is_regular_file() && Entry.path().extension() == ".lisp")
                    {
                        m_Evaluator->Eval(Entry.path());
                    }
                }
            }
            catch(MBLisp::LookupError const& e)
            {
                std::cout<<e.what()<<": "<<m_Evaluator->GetSymbolString(e.GetSymbol())<<std::endl;
            }
            catch (MBLisp::UncaughtSignal& e)
            {
                std::cout<<"Uncaught signal:";
                m_Evaluator->Eval(e.AssociatedScope, e.AssociatedScope->FindVariable(m_Evaluator->GetSymbolID("print")), {e.ThrownValue});
            }
            catch(std::exception const& e)
            {
                std::cout<<e.what()<<std::endl;
            }
        }


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
        bool ShouldRun = true;
        //m_Terminal.PrintWindowBuffer(m_TopWindow.GetBuffer(),0,0);
        m_Terminal.WriteWindow(m_TopLayerer);




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
            auto VisualiserUpdated = m_DBVisualiserWindow->Updated();
            auto TopUpdated = m_TopWindow.Updated();
            auto LayerUpdated = m_TopLayerer.Updated();
            m_Terminal.WriteWindow(m_TopLayerer);
        }

        return ReturnValue;
    }
}
