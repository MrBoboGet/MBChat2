#pragma once

#include <MBCLI/Window.h>
#include "OverlayProtocol.h"
#include "DBConnection.h"
#include <MBTUI/Stacker.h>
#include <MBTUI/MBTUI.h>
namespace MBChat2
{
    class DBVisualiser : public virtual MBCLI::Window
    {
    private:
        std::shared_ptr<DBConnection> m_DBConnection;
    protected:
        DBConnection& GetConnection()
        {
            return *m_DBConnection;   
        }
        std::shared_ptr<DBConnection> GetConnectionPtr()
        {
            return m_DBConnection;   
        }
    public:
        void SetDBConnection(std::shared_ptr<DBConnection> Connection)
        {
            m_DBConnection = std::move(Connection);
        }
        virtual void Init()
        {
               
        }
        virtual void ResourcePublished(NewMessage const& NewHeader)
        {
               
        }
    };
}
