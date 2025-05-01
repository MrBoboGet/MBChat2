#pragma once
#include "OverlayProtocol.h"
#include <MBUtility/Future.h>

#include <MrBoboDatabase/MrBoboDatabase.h>

#include "Connection.h"

namespace MBChat2
{
    class DBConnection
    {

        std::shared_ptr<ConnectionManager> m_ConnectionManager;
        std::shared_ptr<MBDB::MrBoboDatabase> m_Database;

        ID m_DatabaseID;
    public:

        DBConnection(std::shared_ptr<ConnectionManager> ConnectionManager,
                std::shared_ptr<MBDB::MrBoboDatabase> Database,
                ID DatabaseID)
        {
            m_ConnectionManager = std::move(ConnectionManager);   
            m_Database = std::move(Database);
            m_DatabaseID = std::move(DatabaseID);
        }
        virtual void UploadResource(ResourceContent NewResource);
        virtual std::vector<std::string> GetAbsoluteResourcePath(ID const& ResourceRoot,MBDB::IntType& ParentID,MBDB::IntType& OutID);
        virtual ID LatestID();
        ID GetDBID()
        {
            return m_DatabaseID;   
        }
        virtual std::shared_ptr<MBDB::MrBoboDatabase> GetDB();
    };
}
