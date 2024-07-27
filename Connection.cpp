#include "Connection.h"

namespace MBChat2
{
    bool Connection::p_VerifyAuthenticity(State const& SharedState,Message const& NewMessage)
    {
        return true;
    }
    void Connection::p_ReadThread(std::shared_ptr<State> State)
    {
        try
        {
            while(!State->Stopping.load())
            {
                Message NewMessage;
                NewMessage.ReadMessage(State->Transport);
                if(!p_VerifyAuthenticity(*State,NewMessage))
                {
                    //message silently dropped, should reduce trust for peer or something
                    continue;
                }
                MessageCallback Handler;
                {
                    std::lock_guard Lock(State->SendMutex);
                    auto It = State->MessageCallbacks.find(NewMessage.ResponseID);
                    if(It != State->MessageCallbacks.end())
                    {
                        Handler = std::move(It->second);
                        State->MessageCallbacks.erase(It);
                    }
                    else
                    {
                        Handler = State->GenericMessageHandler;
                    }
                }
                Handler(NewMessage);
            }
        }
        catch(...)
        {
            
        }
        State->Stopping.store(true);
        State->Transport.Abort();
        if(State->CloseHandler)
        {
            State->CloseHandler(State->Peer);
        }
    }
    void Connection::p_SendThread(std::shared_ptr<State> State)
    {
        try
        {
            while(!State->Stopping.load())
            {
                Message MessageToSend;
                {
                    std::unique_lock Lock(State->SendMutex);
                    while(!State->Stopping.load() && State->QueuedMessages.size() == 0)
                    {
                        State->SendConditional.wait(Lock);
                    }
                    if(State->Stopping.load())
                    {
                        break;
                    }
                    auto NewMessage = std::move(State->QueuedMessages.front());
                    NewMessage.first.MessageID = State->NextSendID;
                    State->NextSendID.fetch_add(1);
                    if(NewMessage.second)
                    {
                        State->MessageCallbacks[NewMessage.first.MessageID] = std::move(NewMessage.second);
                    }
                    MessageToSend = std::move(NewMessage.first);
                }
                MessageToSend.WriteMessage(State->Transport);
            }
        }
        catch(...)
        {
        }
        State->Stopping.store(true);
        State->Transport.Abort();
    }
    Connection::~Connection()
    {
        m_SharedState->Stopping.store(true);
        m_SharedState->Transport.Abort();

        {
            std::lock_guard Lock(m_SharedState->SendMutex);
            m_SharedState->SendConditional.notify_one();
        }
        m_SharedState->SendThread.detach();
        m_SharedState->ReadThread.detach();
    }
    void Connection::SendMessage(Message MessageToSend,MessageCallback Callback)
    {
        std::lock_guard Lock(m_SharedState->SendMutex);
        m_SharedState->QueuedMessages.push_back(std::make_pair(std::move(MessageToSend),std::move(Callback)));
        m_SharedState->SendConditional.notify_one();
    }
    Connection::Connection(ConnectionParameters Peer,MessageCallback MessageHandler,std::function<void(ConnectionParameters const&)> QuitHandler)
    {
           
    }
}
