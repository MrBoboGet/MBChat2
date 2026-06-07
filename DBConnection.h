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
        virtual ID UploadResource(ResourceContent NewResource);
        virtual ID UploadFile(ResourceContent NewResource,std::filesystem::path const& AbsolutePath,bool Copy=true);
        virtual void RemoveResource(ID const& ResourceID);
        virtual void RemoveResource(std::vector<std::string> const& Path);
        virtual bool GetResourceByID(ID const& ID,ResourceHeader& OutResource);
        virtual ResourceStateHandle GetStateHandle(ID const& ID);


        virtual std::vector<std::string> GetAbsoluteResourcePath(ID const& ResourceRoot,MBDB::IntType& ParentID,MBDB::IntType& OutID);
        virtual std::vector<std::string> GetAbsoluteResourcePath(ID const& ResourceRoot);
        virtual ID LatestID();
        ID GetDBID()
        {
            return m_DatabaseID;   
        }
        virtual std::shared_ptr<MBDB::MrBoboDatabase> GetDB();
    };
}
