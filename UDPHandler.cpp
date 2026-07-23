#include "UDPHandler.h"
#include <unordered_set>

namespace MBChat2
{
    UDPHandler::UDPHandler(uint16_t ListenPort,UDPRequestHandler RequestHandler,UDPNotificationHandler NotificationHandler)
        : m_Socket(ListenPort),m_RequestHandler(std::move(RequestHandler)),m_NotificationHandler(std::move(NotificationHandler))
    {
        m_ListenThread = std::thread(&UDPHandler::p_ReadThread,this);
    }
    void UDPHandler::RegisterTCPListener(uint32_t ConnectionID,uint32_t ClientIP,MBUtility::MOFunction<void(std::string_view)> Callback)
    {
        std::lock_guard Lock(m_StateMutex);
        auto& Listener = m_ActivePacketListeners[ClientIP][ConnectionID] = TCPListener();
        Listener.ID = ConnectionID;
        Listener.ClientIP = ClientIP;
        Listener.PacketRecievedCallback = std::move(Callback);
    }
    void UDPHandler::RemoveTCPListener(uint32_t ConnectionID,uint32_t ClientIP)
    {
        std::lock_guard Lock(m_StateMutex);
        m_ActivePacketListeners[ClientIP].erase(m_ActivePacketListeners[ClientIP].find(ConnectionID));
    }
    void UDPHandler::SendTCPPacket(uint32_t ConnectionID,uint32_t ClientIP,uint16_t Port,std::string_view Data)
    {
        std::string DataToSend;
        DataToSend.reserve(8+Data.size());
        MBUtility::MBStringOutputStream OutStream(DataToSend);
        OutStream & UDPMessageType::TCPData;
        OutStream & ConnectionID;
        OutStream << Data;
        m_Socket.UDPSendData(DataToSend.data(),DataToSend.size(),ClientIP,Port);
    }
    void UDPHandler::p_SendMessage(StoredMessage const& MessageToSend)
    {
        m_Socket.UDPSendData(MessageToSend.SerializedContent.data(),MessageToSend.SerializedContent.size(),MessageToSend.IP,MessageToSend.Port);
    }
    void UDPHandler::p_SendResponse(uint32_t IP,uint16_t Port,uint32_t ID,std::string const& Content)
    {
        std::string DataToSend;
        MBUtility::MBStringOutputStream OutStream(DataToSend);
        OutStream & UDPMessageType::Response;
        OutStream & ID;
        OutStream << Content;
        m_Socket.UDPSendData(DataToSend.data(),DataToSend.size(),IP,Port);
    }
    void UDPHandler::p_ReadThread()
    {
        typedef std::chrono::steady_clock Time;
        typedef std::chrono::milliseconds ms;
        typedef std::chrono::duration<float> fsec;
        
        std::unordered_map<uint32_t,std::unordered_set<uint32_t>> RecievedResponses;
        double WaitTimer = 0;
        while(!m_Stopping.load())
        {
            std::string Buffer(m_Socket.ReadMaxPacketSize(),0);
            MBSockets::UDPSource Source;
            size_t RequestSize = m_Socket.ReadPacket(Source,Buffer.data(),Buffer.size(),WaitTimer);
            //std::cout<< "Packet recieved"<<std::endl;
            MessageLocation Location;
            Location.IP  = Source.IP;
            Location.Port  = Source.Port;
            std::lock_guard Lock(m_StateMutex);
            if(RequestSize > 0)
            {
                MBUtility::MBBufferInputStream InputStream(Buffer.data(),Buffer.size());
                try
                {
                    UDPMessageType Type;
                    InputStream & Type;
                    uint32_t ID;
                    InputStream & ID;
                    if(Type ==  UDPMessageType::Notification)
                    {
                        UDPNotification Notification;
                        ReadVariant(InputStream,Notification);
                        m_ThreadPool.AddTask([&,ResponseID=ID,Location=Location,Notification=std::move(Notification)]()
                                {
                                    try
                                    {
                                        m_NotificationHandler(Location,Notification);
                                    }
                                    catch(...)
                                    {
                                        
                                    }
                                    std::string ResponseContent;
                                    MBUtility::MBStringOutputStream OutStream(ResponseContent);
                                    Ack Ack;
                                    Parse(OutStream,Ack);
                                    p_SendResponse(Location.IP,Location.Port,ResponseID,ResponseContent);
                                });
                    }
                    else if(Type == UDPMessageType::Request)
                    {
                        UDPRequest Request;
                        ReadVariant(InputStream,Request);
                        m_ThreadPool.AddTask([&,RequestID=ID,Request=std::move(Request)]()
                                {
                                    try
                                    {
                                        auto Response = m_RequestHandler(Location,Request);
                                        std::string ResponseContent;
                                        MBUtility::MBStringOutputStream OutStream(ResponseContent);
                                        uint8_t Type = Response.Visit([](auto const& Value)
                                                    {
                                                        return Value.type;
                                                    });
                                        WriteVariant(OutStream,Type ,Response);
                                        p_SendResponse(Location.IP,Location.Port,RequestID,ResponseContent);
                                    }
                                    catch(...)
                                    {
                                        
                                    }
                                });

                    }
                    else if(Type == UDPMessageType::TCPData)
                    {
                        auto ClientIt = m_ActivePacketListeners.find(Location.IP);
                        if(ClientIt == m_ActivePacketListeners.end())
                        {
                             continue;   
                        }
                        auto ListenerIt = ClientIt->second.find(ID);
                        if(ListenerIt == ClientIt->second.end())
                        {
                            continue;
                        }
                        //thread pool perhaps?
                        auto Offset = sizeof(UDPMessageType)+sizeof(uint32_t);
                        ListenerIt->second.PacketRecievedCallback(std::string_view(Buffer.data()+Offset,Buffer.data()+RequestSize));
                    }
                    else if(Type == UDPMessageType::Response)
                    {
                        UDPResponse Response;
                        ReadVariant(InputStream,Response);
                        auto PeerIt = m_ResponseCallbacks.find(Location.IP);
                        if(PeerIt != m_ResponseCallbacks.end())
                        {
                            auto It = PeerIt->second.find(ID);
                            if(It != PeerIt->second.end())
                            {
                                auto Callback = std::move(It->second.Callback);
                                PeerIt->second.erase(It);
                                m_ThreadPool.AddTask([&,Response=Response,Callback= std::move(Callback)] () mutable
                                        {
                                            Callback(Response);
                                        });
                                RecievedResponses[Location.IP].insert(ID);
                            }
                        }
                    }
                }
                catch(...)
                {
                       
                }
            }
            //check which stuff should be resent
            auto CurrentTime = std::chrono::steady_clock::now();
            while(m_SentMessages.size() > 0 && m_SentMessages.front().RetryTime <= CurrentTime)
            {
                auto& MessageToSend = m_SentMessages.front();
                bool ResponseRecieved = false;
                auto IpIt = RecievedResponses.find(MessageToSend.IP);
                decltype(IpIt->second.begin()) IdIt;
                if(IpIt != RecievedResponses.end())
                {
                    IdIt = IpIt->second.find(MessageToSend.ID);
                    if(IdIt != IpIt->second.end())
                    {
                        ResponseRecieved = true;
                    }
                }
                if(MessageToSend.AttemptedRetries < MessageToSend.MaxRetries && !ResponseRecieved)
                {
                    p_SendMessage(MessageToSend);
                    MessageToSend.AttemptedRetries += 1;
                    MessageToSend.RetryTime = CurrentTime + std::chrono::milliseconds(500);
                    WaitTimer = 0.5;
                    std::make_heap(m_SentMessages.begin(),m_SentMessages.end());
                }
                else
                {
                    std::pop_heap(m_SentMessages.begin(),m_SentMessages.end(),std::greater<StoredMessage>());
                    m_SentMessages.resize(m_SentMessages.size()-1);
                    if(ResponseRecieved)
                    {
                        IpIt->second.erase(IdIt);
                    }
                }
            }
            if(m_SentMessages.size() == 0)
            {
                WaitTimer = 0;
            }
        }
    }
}
