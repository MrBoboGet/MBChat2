#pragma once

#include <MBCLI/Window.h>
#include "OverlayProtocol.h"
#include "DBConnection.h"
#include <MBTUI/Stacker.h>
#include <MBTUI/MBTUI.h>
namespace MBChat2
{
    class DBVisualiser : public MBCLI::Window
    {

        std::shared_ptr<DBConnection> m_DBConnection;
        std::shared_ptr<MBTUI::Stacker> m_Stacker;
        std::shared_ptr<MBTUI::REPL> m_InputField;

        MBCLI::Dimensions m_Dims;
        MBCLI::WindowManager m_TopWindowManager;
    public:
        DBVisualiser();
        
        virtual void SetDBConnection(std::shared_ptr<DBConnection> Connection);
        virtual void ResourcePublished(NewMessage const& NewHeader);

        //
        virtual bool Updated() override;
        virtual void HandleInput(MBCLI::ConsoleInput const& Input) override;
        virtual void SetDimensions(MBCLI::Dimensions NewDimensions) override;
        virtual void SetFocus(bool IsFocused) override;
        virtual MBCLI::CursorInfo GetCursorInfo() override;
        virtual MBCLI::TerminalWindowBuffer GetBuffer() override;
    };
}
