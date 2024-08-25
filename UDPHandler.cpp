#include "UDPHandler.h"

namespace MBChat2
{
    UDPHandler::UDPHandler()
        : m_Socket(0,0,0)
    {
        m_Socket.Bind(0);//find some good candidate port
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

        double WaitTimer = 0.5;
        while(!m_Stopping.load())
        {
            std::string Buffer(m_Socket.ReadMaxPacketSize(),0);
            MBSockets::UDPSource Source;
            size_t RequestSize = m_Socket.ReadPacket(Source,Buffer.data(),Buffer.size(),WaitTimer);
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
                        ParseContent(InputStream,Notification);
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
                        ParseContent(InputStream,Request);
                        m_ThreadPool.AddTask([&,RequestID=ID,Request=std::move(Request)]()
                                {
                                    try
                                    {
                                        auto Response = m_RequestHandler(Request);
                                        std::string ResponseContent;
                                        MBUtility::MBStringOutputStream OutStream(ResponseContent);
                                        ParseContent(OutStream,Response);
                                        p_SendResponse(Location.IP,Location.Port,RequestID,ResponseContent);
                                    }
                                    catch(...)
                                    {
                                        
                                    }
                                });

                    }
                    else if(Type == UDPMessageType::Response)
                    {
                        UDPResponse Response;
                        ParseContent(InputStream,Response);
                        auto It = m_ResponseCallbacks.find(ID);
                        if(It != m_ResponseCallbacks.end() && It->second.IP == Location.IP)
                        {
                            auto Callback = std::move(It->second.Callback);
                            m_ResponseCallbacks.erase(It);
                            m_ThreadPool.AddTask([&,Response=Response,Callback= std::move(Callback)] () mutable
                                    {
                                        Callback(Response);
                                    });
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
                if(MessageToSend.AttemptedRetries < MessageToSend.MaxRetries)
                {
                    p_SendMessage(MessageToSend);
                    MessageToSend.AttemptedRetries += 1;
                    MessageToSend.RetryTime = CurrentTime + std::chrono::milliseconds(500);
                    std::make_heap(m_SentMessages.begin(),m_SentMessages.end());
                }
                else
                {
                    std::pop_heap(m_SentMessages.begin(),m_SentMessages.end(),std::greater<StoredMessage>());
                    m_SentMessages.resize(m_SentMessages.size()-1);
                }
            }
        }
    }
}
