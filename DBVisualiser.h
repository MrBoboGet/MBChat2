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
    protected:
        ID m_DBID;
        std::shared_ptr<DBConnection> m_DBConnection;
    private:
        std::shared_ptr<MBTUI::Stacker> m_Stacker;
        std::shared_ptr<MBTUI::REPL> m_InputField;

        MBCLI::Dimensions m_Dims;
        MBCLI::WindowManager m_TopWindowManager;
    public:
        DBVisualiser();
        
        virtual void SetDBID(ID DBID)
        {
            m_DBID = std::move(DBID);
        }
        virtual void SetDBConnection(std::shared_ptr<DBConnection> Connection)
        {
            m_DBConnection = std::move(Connection);
        }
        virtual void Init();
        virtual void ResourcePublished(NewMessage const& NewHeader);

        //
        virtual bool Updated() override;
        virtual void HandleInput(MBCLI::ConsoleInput const& Input) override;
        virtual void SetFocus(bool IsFocused) override;
        virtual MBCLI::CursorInfo GetCursorInfo() override;
        virtual void WriteBuffer(MBCLI::BufferView View,bool Redraw) override;
    };
}
