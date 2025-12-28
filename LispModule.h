#pragma once
#include <MBLisp/Module.h>
#include <MBLisp/Modules/CLI/CLI.h>
#include "DBVisualiser.h"

#include "Client.h"

namespace MBChat2
{

    struct ResourceHandle
    {
        ID Id;
        std::string Type;
        std::vector<std::string> Path;
    };


    class LispVisualiser : public DBVisualiser
    {
        MBLisp::Ref<MBLisp::Scope> m_ChatScope;
        MBLisp::LispWindow m_UnderylingWindow;
    public:
        LispVisualiser(std::shared_ptr<MBLisp::Evaluator> Evaluator,MBLisp::Value Val);
        virtual void Init() override;
        virtual void ResourcePublished(NewMessage const& NewHeader) override;


        virtual bool HandleInput(MBCLI::ConsoleInput const& Input) override;
        virtual MBCLI::Dimensions PreferedDimensions(MBCLI::Dimensions SuggestedDimensions) override;
        virtual void SetFocus(bool IsFocused) override;
        virtual MBCLI::CursorInfo GetCursorInfo() override;
        virtual void WriteBuffer(MBCLI::BufferView View,bool Redraw) override;


        
        MBLisp::Ref<DBConnection> GetConnectionRef();
    };

    class ChatLispModule : public MBLisp::Module
    {
        std::shared_ptr<Client> m_AssociatedClient;
    public:
        ChatLispModule(std::shared_ptr<Client> AssociatedClient)
        {
            m_AssociatedClient = AssociatedClient;   
        }
        virtual MBLisp::Ref<MBLisp::Scope> GetModuleScope(MBLisp::Evaluator& AssociatedEvaluator) override;
    };
}
