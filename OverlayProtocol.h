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
        std::string Content;

        template<typename T>
        friend void Parse(T& Stream,Hash&& Value,uint16_t Version)
        {
            Stream & Value.Content;
        }
    };
    struct Key
    {
        std::string Content;
        template<typename T>
        friend void Parse(T& Stream,Key&& Value,uint16_t Version)
        {
            Stream & Value.Content;
        }
    };
    struct Signature
    {
        std::string Content;
        template<typename T>
        friend void Parse(T& Stream,Signature&& Value,uint16_t Version)
        {
            Stream & Value.Content;
        }
    };






    struct DatabaseDefinition
    {
        uint64_t Timestamp = 0;
        uint64_t MaxMessageSize = -1;
        std::vector<Key> Authors;
        std::vector<Hash> ForkedDatabase;
        std::vector<Hash> Participants;
        std::vector<Signature> AuthorSignatures;
    };

    struct ResourceHeader
    {
        ContentType ContentType = ContentType::Text;
        UploadType UpType = UploadType::New;
        uint64_t TimeStamp = 0;
        uint64_t ResourceSize = 0;
        Hash OriginalDatabaseHash;
        Hash ParentHash;
        Hash ContentHash;
        Key Uploader;
        Signature Sig;
        Hash HeaderHash;


        template<typename T>
        friend void Parse(T& Stream,ResourceHeader&& Value,uint16_t Version)
        {
            Stream & Value.ContentType;
            Stream & Value.UpType;
            Stream & Value.TimeStamp;
            Stream & Value.ResourceSize;

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

    struct ConnectionMetadata
    {
        bool PersistentIP = false;
        uint16_t ListeningPort = 0;
    };
    
    //Messages
    struct Handshake
    {
        std::string Nick;
        ConnectionMetadata ConnectionInfo;
        std::vector<Extension> Extensions;

        template<typename T>
        friend void Parse(T& Stream,Handshake&& Value,uint16_t Version)
        {
            Stream & Value.Nick;
            Parse(Stream,Value.ConnectionInfo,Version);
            Stream & Value.Extensions;
        }
    };
    struct Publish
    {
        ResourceHeader Header;
        template<typename T>
        friend void Parse(T& Stream,Publish&& Value,uint16_t Version)
        {
            Parse(Stream,Value.Header,Version);
        }
    };
    struct GetHeader
    {
        Hash DatabaseHash;
        Hash ResourceHash;
        template<typename T>
        friend void Parse(T& Stream,GetHeader&& Value,uint16_t Version)
        {
            Parse(Stream,Value.DatabaseHash,Version);
            Parse(Stream,Value.ResourceHash,Version);
        }
    };
    struct Header
    {
        ResourceHeader Header;
        template<typename T>
        friend void Parse(T& Stream,struct Header&& Value,uint16_t Version)
        {
            Parse(Stream,Value.Header,Version);
        }
    };
    struct GetContent
    {
        Hash DatabaseHash;
        Hash ResourceHash;
        template<typename T>
        friend void Parse(T& Stream,GetContent&& Value,uint16_t Version)
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

    typedef MBUtility::StaticVariant<Handshake,Publish,GetHeader,Header> MessageContent;
   
    template<typename T>
    void Parse(T& Stream,MessageType Type,MessageContent&& Value,uint16_t Version)
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
            Parse(Stream,Value.GetOrAssign<Header>(),Version);
        }
    }
    template<typename T,typename V>
    void Parse(T& Stream,std::vector<V>&& Value,uint16_t Version)
    {
        if constexpr(MBParsing::Writing<T>())
        {
            uint16_t Size = Value.size();
            Stream & Size;
            for(auto const& Elem : Value)
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
        //}
        template<typename T>
        friend void Parse(T& Stream,Message&& Value)
        {
            Stream & Value.Version;
            Stream & Value.Type;
            Stream & Value.MessageID;
            Stream & Value.ResponseID;
            Stream & Value.MessageLength;
            Stream & Value.Signature;
            Parse(Stream,Value.Type,Value.Content,Value.Version);
        }

        //void ReadHeader(MBUtility::IndeterminateInputStream& InputStream);
        void ReadMessage(MBUtility::IndeterminateInputStream& InputStream);
        void WriteMessage(MBUtility::MBOctetOutputStream& OutStream);
        MessageContent Content;
    };
}
