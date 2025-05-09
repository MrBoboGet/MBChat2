#include "DBConnection.h"

namespace MBChat2
{
    void DBConnection::UploadResource(ResourceContent NewResource)
    {
        PublishableResourceHeader HeaderToPublish;
        HeaderToPublish.ContentHash = StringToID(MBCrypto::HashData(NewResource.Content,MBCrypto::HashFunction::SHA256));
        HeaderToPublish.Content = std::move(NewResource.Content);
        HeaderToPublish.ParentHash = std::move(NewResource.ParentHash);
        HeaderToPublish.Type = NewResource.Type;
        HeaderToPublish.UpType = NewResource.UpType;
        HeaderToPublish.Name = NewResource.Name;
        if(HeaderToPublish.Name.empty())
        {
            HeaderToPublish.Name = IDToString(HeaderToPublish.ContentHash.Content);
        }
        HeaderToPublish.DatabaseHash = m_DatabaseID;
        if(HeaderToPublish.ParentHash.Content.size() == 0)
        {
            HeaderToPublish.ParentHash = LatestID();   
        }
        m_ConnectionManager->PublishMessage(std::move(HeaderToPublish));
    }
    void DBConnection::RemoveResource(ID const& ResourceID)
    {
        m_ConnectionManager->RemoveResource(m_DatabaseID,ResourceID);
    }
    void DBConnection::RemoveResource(std::vector<std::string> const& Path)
    {
        m_ConnectionManager->RemoveResource(m_DatabaseID,Path);
    }
    std::vector<std::string> DBConnection::GetAbsoluteResourcePath(ID const& ResourceRoot,MBDB::IntType& ParentID,MBDB::IntType& OutID)
    {
        return m_ConnectionManager->GetAbsoluteResourcePath(GetDBID(),ResourceRoot,ParentID,OutID);
    }
    ID DBConnection::LatestID()
    {
        ID ReturnValue;
        auto Stmt = m_Database->
            GetSQLStatement("SELECT * FROM Resources WHERE DatabaseHash=:DBHash ORDER BY RecievedTimestamp DESC LIMIT 1");
        Stmt.BindBlob("DBHash",m_DatabaseID);
        auto Results = m_Database->GetAllRows(Stmt);
        if(Results.size() == 0)
        {
            ReturnValue = m_DatabaseID;
        }
        else
        {
            auto& Result = Results[0];
            ReturnValue = StringToID(std::get<std::string>(Result["Hash"]));
        }
        return ReturnValue;
    }
    std::shared_ptr<MBDB::MrBoboDatabase> DBConnection::GetDB()
    {
        return m_Database;
    }
}
