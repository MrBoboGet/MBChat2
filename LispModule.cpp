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
        ResourceHandle Handle;
        Handle.Id = NewHeader.Header.HeaderHash.Content;
        Handle.Type = NewHeader.Header.Type;

        auto Connection = GetConnectionPtr();
        Handle.Path = Connection->GetAbsoluteResourcePath(Handle.Id);


        auto Result = Evaluator.Eval(
                m_ChatScope,Evaluator.GetValue(*m_ChatScope,"resource-published") ,
                {m_UnderylingWindow.GetUnderylingValue(),MBLisp::Value::EmplaceExternal<ResourceHandle>(Handle)});
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

    static ResourceHandle UploadString(DBConnection& Connection,MBLisp::String const& String)
    {
        ResourceHandle ReturnValue;
        ResourceContent NewContent;
        //NewContent.ParentHash = Connection.LatestID();
        NewContent.Content = String;
        ReturnValue.Id = Connection.UploadResource(std::move(NewContent));
        MBDB::IntType ParentID = 0;
        MBDB::IntType OutID = 0;
        ReturnValue.Path = Connection.GetAbsoluteResourcePath(ReturnValue.Id,ParentID,OutID);
        return ReturnValue;
    }
    static ResourceHandle UploadString_Type(DBConnection& Connection,MBLisp::String const& Content,MBLisp::String const& Type)
    {
        ResourceHandle ReturnValue;
        ResourceContent NewContent;
        NewContent.ParentHash = Connection.LatestID();
        NewContent.Content = Content;
        NewContent.Type = Type;
        ReturnValue.Type = Type;
        ReturnValue.Id = Connection.UploadResource(std::move(NewContent));
        MBDB::IntType ParentID = 0;
        MBDB::IntType OutID = 0;
        ReturnValue.Path = Connection.GetAbsoluteResourcePath(ReturnValue.Id,ParentID,OutID);
        return ReturnValue;
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
    MBLisp::String GetUser(DBConnection& Connection,ResourceHandle const& Resource)
    {
        MBLisp::String ReturnValue;

        auto DB = Connection.GetDB();
        auto Stmt = DB->GetSQLStatement("SELECT UploaderID FROM Resources WHERE Hash = :Hash");
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
        auto const& ID = std::get<std::string>(Row["UploaderID"]);
        ReturnValue = MBUtility::HexEncodeString(ID);
        ReturnValue = ReturnValue.substr(0,10);
        return ReturnValue;
    }
    MBLisp::String GetType(ResourceHandle const& Resource)
    {
        return Resource.Type;
    }
    std::vector<std::string> GetPath(ResourceHandle const& Resource)
    {
        return Resource.Path;
    }
    MBLisp::String GetId(ResourceHandle const& Resource)
    {
        return IDToString(Resource.Id);
    }
    MBLisp::String GetPathString(ResourceHandle const& Resource)
    {
        MBLisp::String ReturnValue = "";
        for(auto const& Part : Resource.Path)
        {
            ReturnValue += "/";
            ReturnValue += Part;
        }
        return ReturnValue;
    }


    static ResourceStateHandle GetStateObserver(DBConnection& Connection,MBLisp::String const& ID)
    {
        return Connection.GetStateHandle(StringToID(ID));
    }
    static MBLisp::Float DownloadPercent(ResourceStateHandle& Handle)
    {
        return Handle.DownloadPercent();
    }
    static void StartDownload(ResourceStateHandle& Handle)
    {
        auto Task = Handle.StartDownload();
        Task.resume();
    }
    static void OnStateChanged_ResourceHandle(ResourceStateHandle& Handle)
    {

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
                            ID const& DBID,
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
            AllParent.BindBlob("DatabaseID",DBID);
            auto Rows = DB.GetAllRows(AllParent);
            for(auto const& Row : Rows)
            {
                ID NewHash;
                NewHash = MBChat2::StringToID(std::get<std::string>(Row["ResourceID"]));
                auto NewID = std::get<MBDB::IntType>(Row["ID"]);
                auto Name = std::get<std::string>(Row["Name"]);
                auto Type = std::get<std::string>(Row["Type"]);
                std::smatch Match;
                if(std::regex_match(Name,Match,CurrentPart.RegexPattern))
                {
                    if(Offset + 1 == Parts.size())
                    {
                        ResourceHandle NewHandle;
                        NewHandle.Id = std::move(NewHash);
                        NewHandle.Path = CurrentPath;
                        NewHandle.Path.push_back(Name);
                        NewHandle.Type = Type;
                        OutResult.emplace_back(MBLisp::Value::EmplaceExternal<ResourceHandle>(std::move(NewHandle)));
                    }
                    else
                    {
                        size_t PreviousPathSize = CurrentPath.size();
                        CurrentPath.push_back(Name);
                        GetResource_ImplPrepared(DB,DBID,OutResult,ExplicitName,AllParent,Parts,NewID,CurrentPath,Offset+1,RecursiveIndex);
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
            ExplicitName.BindBlob("DatabaseID",DBID);
            auto Rows = DB.GetAllRows(ExplicitName);
            for(auto const& Row : Rows)
            {
                ID NewId;
                NewId = MBChat2::StringToID(std::get<std::string>(Row["ResourceID"]));
                auto Name = std::get<std::string>(Row["Name"]);
                auto NewID = std::get<MBDB::IntType>(Row["ID"]);
                auto Type = std::get<std::string>(Row["Type"]);
                if(Offset + 1 == Parts.size())
                {
                    ResourceHandle NewHandle;
                    NewHandle.Id = std::move(NewId);
                    NewHandle.Path = CurrentPath;
                    NewHandle.Path.push_back(Name);
                    NewHandle.Type = Type;
                    OutResult.emplace_back(MBLisp::Value::EmplaceExternal<ResourceHandle>(std::move(NewHandle)));
                }
                else
                {
                    size_t PreviousPathSize = CurrentPath.size();
                    CurrentPath.push_back(Name);
                    GetResource_ImplPrepared(DB,DBID,OutResult,ExplicitName,AllParent,Parts,NewID,CurrentPath,Offset+1,RecursiveIndex);
                    CurrentPath.resize(PreviousPathSize);
                }
            }
        }
        if(RecursiveIndex != -1)
        {
            AllParent.Reset();
            AllParent.BindValue("ParentID",ParentInt);
            AllParent.BindBlob("DatabaseID",DBID);
            auto Rows = DB.GetAllRows(AllParent);
            for(auto const& Row : Rows)
            {
                ID NewId;
                NewId = MBChat2::StringToID(std::get<std::string>(Row["ResourceID"]));
                auto Name = std::get<std::string>(Row["Name"]);
                auto NewID = std::get<MBDB::IntType>(Row["ID"]);
                size_t PreviousPathSize = CurrentPath.size();
                CurrentPath.push_back(Name);
                GetResource_ImplPrepared(DB,DBID,OutResult,ExplicitName,AllParent,Parts,NewID,CurrentPath,RecursiveIndex,RecursiveIndex);
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
        auto DBID = Connection.GetDBID();
        MBDB::SQLStatement ExplicitName = DB->GetSQLStatement(
                "SELECT tree.ID,tree.ParentID,tree.ResourceID,tree.Name,resource.ContentType AS Type FROM ActiveTree tree INNER JOIN "
                " Resources resource ON tree.ResourceID = resource.Hash AND tree.DatabaseID = resource.DatabaseHash "
                " WHERE tree.ParentID = :ParentID AND tree.Name = :Name AND tree.DatabaseID = :DatabaseID ORDER BY resource.Time ASC");
        MBDB::SQLStatement AllParent = DB->GetSQLStatement(
                " SELECT tree.ID,tree.ParentID,tree.ResourceID,resource.ContentType AS Type,tree.Name FROM ActiveTree tree INNER JOIN Resources resource ON  "
                " tree.ResourceID = resource.Hash AND tree.DatabaseID = resource.DatabaseHash "
                " WHERE tree.ParentID = :ParentID AND tree.DatabaseID = :DatabaseID ORDER BY resource.Time ASC");
        MBDB::IntType ParentID = 0;
        MBDB::IntType OutID = 0;
        std::vector<std::string> ParentPath =  Connection.GetAbsoluteResourcePath(ResourceRoot,ParentID,OutID);
        GetResource_ImplPrepared(*DB,DBID,OutResult,ExplicitName,AllParent,Parts,ParentID,ParentPath,Offset);
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
    MBLisp::Value GetResourceById(DBConnection& Connection,MBLisp::String const& ID)
    {
        ResourceHeader Header;
        if(!Connection.GetResourceByID(StringToID(ID),Header))
        {
            return MBLisp::Value();
        }
        MBDB::IntType ParentID;
        MBDB::IntType PathID;
        auto Path = Connection.GetAbsoluteResourcePath(Header.HeaderHash.Content,ParentID,PathID);
        ResourceHandle NewHandle;
        NewHandle.Id = StringToID(ID);
        NewHandle.Path = std::move(Path);
        NewHandle.Type = Header.Type;
        return MBLisp::Value::EmplaceExternal<ResourceHandle>(std::move(NewHandle));
    }
    MBLisp::String GetResourceID(ResourceHandle const& Handle)
    {
        return IDToString(Handle.Id);
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
    ResourceHandle AddResource_Query_Type(DBConnection& Connection,std::vector<QueryPart>& QueryParts,MBLisp::String const& Content,MBLisp::String const& Type)
    {
        ResourceHandle ReturnValue;
        ResourceContent NewContent;
        NewContent.Content = Content;
        NewContent.Name = QueryParts.back().Name;
        NewContent.Type = Type;
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
        ReturnValue.Id = Connection.UploadResource(std::move(NewContent));
        ReturnValue.Type = Type;
        MBDB::IntType ParentId = 0;
        MBDB::IntType TreeId = 0;
        ReturnValue.Path = Connection.GetAbsoluteResourcePath(ReturnValue.Id,ParentId,TreeId);
        return ReturnValue;
    }
    ResourceHandle AddResource_Path_Type(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Content,MBLisp::String const& Type)
    {
        auto QueryParts = i_ParseQueryParts(Path);
        if(QueryParts.size() == 0 || !std::all_of(QueryParts.begin(),QueryParts.end(),[](QueryPart const& Part)
                    {return !Part.RecursiveAll && !Part.Wildcard;})  )
        {
            throw std::runtime_error("Error adding resource: invalid path \""+Path+"\"");
        }
        return AddResource_Query_Type(Connection,QueryParts,Content,Type);
    }
    ResourceHandle AddResource_Path(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Content)
    {
        return AddResource_Path_Type(Connection,Path,Content,"");
    }
    ResourceHandle AddResource_VectorPath_Type(DBConnection& Connection,
            std::vector<std::string> const& Path,MBLisp::String const& Content,MBLisp::String const& Type)
    {
        auto QueryParts = i_ConvertPath(Path);
        if(QueryParts.size() == 0 || !std::all_of(QueryParts.begin(),QueryParts.end(),[](QueryPart const& Part)
                    {return !Part.RecursiveAll && !Part.Wildcard;})  )
        {
            throw std::runtime_error("Error adding resource: invalid path");
        }
        return AddResource_Query_Type(Connection,QueryParts,Content,Type);
    }
    ResourceHandle AddResource_VectorPath(DBConnection& Connection,
            std::vector<std::string> const& Path,MBLisp::String const& Content)
    {
        return AddResource_VectorPath_Type(Connection,Path,Content,"");
    }
    ResourceHandle AddChild_Path_Type(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Content,MBLisp::String const& Type)
    {
        ResourceContent NewContent;
        NewContent.Content = Content;
        NewContent.Type = Type;
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
        auto ID = Connection.UploadResource(std::move(NewContent));
        std::vector<std::string> ResourcePath = std::move(Parent.Path);
        ResourcePath.push_back(MBCrypto::HashData(Content,MBCrypto::HashFunction::SHA256));
        ResourceHandle Ret;
        Ret.Path = std::move(ResourcePath);
        Ret.Id = ID;
        Ret.Type = Type;
        return Ret;
    }
    ResourceHandle AddChild_Path(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Content)
    {
        return AddChild_Path_Type(Connection,Path,Content,"");
    }
    ResourceHandle UploadFile_Path_Type(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Filepath,MBLisp::String const& Type)
    {
        ResourceContent NewContent;
        NewContent.Type = Type;
        auto QueryParts = i_ParseQueryParts(Path);
        if(QueryParts.size() == 0 || !std::all_of(QueryParts.begin(),QueryParts.end(),[](QueryPart const& Part)
                    {return !Part.RecursiveAll && !Part.Wildcard;})  )
        {
            throw std::runtime_error("Error adding resource: invalid path \""+Path+"\"");
        }
        MBLisp::List Resources;
        GetResource_Impl(Connection,Resources,QueryParts,Connection.GetDBID(),0);
        if(Resources.size() != 1)
        {
            throw std::runtime_error("Error adding resource to specific path: directory query returned "+std::to_string(Resources.size())+" elements");
        }
        auto& Parent = Resources[0].GetType<ResourceHandle>();
        NewContent.ParentHash.Content = Parent.Id;
        bool Copy = true;
        if(std::filesystem::file_size(Filepath) > 25*1000000)
        {
            Copy = false;
        }
        auto ID = Connection.UploadFile(std::move(NewContent),Filepath,Copy);
        ResourceHandle Ret;
        Ret.Path = Connection.GetAbsoluteResourcePath(ID);
        Ret.Id = ID;
        Ret.Type = Type;
        return Ret;
    }
    ResourceHandle UploadFile_Path(DBConnection& Connection,MBLisp::String const& Path,MBLisp::String const& Filepath)
    {
        return UploadFile_Path_Type(Connection,Path,Filepath,"");
    }

    ResourceHandle AddResource_Parent(DBConnection& Connection,ResourceHandle const& Parent,MBLisp::String const& Name,MBLisp::String const& Content)
    {
        ResourceHandle ReturnValue;
        ResourceContent NewContent;
        NewContent.ParentHash = Parent.Id;
        NewContent.Content = Content;
        NewContent.Name = Name;
        ReturnValue.Id = Connection.UploadResource(std::move(NewContent));
        return ReturnValue;
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
    static MBLisp::List GetDatabases(Client& Client,MBLisp::Evaluator& Evaluator)
    {
        MBLisp::List ReturnValue;



        return ReturnValue;
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
        AssociatedEvaluator.AddGeneric<UploadString_Type>(ReturnValue,"upload-resource");

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
        AssociatedEvaluator.AddFunctionObject(ReturnValue,"add-completion",
                [Client=m_AssociatedClient,Evaluator=AssociatedEvaluator.shared_from_this()](MBLisp::String const& CommandName,MBLisp::Value const& Val) mutable
                {
                    Client->AddCommandCompletion(CommandName,[Val = Val,Evaluator=Evaluator](std::vector<std::string> const& Args) -> std::vector<std::string>
                            {
                                MBLisp::FuncArgVector CallArgs;
                                MBLisp::List Tokens;
                                for(auto const& Token : Args)
                                {
                                    Tokens.push_back(Token);
                                }
                                CallArgs.push_back(std::move(Tokens));
                                auto Res = Evaluator->Eval(Val,std::move(CallArgs));
                                if(!Res.IsType<MBLisp::List>())
                                {
                                    throw std::runtime_error("Error evaluating completions: Resulst is not a list");
                                }
                                std::vector<std::string> ReturnValue;
                                for(auto const& Value : Res.GetType<MBLisp::List>())
                                {
                                    if(Value.IsType<MBLisp::String>())
                                    {
                                        throw std::runtime_error("Error evaluating completion func: Element of result is not a string");
                                    }
                                    ReturnValue.push_back(Value.GetType<MBLisp::String>());
                                }
                                return ReturnValue;
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
        AssociatedEvaluator.AddGeneric<GetResourceById>(ReturnValue,"get-resource-by-id");
        AssociatedEvaluator.AddGeneric<GetResourceID>(ReturnValue,"get-resource-id");
        AssociatedEvaluator.AddGeneric<GetResources>(ReturnValue,"get-resources");
        AssociatedEvaluator.AddGeneric<GetResources_Handle>(ReturnValue,"get-resources");

        AssociatedEvaluator.AddGeneric<AddResource_Path>(ReturnValue,"add-resource");
        AssociatedEvaluator.AddGeneric<AddResource_VectorPath>(ReturnValue,"add-resource");
        AssociatedEvaluator.AddGeneric<AddResource_Parent>(ReturnValue,"add-resource");

        AssociatedEvaluator.AddGeneric<AddResource_Path_Type>(ReturnValue,"add-resource");
        AssociatedEvaluator.AddGeneric<AddResource_VectorPath_Type>(ReturnValue,"add-resource");

        AssociatedEvaluator.AddGeneric<RemoveResource_Resource>(ReturnValue,"remove-resource");
        AssociatedEvaluator.AddGeneric<RemoveResource_Path>(ReturnValue,"remove-resource");

        //state observers
        AssociatedEvaluator.AddType<ResourceStateHandle>(ReturnValue,"resource-state-observer_t");
        AssociatedEvaluator.AddGeneric<GetStateObserver>(ReturnValue,"get-state-observer");
        AssociatedEvaluator.AddGeneric<DownloadPercent>(ReturnValue,"download-percent");
        AssociatedEvaluator.AddGeneric<StartDownload>(ReturnValue,"start-download");
        AssociatedEvaluator.AddFunctionObject(ReturnValue,"on-state-changed",
                [Client=m_AssociatedClient,Evaluator=AssociatedEvaluator.shared_from_this()](ResourceStateHandle& Handle,MBLisp::Value Callable) mutable
                {
                    Handle.SetOnStateChanged([Client=Client,Evaluator=Evaluator,Callable=std::move(Callable)]()
                            {
                                Client->AddEvent([Evaluator=Evaluator,Callable=Callable]()
                                        {
                                            Evaluator->Eval(Callable,{});
                                        });
                            });
                });
        //

        AssociatedEvaluator.AddGeneric<AddChild_Path>(ReturnValue,"add-child");
        AssociatedEvaluator.AddGeneric<AddChild_Path_Type>(ReturnValue,"add-child");
        AssociatedEvaluator.AddGeneric<UploadFile_Path>(ReturnValue,"upload-file");
        AssociatedEvaluator.AddGeneric<UploadFile_Path_Type>(ReturnValue,"upload-file");

        AssociatedEvaluator.AddGeneric<GetContent>(ReturnValue,"get-content");
        AssociatedEvaluator.AddGeneric<GetUser>(ReturnValue,"get-user");
        AssociatedEvaluator.AddGeneric<GetType>(ReturnValue,"get-type");
        AssociatedEvaluator.AddGeneric<GetPath>(ReturnValue,"get-path");
        AssociatedEvaluator.AddGeneric<GetId>(ReturnValue,"get-id");
        AssociatedEvaluator.AddGeneric<GetPathString>(ReturnValue,"get-path-string");


        AssociatedEvaluator.AddType<DatabaseDefinition>(ReturnValue,"db-def_t");
        AssociatedEvaluator.AddType<Message>(ReturnValue,"message_t");
        AssociatedEvaluator.AddType<ResourceHandle>(ReturnValue,"resource-handle_t");

        return ReturnValue;
    }
}
