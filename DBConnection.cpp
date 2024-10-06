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
        HeaderToPublish.DatabaseHash = m_DatabaseID;
        if(HeaderToPublish.ParentHash.Content.size() == 0)
        {
            HeaderToPublish.ParentHash = LatestID();   
        }
        m_ConnectionManager->PublishMessage(std::move(HeaderToPublish));
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
