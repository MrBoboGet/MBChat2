#pragma once
#include <string>
#include <vector>
#include <MBUtility/StaticVariant.h>
#include <MBUtility/MBInterfaces.h>
#include <MBUtility/IndeterminateInputStream.h>
#include <MBParsing/MBParsing.h>
#include <MBParsing/StreamSerialize.h>

namespace MBChat2
{
    using MBParsing::operator&;

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
        RPC,
        Close,
        RPCResponse,

        //Kademlia messages
        Store,
        FindNode,
        FindValue,
        Ping
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
        Text
    };
    enum class UploadType : uint32_t
    {
        New,
    };

    struct Hash
    {
        std::vector<uint8_t> Content;

        template<typename T>
        friend void Parse(T& Stream,Hash& Value,uint16_t Version)
        {
            Stream & Value.Content;
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
        Hash ID;
        uint64_t Timestamp = 0;
        uint64_t MaxMessageSize = -1;
        std::vector<Hash> ForkedDatabase;
        std::vector<Key> Participants;
    };

    struct ResourceHeader
    {
        Hash HeaderHash;
        ContentType ContentType = ContentType::Text;
        UploadType UpType = UploadType::New;
        uint64_t TimeStamp = 0;
        uint64_t ContentSize = 0;
        Hash OriginalDatabaseHash;
        Hash ParentHash;
        Hash ContentHash;
        Key Uploader;
        Signature Sig;


        template<typename T>
        friend void Parse(T& Stream,ResourceHeader& Value,uint16_t Version)
        {
            Stream & Value.ContentType;
            Stream & Value.UpType;
            Stream & Value.TimeStamp;
            Stream & Value.ContentSize;

            Parse(Stream,Value.OriginalDatabaseHash,Version);
            Parse(Stream,Value.ParentHash,Version);
            Parse(Stream,Value.ContentHash,Version);
            Parse(Stream,Value.Uploader,Version);
            Parse(Stream,Value.Sig,Version);
            Parse(Stream,Value.HeaderHash,Version);
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
        Key Uploader;
        ContentType ContentType = ContentType::Text;
        UploadType UpType = UploadType::New;
        Hash ParentHash;
        std::string Content;
    };
    struct PublishableResourceHeader
    {
        Key Uploader;
        ContentType ContentType = ContentType::Text;
        UploadType UpType = UploadType::New;
        Hash DatabaseHash;
        Hash ParentHash;
        Hash ContentHash;
        uint64_t ResourceSize = 0;
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
    //Messages

    typedef MBUtility::StaticVariant<Handshake,Publish,GetHeader,HeaderMessage> MessageContent;
   
    template<typename T>
    void Parse(T& Stream,MessageType Type,MessageContent& Value,uint16_t Version)
    {
        if(Type == MessageType::Handshake)
        {
            Parse(Stream,Value.GetOrAssign<Handshake>(),Version);
        }
        else if(Type == MessageType::Publish)
        {
            Parse(Stream,Value.GetOrAssign<Publish>(),Version);
        }
        else if(Type == MessageType::GetHeader)
        {
            Parse(Stream,Value.GetOrAssign<GetHeader>(),Version);
        }
        else if(Type == MessageType::Header)
        {
            Parse(Stream,Value.GetOrAssign<HeaderMessage>(),Version);
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

    struct Message 
    {
        uint16_t Version = 0;
        MessageType Type = MessageType::Handshake;
        uint32_t MessageID = 0;
        uint32_t ResponseID = 0;
        uint64_t MessageLength = 0;
        std::string Signature;


        //template<typename T>
        //void friend operator&(T& Stream,Message&& Rhs)
        //{
        //    Stream & Rhs.Version;
        //    Stream & Rhs.Type;
        //    Stream & Rhs.MessageID;
        //    Stream & Rhs.ResponseID;
        //    Stream & Rhs.MessageLength;
        //    Stream & Rhs.Signature;
        //    Parse(Stream,Rhs.Type,Rhs.Content);
        template<typename T>
        friend void ParseHeader(T& Stream,Message& Value)
        {
            Stream & Value.Version;
            Stream & Value.Type;
            Stream & Value.MessageID;
            Stream & Value.ResponseID;
            Stream & Value.MessageLength;
            Stream & Value.Signature;
        }
        template<typename T>
        friend void ParseBody(T& Stream,Message& Value)
        {
            Parse(Stream,Value.Type,Value.Content,Value.Version);
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
