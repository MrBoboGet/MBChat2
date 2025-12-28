#pragma once
#include "../DBVisualiser.h"

namespace MBChat2
{
    class ChatVisualiser : public DBVisualiser
    {
    private:
        std::shared_ptr<MBTUI::Stacker> m_Stacker;
        std::shared_ptr<MBTUI::REPL> m_InputField;

        MBCLI::Dimensions m_Dims;
        MBCLI::WindowManager m_TopWindowManager;
    public:
        ChatVisualiser();
        
        virtual void Init() override;
        virtual void ResourcePublished(NewMessage const& NewHeader) override;


        virtual bool HandleInput(MBCLI::ConsoleInput const& Input) override;
        virtual void SetFocus(bool IsFocused) override;
        virtual MBCLI::CursorInfo GetCursorInfo() override;
        virtual void WriteBuffer(MBCLI::BufferView View,bool Redraw) override;
    };
}
