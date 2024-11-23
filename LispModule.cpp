#include "LispModule.h"
#include <MBLisp/Evaluator.h>

namespace MBChat2
{
    LispVisualiser::LispVisualiser(std::shared_ptr<MBLisp::Evaluator> Evaluator,MBLisp::Value Val)
        : MBLisp::LispWindow(Evaluator,Val)
    {
        m_ChatScope = Evaluator->GetModuleScope("mbchat");
    }
    void LispVisualiser::Init() 
    {
        auto Ref = MBLisp::FromShared(GetConnectionPtr());
        auto Value = MBLisp::Value(Ref);
        auto Result = m_Evaluator->Eval(
                m_ChatScope,m_Evaluator->GetValue(*m_ChatScope,"init") ,{m_Value,Value});
    }
    void LispVisualiser::ResourcePublished(NewMessage const& NewHeader) 
    {
        auto Result = m_Evaluator->Eval(
                m_ChatScope,m_Evaluator->GetValue(*m_ChatScope,"resource-published") ,
                {m_Value,MBLisp::Value::EmplaceExternal<NewMessage>(NewHeader)});
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

        AssociatedEvaluator.AddGeneric<AddVisualiser>(ReturnValue,"add-visualiser");
        AssociatedEvaluator.AddGeneric<Connection_DB>(ReturnValue,"db");

        return ReturnValue;
    }
}
