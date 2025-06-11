#pragma once
#include <coroutine>
#include <stdexcept>
#include <optional>
#include <MBUtility/Future.h>
namespace MBChat2
{
    template<typename T>
    class FutureAwaiter
    {
        MBUtility::Future<T> m_Future;
        std::shared_ptr<std::atomic<bool>> m_Suspended = std::make_shared<std::atomic<bool>>(false);
        bool m_ValueAvailable = false;
        //ULTRA Hacky
        //TODO: is accessing a value touched by multiple threads without explicit synchronization 
        // undefined behaviour even if it isn't modified after...
        T* m_ValuePtr = nullptr;
    public:
        FutureAwaiter(MBUtility::Future<T> Future)
            : m_Future(std::move(Future))
        {
               
        }
        bool await_ready()
        {
            return m_Future.ValueAvailable();
        }
        bool await_suspend(std::coroutine_handle<> Handle)
        {
            auto Suspended = m_Suspended;
            m_Future.Then([this,Handle=Handle](T const& Value)
                    {
                        if(m_Suspended->load())
                        {
                            m_ValuePtr = const_cast<T*>(&Value);
                            Handle.resume();
                        }
                    });
            Suspended->store(true);
            return true;
        }
        T await_resume()
        {
            if(m_ValuePtr == nullptr)
            {
                throw std::runtime_error("Error in FutureAwaiter: await_resume with empty value");
            }
            return std::move(*m_ValuePtr);
        }
    };

    template<typename T>
    FutureAwaiter<T> operator co_await(MBUtility::Future<T>&& Future)
    {
        return FutureAwaiter(std::move(Future));
    }

    
    template<typename T>
    class TaskPromise;

    template<typename T>
    class Task : public std::coroutine_handle<TaskPromise<T>>
    {
    public:
        typedef TaskPromise<T> promise_type;
        bool await_ready()
        {
            return false;
        }
        bool await_suspend(std::coroutine_handle<> Handle);
        T await_resume();
    };

    template<typename T>
    class TaskPromise
    {
        friend Task<T>;

        std::optional<T> m_Result;
        std::coroutine_handle<> m_WaitingCoroutine;
        bool m_WaitingResumed = true;
    public:

        typedef Task<T> Handle;
        Handle get_return_object(){ return {Handle::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept {return {};};
        std::suspend_never final_suspend() noexcept {return {};};
        void return_value(T Result)
        {
            m_Result = std::move(Result);
            if(m_WaitingCoroutine)
            {
                m_WaitingResumed = true;
                m_WaitingCoroutine.resume();
            }
        }
        void unhandled_exception()
        {
               
        }
        ~TaskPromise()
        {
            if(!m_WaitingResumed && m_WaitingCoroutine)
            {
                m_WaitingCoroutine.destroy();
            }
        }
    };

    template<typename T>
    bool Task<T>::await_suspend(std::coroutine_handle<> Handle)
    {
       this->promise().m_WaitingCoroutine = Handle;
       this->promise().m_WaitingResumed = false;
       this->resume();
       return true;
    }
    template<typename T>
    T Task<T>::await_resume()
    {
        if(this->promise().m_Result.has_value())
        {
            return std::move(this->promise().m_Result.value());
        }
        throw std::runtime_error("Error in Task await_resume: Result was uninitialized");
    }
}
