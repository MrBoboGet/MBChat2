#include "LispModule.h"
#include <MBLisp/Evaluator.h>
#include <MBLisp/Modules/CLI/CLI.h>

namespace MBChat2
{
    LispVisualiser::LispVisualiser(std::shared_ptr<MBLisp::Evaluator> Evaluator,MBLisp::Value Val)
        : m_UnderylingWindow(Evaluator,Val)
    {
        SetChild(m_UnderylingWindow);
        m_ChatScope = Evaluator->GetModuleScope("mbchat");
    }
    void LispVisualiser::Init() 
    {
        auto Ref = MBLisp::FromShared(GetConnectionPtr());
        auto Value = MBLisp::Value(Ref);
        auto& Evaluator = m_UnderylingWindow.GetEvaluator();
        auto Result = Evaluator.Eval(
                m_ChatScope,Evaluator.GetValue(*m_ChatScope,"init") ,{m_UnderylingWindow.GetUnderylingValue(),Value});
    }
    void LispVisualiser::ResourcePublished(NewMessage const& NewHeader) 
    {
        auto& Evaluator = m_UnderylingWindow.GetEvaluator();
        auto Result = Evaluator.Eval(
                m_ChatScope,Evaluator.GetValue(*m_ChatScope,"resource-published") ,
                {m_UnderylingWindow.GetUnderylingValue(),MBLisp::Value::EmplaceExternal<NewMessage>(NewHeader)});
    }
    bool LispVisualiser::HandleInput(MBCLI::ConsoleInput const& Input)
    {
        return m_UnderylingWindow.HandleInput(Input);
    }
    MBCLI::Dimensions LispVisualiser::PreferedDimensions(MBCLI::Dimensions SuggestedDimensions) 
    {
        return m_UnderylingWindow.PreferedDimensions(SuggestedDimensions);
    }
    void LispVisualiser::SetFocus(bool IsFocused) 
    {
        m_UnderylingWindow.SetFocus(IsFocused);
    }
    MBCLI::CursorInfo LispVisualiser::GetCursorInfo() 
    {
        return m_UnderylingWindow.GetCursorInfo();
    }
    void LispVisualiser::WriteBuffer(MBCLI::BufferView View,bool Redraw) 
    {
        m_UnderylingWindow.WriteBuffer(std::move(View),Redraw);
    }


    MBLisp::Ref<DBConnection> LispVisualiser::GetConnectionRef()
    {
        return MBLisp::FromShared(GetConnectionPtr());
    }

    static void UploadString(DBConnection& Connection,MBLisp::String const& String)
    {
        ResourceContent NewContent;
        NewContent.ParentHash = Connection.LatestID();
        NewContent.Content = String;
        Connection.UploadResource(std::move(NewContent));
    }

    static void Init_LispVisuliser(LispVisualiser& Visualiser,DBConnection& Connection)
    {
        Visualiser.Init();
    }
    static void ResourcePublished_LispVisualiser(LispVisualiser& Visualiser,NewMessage const& Resource)
    {
        Visualiser.ResourcePublished(Resource);
    }


    MBLisp::String GetContent(DBConnection& Connection,ResourceHandle const& Resource)
    {
        MBLisp::String ReturnValue;

        auto DB = Connection.GetDB();
        auto Stmt = DB->GetSQLStatement("SELECT StoredLocaly,StoredInline,Content FROM Resources WHERE Hash = :Hash");
        Stmt.BindBlob("Hash",Resource.Id);
        
        auto Rows = DB->GetAllRows(Stmt);
        if(Rows.size() == 0)
        {
            throw std::runtime_error("Cannot get content: invalid resource id");
        }
        if(Rows.size() > 1)
        {
            throw std::runtime_error("Error getting content: invariant broken, more than one row in db");
        }
		auto const& Row = Rows[0];
        if(std::get<MBDB::IntType>(Row["StoredLocaly"]) == 0)
        {
            throw std::runtime_error("Error getting content: invariant broken, more than one row in db");
        }
        if(std::get<MBDB::IntType>(Row["StoredInline"]) == 0)
        {
            throw std::runtime_error("Getting content from file not supported yet");
        }
        ReturnValue = std::get<std::string>(Row["Content"]);
        return ReturnValue;
    }
    std::vector<std::string> GetPath(ResourceHandle const& Resource)
    {
        return Resource.Path;
    }

    struct QueryPart
    {
        bool RecursiveAll = false;
        bool Wildcard = false;
        std::string Name;
        std::regex RegexPattern;
    };



    std::vector<QueryPart> i_ConvertPath(std::vector<std::string> const& Path)
    {
        std::vector<QueryPart> ReturnValue;
        for(auto const& Part : Path)
        {
            auto& QueryPart = ReturnValue.emplace_back();
            QueryPart.Name = Part;
        }
        return ReturnValue;
    }
    std::vector<QueryPart> i_ParseQueryParts(MBLisp::String const& Query)
    {
        std::vector<QueryPart> ReturnValue;
        size_t ParseOffset = 0;
        bool HasRecursive = false;
        while(ParseOffset < Query.size())
        {
            auto NextDelim = Query.find('/',ParseOffset);
            size_t NextNonWhitespace = ParseOffset; 
            MBParsing::SkipWhitespace(Query,ParseOffset,&NextNonWhitespace);
            if(NextNonWhitespace ==  NextDelim)
            {
                ParseOffset = NextNonWhitespace +1;
                continue;
            }
            auto& NewPart = ReturnValue.emplace_back();
            NewPart.Name = Query.substr(NextNonWhitespace,NextDelim-NextNonWhitespace);
            NewPart.RecursiveAll = NewPart.Name == "**";
            NewPart.Wildcard = NewPart.Name.find_first_of("*?") != NewPart.Name.npos && !NewPart.RecursiveAll;
            if(NewPart.Wildcard)
            {
                std::string RegexString;
                size_t RegexParseOffset = 0;
                auto InsertEscaped = [](std::string& OutString,std::string const& Content,size_t Offset,size_t End)
                {
                    for(size_t i = Offset; i < End;i++)
                    {
                        auto SpecialChars = {'f','n','r','t','v','x','u','c'};
                        auto CurrentChar = Content[i];
                        if(std::find(SpecialChars.begin(),SpecialChars.end(),Content[i]) != SpecialChars.end())
                        {
                            OutString += CurrentChar;
                        }
                        else
                        {
                            OutString += '\\';
                            OutString += CurrentChar;
                        }
                    }
                };
                while(RegexParseOffset < NewPart.Name.size())
                {
                    auto NextSpecial = NewPart.Name.find_first_of("*?");
                    if(NextSpecial == std::string::npos)
                    {
                        InsertEscaped(RegexString,NewPart.Name,RegexParseOffset,NewPart.Name.size());
                        break;
                    }
                    InsertEscaped(RegexString,NewPart.Name,RegexParseOffset,NextSpecial);
                    if(NewPart.Name[NextSpecial] == '*')
                    {
                        RegexString += "[\\s\\S]*";
                    }
                    else if(NewPart.Name[NextSpecial] == '?')
                    {
                        RegexString += "[\\s\\S]";
                    }
                    RegexParseOffset = NextSpecial+1;
                }
                NewPart.RegexPattern = std::regex(RegexString);
            }
            if(NewPart.RecursiveAll)
            {
                if(HasRecursive)
                {
                    throw std::runtime_error("Query path can only contain one recursive wildcard");
                }
                HasRecursive = true;
            }
            if(NextDelim == Query.npos)
            {
                break;   
            }
            ParseOffset = NextDelim+1;
        }
        if(ReturnValue.size() == 0)
        {
            throw std::runtime_error("Invalid query: path query cannot be empty");
        }
        if(ReturnValue.back().RecursiveAll)
        {
            throw std::runtime_error("Invalid query: recursive query cannot be last part");
        }
        return ReturnValue;
    }
    void GetResource_ImplPrepared(MBDB::MrBoboDatabase& DB,
                            MBLisp::List& OutResult ,
                            MBDB::SQLStatement& ExplicitName,
                            MBDB::SQLStatement& AllParent,
                            std::vector<QueryPart> const& Parts,
                            MBDB::IntType ParentInt,
                            std::vector<std::string>& CurrentPath,
                            size_t Offset, 
                            size_t RecursiveIndex = -1)
    {
        auto const& CurrentPart = Parts[Offset];
        if(CurrentPart.RecursiveAll)
        {
            RecursiveIndex = Offset+1;
        }
        else if(CurrentPart.Wildcard)
        {
            AllParent.Reset();
            AllParent.BindValue("ParentID",ParentInt);
            auto Rows = DB.GetAllRows(AllParent);
            for(auto const& Row : Rows)
            {
                ID NewHash;
                NewHash = MBChat2::StringToID(std::get<std::string>(Row["ResourceID"]));
                auto NewID = std::get<MBDB::IntType>(Row["ID"]);
                auto Name = std::get<std::string>(Row["Name"]);
                std::smatch Match;
                if(std::regex_match(Name,Match,CurrentPart.RegexPattern))
                {
                    if(Offset + 1 == Parts.size())
                    {
                        ResourceHandle NewHandle;
                        NewHandle.Id = std::move(NewHash);
                        NewHandle.Path = CurrentPath;
                        NewHandle.Path.push_back(Name);
                        OutResult.emplace_back(MBLisp::Value::EmplaceExternal<ResourceHandle>(std::move(NewHandle)));
                    }
                    else
                    {
                        size_t PreviousPathSize = CurrentPath.size();
                        CurrentPath.push_back(Name);
                        GetResource_ImplPrepared(DB,OutResult,ExplicitName,AllParent,Parts,NewID,CurrentPath,Offset+1,RecursiveIndex);
                        CurrentPath.resize(PreviousPathSize);
                    }
                }
            }
        }
        else
        {
            ExplicitName.Reset();
            ExplicitName.BindValue("ParentID",ParentInt);
            ExplicitName.BindString("Name",CurrentPart.Name);
            auto Rows = DB.GetAllRows(ExplicitName);
            for(auto const& Row : Rows)
            {
                ID NewId;
                NewId = MBChat2::StringToID(std::get<std::string>(Row["ResourceID"]));
                auto Name = std::get<std::string>(Row["Name"]);
                auto NewID = std::get<MBDB::IntType>(Row["ID"]);
                if(Offset + 1 == Parts.size())
                {
                    ResourceHandle NewHandle;
                    NewHandle.Id = std::move(NewId);
                    NewHandle.Path = CurrentPath;
                    NewHandle.Path.push_back(Name);
                    OutResult.emplace_back(MBLisp::Value::EmplaceExternal<ResourceHandle>(std::move(NewHandle)));
                }
                else
                {
                    size_t PreviousPathSize = CurrentPath.size();
                    CurrentPath.push_back(Name);
                    GetResource_ImplPrepared(DB,OutResult,ExplicitName,AllParent,Parts,NewID,CurrentPath,Offset+1,RecursiveIndex);
                    CurrentPath.resize(PreviousPathSize);
                }
            }
        }
        if(RecursiveIndex != -1)
        {
            AllParent.Reset();
            AllParent.BindValue("ParentID",ParentInt);
            auto Rows = DB.GetAllRows(AllParent);
            for(auto const& Row : Rows)
            {
                ID NewId;
                NewId = MBChat2::StringToID(std::get<std::string>(Row["ResourceID"]));
                auto Name = std::get<std::string>(Row["Name"]);
                auto NewID = std::get<MBDB::IntType>(Row["ID"]);
                size_t PreviousPathSize = CurrentPath.size();
                CurrentPath.push_back(Name);
                GetResource_ImplPrepared(DB,OutResult,ExplicitName,AllParent,Parts,NewID,CurrentPath,RecursiveIndex,RecursiveIndex);
                CurrentPath.resize(PreviousPathSize);
            }
        }
    }
    void GetResource_Impl(DBConnection& Connection,
                            MBLisp::List& OutResult ,
                            std::vector<QueryPart> const& Parts,
                            ID const& ResourceRoot,
                            size_t Offset)
    {
        auto DB = Connection.GetDB();
        MBDB::SQLStatement ExplicitName = DB->GetSQLStatement("SELECT ID,ParentID,ResourceID,Name FROM ActiveTree WHERE ParentID = :ParentID AND Name = :Name");
        MBDB::SQLStatement AllParent = DB->GetSQLStatement("SELECT ID,ParentID,ResourceID,Name FROM ActiveTree WHERE ParentID = :ParentID");
        MBDB::IntType ParentID = 0;
        MBDB::IntType OutID = 0;
        std::vector<std::string> ParentPath =  Connection.GetAbsoluteResourcePath(ResourceRoot,ParentID,OutID);
        GetResource_ImplPrepared(*DB,OutResult,ExplicitName,AllParent,Parts,ParentID,ParentPath,Offset);
    }

    MBLisp::Value GetResource(DBConnection& Connection,MBLisp::String const& Path)
    {
        auto Query = i_ParseQueryParts(Path);
        MBLisp::List Result;
        GetResource_Impl(Connection,Result,Query,Connection.GetDBID(),0);
        if(Result.size() == 0)
        {
            //throw std::runtime_error("Cannot find resource with path \""+Path+"\"");
            return MBLisp::Value();
        }
        return Result[0];
    }

    MBLisp::List GetResources(DBConnection& Connection,MBLisp::String const& Query)
    {
        auto QueryParts = i_ParseQueryParts(Query);
        MBLisp::List Result;
        GetResource_Impl(Connection,Result,QueryParts,Connection.GetDBID(),0);
        return Result;
    }

    MBLisp::Value GetResource_Handle(DBConnection& Connection,ResourceHandle const& Root ,MBLisp::String const& Query)
    {
        auto QueryParts = i_ParseQueryParts(Query);
        MBLisp::List Result;
        GetResource_Impl(Connection,Result,QueryParts,Root.Id,0);
        if(Result.size() == 0)
        {
            //throw std::runtime_error("Cannot find resource with path \""+Query+"\"");
            return MBLisp::Value();
        }
        return Result[0];
    }
    MBLisp::List GetResources_Handle(DBConnection& Connection,ResourceHandle const& Root ,MBLisp::String const& Query)
    {
        auto QueryParts = i_ParseQueryParts(Query);
        MBLisp::List Result;
        GetResource_Impl(Connection,Result,QueryParts,Root.Id,0);
        return Result;
    }
    void AddResource_Query(DBConnection& Connection,std::vector<QueryPart>& QueryParts,MBLisp::String const& Content)
    {
        ResourceContent NewContent;
        NewContent.Content = Content;
        NewContent.Name = QueryParts.back().Name;
        ID Parent = Connection.GetDBID();
        if(QueryParts.size() > 0)
        {
            QueryParts.resize(QueryParts.size()-1);
        }
        if(QueryParts.size() != 0)
        {
            MBLisp::List Resources;
            GetResource_Impl(Connection,Resources,QueryParts,Connection.GetDBID(),0);
            if(Resources.size() != 1)
            {
                throw std::runtime_error("Error adding resource to specific path: directory query returned "+std::to_string(Resources.size())+" elements");
            }
            Parent = Resources[0].GetType<ResourceHandle>().Id;
        }
        NewContent.ParentHash.Content = Parent;
        Connection.UploadResource(std::move(NewContent));
    }
    void AddResource_Path(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Content)
    {
        auto QueryParts = i_ParseQueryParts(Path);
        if(QueryParts.size() == 0 || !std::all_of(QueryParts.begin(),QueryParts.end(),[](QueryPart const& Part)
                    {return !Part.RecursiveAll && !Part.Wildcard;})  )
        {
            throw std::runtime_error("Error adding resource: invalid path \""+Path+"\"");
        }
        AddResource_Query(Connection,QueryParts,Content);
    }
    void AddResource_VectorPath(DBConnection& Connection,std::vector<std::string> const& Path,MBLisp::String const& Content)
    {
        auto QueryParts = i_ConvertPath(Path);
        if(QueryParts.size() == 0 || !std::all_of(QueryParts.begin(),QueryParts.end(),[](QueryPart const& Part)
                    {return !Part.RecursiveAll && !Part.Wildcard;})  )
        {
            throw std::runtime_error("Error adding resource: invalid path");
        }
        AddResource_Query(Connection,QueryParts,Content);
    }
    std::vector<std::string> AddChild_Path(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Content)
    {
        ResourceContent NewContent;
        NewContent.Content = Content;
        auto QueryParts = i_ParseQueryParts(Path);
        if(QueryParts.size() == 0 || !std::all_of(QueryParts.begin(),QueryParts.end(),[](QueryPart const& Part)
                    {return !Part.RecursiveAll && !Part.Wildcard;})  )
        {
            throw std::runtime_error("Error adding resource: invalid path \""+Path+"\"");
        }
        //NewContent.Name = QueryParts.back().Name;
        MBLisp::List Resources;
        GetResource_Impl(Connection,Resources,QueryParts,Connection.GetDBID(),0);
        if(Resources.size() != 1)
        {
            throw std::runtime_error("Error adding resource to specific path: directory query returned "+std::to_string(Resources.size())+" elements");
        }
        auto& Parent = Resources[0].GetType<ResourceHandle>();
        NewContent.ParentHash.Content = Parent.Id;
        Connection.UploadResource(std::move(NewContent));
        std::vector<std::string> ReturnValue = std::move(Parent.Path);
        ReturnValue.push_back(MBCrypto::HashData(Content,MBCrypto::HashFunction::SHA256));
        return ReturnValue;
    }

    void AddResource_Parent(DBConnection& Connection,ResourceHandle const& Parent,MBLisp::String const& Name,MBLisp::String const& Content)
    {
        ResourceContent NewContent;
        NewContent.ParentHash = Parent.Id;
        NewContent.Content = Content;
        NewContent.Name = Name;
        Connection.UploadResource(std::move(NewContent));
    }
    static void RemoveResource_Path(DBConnection& Connection,std::vector<std::string> const& Path)
    {
        Connection.RemoveResource(Path);
    }
    static void RemoveResource_Resource(DBConnection& Connection,ResourceHandle const& Resource)
    {
        Connection.RemoveResource(Resource.Id);
    }


    //MBLisp::List EditResource(DBConnection& Connection,ResourceHandle const& New)
    //{

    //}


    static MBLisp::Value Index_NewMessage(MBLisp::Evaluator& Evaluator
            ,NewMessage const& Message,MBLisp::Symbol Sym)
    {
        MBLisp::Value ReturnValue;
        auto const& SymString = Evaluator.GetSymbolString(Sym.ID);
        if(SymString == "content")
        {
            return Message.Content;   
        }
        return ReturnValue;
    }
    static MBLisp::Value Index_Database(MBLisp::Evaluator& Evaluator
            ,DatabaseDefinition const& Message,MBLisp::Symbol Sym)
    {
        MBLisp::Value ReturnValue;
        auto const& SymString = Evaluator.GetSymbolString(Sym.ID);
        if(SymString == "type")
        {
            return Message.Type;
        }
        return ReturnValue;
    }

    static void AddVisualiser(MBLisp::Evaluator& Evaluator,Client& Client,MBLisp::String const& Type,MBLisp::Value LispValue)
    {
        Client.AddVisualiser(Type,
                [Value=LispValue,
                Evaluator = Evaluator.shared_from_this()]
                () -> std::unique_ptr<DBVisualiser>
                {
                    auto Result = Evaluator->Eval(Value,{});

                    return std::make_unique<LispVisualiser>(Evaluator,Result);
                });
    }

    static MBDB::SQLStatement Statement_Connection(DBConnection& Connection,MBLisp::String const& SQL)
    {
        return Connection.GetDB()->GetSQLStatement(SQL);
    }
    static MBLisp::Value Connection_DB(DBConnection& Connection)
    {
        return MBLisp::FromShared(Connection.GetDB());
    }
    static DatabaseDefinition NewDB(MBLisp::String const& Type)
    {
        DatabaseDefinition Def;
        Def.Type = Type;
        return Def;
    }

    MBLisp::Ref<MBLisp::Scope> ChatLispModule::GetModuleScope(MBLisp::Evaluator& AssociatedEvaluator) 
    {
        auto ReturnValue = MBLisp::MakeRef<MBLisp::Scope>();

        ReturnValue->SetVariable(AssociatedEvaluator.GetSymbolID("client"),
                MBLisp::FromShared(m_AssociatedClient));

        AssociatedEvaluator.AddGeneric<Init_LispVisuliser>(ReturnValue,"init");
        AssociatedEvaluator.AddGeneric<ResourcePublished_LispVisualiser>(ReturnValue,"resource-published");

        AssociatedEvaluator.AddObjectMethod<&DBConnection::LatestID>(ReturnValue,"latest-id");
        AssociatedEvaluator.AddObjectMethod<&LispVisualiser::GetConnectionRef>(ReturnValue,"connection");
        AssociatedEvaluator.AddGeneric<UploadString>(ReturnValue,"upload-resource");

        AssociatedEvaluator.AddGeneric<AddVisualiser>(ReturnValue,"add-visualiser");

        AssociatedEvaluator.AddFunctionObject(ReturnValue,"add-command",
                [Client=m_AssociatedClient,Evaluator=AssociatedEvaluator.shared_from_this()](MBLisp::String const& CommandName,MBLisp::Value const& Val) mutable
                {
                    Client->AddCommand(CommandName,[Val = Val,Evaluator=Evaluator](std::vector<MBLisp::Value> const& Args)
                            {
                                MBLisp::FuncArgVector CallArgs;
                                for(auto const& Arg : Args)
                                {
                                    CallArgs.push_back(Arg);
                                }
                                Evaluator->Eval(Val,std::move(CallArgs));
                            });
                });
        AssociatedEvaluator.AddFunctionObject(ReturnValue,"display-overlay",
                [Client=m_AssociatedClient,Evaluator=AssociatedEvaluator.shared_from_this()](MBLisp::Value Value) mutable
                {
                    MBUtility::SmartPtr<MBCLI::Window> Window;
                    if(Value.IsType<MBCLI::Window>())
                    {
                        Window = Value.GetSharedPtr<MBCLI::Window>();
                    }
                    else
                    {
                        Window = std::unique_ptr<MBCLI::Window>(std::make_unique<MBLisp::LispWindow>(Evaluator,Value));
                    }
                    Client->DisplayOverlay(std::move(Window));
                });
        AssociatedEvaluator.AddFunctionObject(ReturnValue,"display-db",
                [Client=m_AssociatedClient,Evaluator=AssociatedEvaluator.shared_from_this()](MBLisp::String const& Val) mutable
                {
                    Client->OpenDatabase(StringToID(Val));
                });
        AssociatedEvaluator.AddFunctionObject(ReturnValue,"display-db",
                [Client=m_AssociatedClient,Evaluator=AssociatedEvaluator.shared_from_this()](DatabaseDefinition const& Val) mutable
                {
                    Client->OpenDatabase(Val.DatabaseID.Content);
                });
        AssociatedEvaluator.AddFunctionObject(ReturnValue,"get-databases",
                [Client=m_AssociatedClient,Evaluator=AssociatedEvaluator.shared_from_this()]() mutable
                {
                    MBLisp::List ReturnValue;
                    auto Databases = Client->GetDatabases();
                    for(auto& DB : Databases)
                    {
                        ReturnValue.emplace_back(
                                MBLisp::Value::EmplaceExternal<DatabaseDefinition>(DB));
                    }
                    return ReturnValue;
                });
        AssociatedEvaluator.AddGeneric<NewDB>(ReturnValue,"new-db");
        AssociatedEvaluator.AddFunctionObject(ReturnValue,"create-db",
                [Client=m_AssociatedClient](DatabaseDefinition const& Def) mutable
                {
                    return Client->CreateDatabase(Def);
                });

        AssociatedEvaluator.AddGeneric<Connection_DB>(ReturnValue,"db");
        AssociatedEvaluator.AddGeneric<Index_NewMessage>("index");
        AssociatedEvaluator.AddGeneric<Index_Database>("index");


        AssociatedEvaluator.AddGeneric<GetResource>(ReturnValue,"get-resource");
        AssociatedEvaluator.AddGeneric<GetResource_Handle>(ReturnValue,"get-resource");
        AssociatedEvaluator.AddGeneric<GetResources>(ReturnValue,"get-resources");
        AssociatedEvaluator.AddGeneric<GetResources_Handle>(ReturnValue,"get-resources");

        AssociatedEvaluator.AddGeneric<AddResource_Path>(ReturnValue,"add-resource");
        AssociatedEvaluator.AddGeneric<AddResource_VectorPath>(ReturnValue,"add-resource");
        AssociatedEvaluator.AddGeneric<AddResource_Parent>(ReturnValue,"add-resource");

        AssociatedEvaluator.AddGeneric<RemoveResource_Resource>(ReturnValue,"remove-resource");
        AssociatedEvaluator.AddGeneric<RemoveResource_Path>(ReturnValue,"remove-resource");


        AssociatedEvaluator.AddGeneric<AddChild_Path>(ReturnValue,"add-child");

        AssociatedEvaluator.AddGeneric<GetContent>(ReturnValue,"get-content");
        AssociatedEvaluator.AddGeneric<GetPath>(ReturnValue,"get-path");

        AssociatedEvaluator.AddType<DatabaseDefinition>(ReturnValue,"db-def_t");
        AssociatedEvaluator.AddType<Message>(ReturnValue,"message_t");
        AssociatedEvaluator.AddType<ResourceHandle>(ReturnValue,"resource-handle_t");

        return ReturnValue;
    }
}
