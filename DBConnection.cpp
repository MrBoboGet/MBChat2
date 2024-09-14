#include "DBConnection.h"

namespace MBChat2
{
    MBUtility::Future<ResourceHeader> DBConnection::GetHeader(Hash const& ResourceHash)
    {
        return MBUtility::Promise<ResourceHeader>().GetFuture();
    }
    MBUtility::Future<ResourceContent> DBConnection::GetContent(Hash const& ResourceHash)
    {
        return MBUtility::Promise<ResourceContent>().GetFuture();
    }
    void DBConnection::UploadResource(ResourceContent NewResource)
    {
        PublishableResourceHeader HeaderToPublish;
        HeaderToPublish.ContentHash = StringToID(MBCrypto::HashData(NewResource.Content,MBCrypto::HashFunction::SHA256));
        HeaderToPublish.Content = std::move(NewResource.Content);
        HeaderToPublish.ParentHash = std::move(NewResource.ParentHash);
        HeaderToPublish.Type = NewResource.Type;
        HeaderToPublish.UpType = NewResource.UpType;
        HeaderToPublish.DatabaseHash = m_DatabaseID;

        m_ConnectionManager->PublishMessage(std::move(HeaderToPublish));
    }
    std::shared_ptr<MBDB::MrBoboDatabase> DBConnection::GetDB()
    {
        return m_Database;
    }
}
