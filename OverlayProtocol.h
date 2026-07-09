#pragma once
#include <string>
#include <vector>
#include <MBUtility/StaticVariant.h>
#include <MBUtility/MBInterfaces.h>
#include <MBUtility/IndeterminateInputStream.h>
#include <MBParsing/MBParsing.h>
#include <MBParsing/StreamSerialize.h>


#include <MBCrypto/MBCrypto.h>

namespace MBChat2
{
    using MBParsing::operator&;

    typedef int64_t TimestampType;

    typedef std::vector<uint8_t> ID;
    enum class MessageType : uint32_t
    {
        Handshake = 0,
        Publish,
        GetHeader,
        Header,
        GetContent,
        Content,
        STUNRequest,
        STUNResponse,
        GetPeers,
        Peers,
        JSONRPC,
        RPC,
        Close,
        RPCResponse,

        GetResources,
        ResourceHeader,
        //Kademlia messages
        Store,
        FindNode,
        FindValue,
        Ping
    };


    ID Distance(ID const& Lhs,ID const& Rhs);
    std::string IDToString(ID const& IDToConvert);
    ID StringToID(std::string const& StringToConvert);

    template<MessageType Type>
    struct MessageBase
    {
        static constexpr MessageType type = Type;
    };

    enum class ExtensionType
    {
        Optional,
        Mandatory,
    };
    struct Extension
    {
        ExtensionType Type = ExtensionType::Optional;
        uint16_t Length = 0;

        template<typename T>
        friend void Parse(T& Stream,Extension& Value,uint16_t Version)
        {
            Stream & Value.Type;
            Stream & Value.Length;
        }
    };

    enum class ContentType : uint32_t
    {
        Text,
    };
    enum class UploadType : uint32_t
    {
        New,
    };

    struct Hash
    {
        std::vector<uint8_t> Content;

        Hash() = default;
        Hash(ID HashContent)
        {
            Content = std::move(HashContent);
        }

        bool operator==(Hash const& rhs) const
        {
            return Content == rhs.Content;
        }

        uint64_t SerializedSize()
        {
            uint64_t ReturnValue = 2+Content.size();
            return ReturnValue;
        }

        template<typename T>
        friend void Parse(T& Stream,Hash& Value,uint16_t Version)
        {
            Stream & Value.Content;
        }
        template<typename T>
        friend void Parse(T& Stream,Hash& Value)
        {
            Stream & Value.Content;
        }
        template<typename T>
        friend void operator&(T& Stream,Hash& Rhs)
        {
            Stream & Rhs.Content;
        }


        bool operator<(Hash const& rhs) const
        {
            return Content < rhs.Content;   
        }
    };
    struct Key
    {
        std::string Content;
        template<typename T>
        friend void Parse(T& Stream,Key& Value,uint16_t Version)
        {
            Stream & Value.Content;
        }
    };
    struct Signature
    {
        std::string Content;
        template<typename T>
        friend void Parse(T& Stream,Signature& Value,uint16_t Version)
        {
            Stream & Value.Content;
        }
    };






    struct DatabaseDefinition
    {
        Hash DatabaseID;
        uint64_t Timestamp = 0;
        std::string Type;
        std::vector<Hash> ForkedDatabase;
        std::vector<Hash> Participants;

        void CalculateHash()
        {
            std::string OutString;
            MBUtility::MBStringOutputStream OutStream(OutString);
            OutStream & Timestamp;
            OutStream & Type;
            OutStream & ForkedDatabase;
            OutStream & Participants;
            auto Hash = MBCrypto::HashData(OutString,MBCrypto::HashFunction::SHA256);
            DatabaseID.Content.resize(Hash.size());
            std::memcpy(DatabaseID.Content.data(),Hash.data(),Hash.size());
        }
    };

    struct ResourceHeader : public MessageBase<MessageType::ResourceHeader>
    {
        Hash HeaderHash;
        std::string Type;
        UploadType UpType = UploadType::New;
        uint64_t TimeStamp = 0;
        uint64_t ContentSize = 0;
        Hash OriginalDatabaseHash;
        Hash ParentHash;
        std::string Name;
        Hash ContentHash;
        Hash Uploader;
        Signature Sig;

        template<typename T>
        friend void ParseHashedContent(T& Stream,ResourceHeader& Value,uint16_t Version)
        {
            Stream & Value.Type;
            Stream & Value.UpType;
            Stream & Value.TimeStamp;
            Stream & Value.ContentSize;
            Parse(Stream,Value.OriginalDatabaseHash,Version);
            Parse(Stream,Value.ParentHash,Version);
            Stream & Value.Name;
            Parse(Stream,Value.ContentHash,Version);
            Parse(Stream,Value.Uploader,Version);
            Parse(Stream,Value.Sig,Version);
        }
        ID CalculateHeaderHash()
        {
            std::string OutContent;
            MBUtility::MBStringOutputStream OutStream(OutContent);
            ParseHashedContent(OutStream,*this,0);
            return StringToID(MBCrypto::HashData(OutContent,MBCrypto::HashFunction::SHA256));
        }
        template<typename T>
        friend void Parse(T& Stream,ResourceHeader& Value,uint16_t Version)
        {
            ParseHashedContent(Stream,Value,Version);
            Parse(Stream,Value.HeaderHash,Version);
        }
        template<typename T>
        friend void Parse(T& Stream,ResourceHeader& Value)
        {
            Parse(Stream,Value,0);
        }
        template<typename T>
        friend void operator&(T& Stream,ResourceHeader& Value)
        {
            Parse(Stream,Value,0);
        }
    };

    struct Peer
    {
        Key ID;
    };


    struct PeerInfo
    {
        Hash ID;
        std::string Nick;
        bool PersistentIP = false;
        uint32_t IP = 0;
        uint16_t ListeningPort = 0;
        PeerInfo() = default;
        PeerInfo( PeerInfo const& ) = default;
        PeerInfo( PeerInfo && ) noexcept = default;
        PeerInfo& operator=(PeerInfo const&) = default;
        PeerInfo& operator=(PeerInfo&&) noexcept = default;
        
        bool operator==(PeerInfo const& rhs) const
        {
            return std::tie(ID,Nick,PersistentIP,IP,ListeningPort) == std::tie(rhs.ID,rhs.Nick,rhs.PersistentIP,rhs.IP,rhs.ListeningPort);   
        }
        template<typename T>
        friend void Parse(T& Stream,PeerInfo& Value)
        {
            Parse(Stream,Value.ID);
            Stream & Value.Nick;
            Stream & Value.PersistentIP;
            Stream & Value.IP;
            Stream & Value.ListeningPort;
        }
        template<typename T>
        friend void operator&(T& Stream,PeerInfo& Value)
        {
            Parse(Stream,Value);
        }
    };
    struct ConnectionMetadata
    {
        bool PersistentIP = false;
        uint16_t ListeningPort = 0;

        template<typename T>
        friend void Parse(T& Stream,ConnectionMetadata& Value,uint16_t Version)
        {
            Stream & Value.PersistentIP;
            Stream & Value.ListeningPort;
        }
    };
    struct ResourceContent
    {
        std::string Type;
        UploadType UpType = UploadType::New;
        Hash ParentHash;
        std::string Name;
        std::string Content;
    };
    struct PublishableResourceHeader
    {
        std::string Type;
        UploadType UpType = UploadType::New;
        Hash DatabaseHash;
        Hash ParentHash;
        std::string Name;
        Hash ContentHash;
        std::string Content;
    };
    
    //Messages
    struct FindValue
    {
           
    };
    struct FindNode
    {
           
    };
    struct Store
    {
           
    };
    
    struct Handshake
    {
        std::string Nick;
        ConnectionMetadata ConnectionInfo;
        std::vector<Extension> Extensions;

        template<typename T>
        friend void Parse(T& Stream,Handshake& Value,uint16_t Version)
        {
            Stream & Value.Nick;
            Parse(Stream,Value.ConnectionInfo,Version);
            Parse(Stream,Value.Extensions,Version);
        }
    };
    struct Publish
    {
        ResourceHeader Header;
        template<typename T>
        friend void Parse(T& Stream,Publish& Value,uint16_t Version)
        {
            Parse(Stream,Value.Header,Version);
        }
    };
    struct GetHeader
    {
        Hash DatabaseHash;
        Hash ResourceHash;
        template<typename T>
        friend void Parse(T& Stream,GetHeader& Value,uint16_t Version)
        {
            Parse(Stream,Value.DatabaseHash,Version);
            Parse(Stream,Value.ResourceHash,Version);
        }
    };
    struct HeaderMessage
    {
        ResourceHeader Header;
        template<typename T>
        friend void Parse(T& Stream,HeaderMessage& Value,uint16_t Version)
        {
            Parse(Stream,Value.Header,Version);
        }
    };
    struct GetContent
    {
        Hash DatabaseHash;
        Hash ResourceHash;
        template<typename T>
        friend void Parse(T& Stream,GetContent& Value,uint16_t Version)
        {
            Parse(Stream,Value.DatabaseHash,Version);
            Parse(Stream,Value.ResourceHash,Version);
        }
    };
    struct Content
    {
        //No content is actually present, as it cannot be assumed to fit in memory
        //or can it...
    };
    struct STUNRequest
    {
        Key PeerID;
        uint16_t Port;
    };
    enum class STUNResult
    {
        Refused,
        NotFound,
        Accepted
    };

    struct GetResources : public MessageBase<MessageType::GetResources>
    {
        Hash DBHash;
        TimestampType StartTime;
        TimestampType EndTime;
        uint64_t MaxInlineSize = 1500;

        uint64_t SerializedSize()
        {
            uint64_t  ReturnValue = 0;
            ReturnValue += DBHash.SerializedSize();
            ReturnValue += sizeof(StartTime);
            ReturnValue += sizeof(EndTime);
            ReturnValue += sizeof(MaxInlineSize);
            return ReturnValue;
        }

        template<typename T>
        friend void Parse(T& Stream,GetResources& Value,uint16_t Version)
        {
            Parse(Stream,Value.DBHash,Version);
            Stream & Value.StartTime;
            Stream & Value.EndTime;
            Stream & Value.MaxInlineSize;
        }
    };


    struct ResourceResponse
    {
        ResourceHeader Header;
        std::string Content;

        template<typename T>
        friend void Parse(T& Stream,ResourceResponse& Value,uint16_t  Version)
        {
            Parse(Stream,Value.Header);
            Stream & Value.Content;
        }
        template<typename T>
        friend void Parse(T& Stream,ResourceResponse& Value)
        {
            Parse(Stream,Value,0);
        }
    };

    struct STUNResponse
    {
        STUNResult Result = STUNResult::Refused;
        uint32_t Address;
        uint16_t Port;
    };
    struct GetPeers
    {

    };
    struct Peers
    {
        std::vector<Peer> Result;
    };
    struct Close
    {
        std::vector<Peer> Result;
    };

    struct JSONRPC : public MessageBase<MessageType::GetResources>
    {
        static constexpr MessageType TypeID = MessageType::JSONRPC;
        MBParsing::JSONObject Object;
        template<typename T>
        friend void Parse(T& Stream,JSONRPC& Value,uint16_t Version)
        {
            if(MBParsing::Writing<T>())
            {
                std::string SerializedString = Value.Object.ToString();
                Stream & SerializedString;
            }
            else
            {
                std::string Content;
                Stream & Content;

                size_t ParseOffset = 0;
                MBError Error = true;
                Value.Object = MBParsing::ParseJSONObject(Content,0,&ParseOffset,&Error);
                if(!Error)
                {
                    throw std::runtime_error("Error parsing JSONRPC request: invalid JSON: "+Error.ErrorMessage);
                }
            }
        }
    };
    struct GetResourceContent_Message : public MessageBase<MessageType::GetContent>
    {
        static constexpr MessageType TypeID = MessageType::GetContent;
        ID DatabaseID;
        ID ResourceID;
        uint64_t Offset = 0;
        uint64_t Length = 0;
        template<typename T>
        friend void Parse(T& Stream,GetResourceContent_Message& Value,uint16_t Version)
        {
            Stream & Value.DatabaseID;
            Stream & Value.ResourceID;
            Stream & Value.Offset;
            Stream & Value.Length;
        }
    };
    struct ResourceContentResponse : public MessageBase<MessageType::Content>
    {
        static constexpr MessageType TypeID = MessageType::Content;
        bool HasContent = true;
        std::string Content;
        template<typename T>
        friend void Parse(T& Stream,ResourceContentResponse& Value,uint16_t Version)
        {
            Stream & Value.HasContent;
            Stream & Value.Content;
        }
    };
    //Messages

    typedef MBUtility::StaticVariant<JSONRPC,GetResources,GetResourceContent_Message,ResourceContentResponse,ResourceHeader> MessageContent;
   
    template<typename T>
    void Parse(T& Stream,MessageType Type,MessageContent& Value,uint16_t Version)
    {
        if(Type == MessageType::GetResources)
        {
            Parse(Stream,Value.GetOrAssign<GetResources>(),Version);
        }
        else if(Type == MessageType::JSONRPC)
        {
            Parse(Stream,Value.GetOrAssign<JSONRPC>(),Version);
        }
        else if(Type == MessageType::ResourceHeader)
        {
            Parse(Stream,Value.GetOrAssign<ResourceHeader>(),Version);
        }
        else if(Type == MessageType::GetContent)
        {
            Parse(Stream,Value.GetOrAssign<GetResourceContent_Message>(),Version);
        }
        else if(Type == MessageType::Content)
        {
            Parse(Stream,Value.GetOrAssign<ResourceContentResponse>(),Version);
        }

    }
    template<typename T,typename V>
    void Parse(T& Stream,std::vector<V>& Value,uint16_t Version)
    {
        if constexpr(MBParsing::Writing<T>())
        {
            uint16_t Size = Value.size();
            Stream & Size;
            for(auto& Elem : Value)
            {
                Parse(Stream,Elem,Version);
            }
        }
        else
        {
            uint16_t Size = MBParsing::ParseBigEndianInteger(Stream,2);
            Value.reserve(Size);
            for(size_t i = 0; i < Size;i++)
            {
                auto& NewValue = Value.emplace_back();
                Parse(Stream,NewValue,Version);
            }
        }
    }
    struct MessageHeader
    {
        uint16_t Version = 0;
        MessageType Type = MessageType::Handshake;
        uint32_t MessageID = 0;
        uint32_t ResponseID = 0;
        uint64_t MessageLength = 0;


        template<typename T>
        friend void operator&(T& Stream,MessageHeader& Header)
        {
            Stream & Header.Version;
            Stream & Header.Type;
            Stream & Header.MessageID;
            Stream & Header.ResponseID;
            Stream & Header.MessageLength;
        }
    };
    struct Message 
    {

        MessageHeader Header;
        template<typename T>
        friend void ParseHeader(T& Stream,Message& Value)
        {
            Stream & Value.Header;
        }
        template<typename T>
        friend void ParseBody(T& Stream,Message& Value)
        {
            Parse(Stream,Value.Header.Type,Value.Content,Value.Header.Version);
        }
        template<typename T>
        friend void Parse(T& Stream,Message& Value)
        {
            ParseHeader(Stream,Value);
            ParseBody(Stream,Value);
        }
        

        //void ReadHeader(MBUtility::IndeterminateInputStream& InputStream);
        void ReadMessageHeader(MBUtility::IndeterminateInputStream& InputStream);
        void ReadMessage(MBUtility::IndeterminateInputStream& InputStream);
        void ReadBody(MBUtility::IndeterminateInputStream& InputStream);
        void WriteMessage(MBUtility::MBOctetOutputStream& OutStream);
        MessageContent Content;
    };
}

template<>
struct std::hash<MBChat2::PeerInfo>
{
    size_t operator()(MBChat2::PeerInfo const& p) const
    {
        return std::hash<uint32_t>()(p.IP);
    }
};
template<>
struct std::hash<MBChat2::ID>
{
    size_t operator()(MBChat2::ID const& ID) const
    {
        return std::hash<std::string_view>()(std::string_view((const char*) ID.data(),
                    ID.size()));
    }
};
