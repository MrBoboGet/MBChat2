#include "Client.h"
#include "Task.h"



MBChat2::Task<int> Multiply(int lhs,int rhs)
{
    co_return lhs * rhs;
}

MBChat2::Task<int> GetInt(int Value)
{
    co_return co_await Multiply(Value,123);
}

namespace Test
{
    class TestClass
    {
           
    };
    template<typename T>
    MBChat2::Task<int> operator co_await(T const&) requires std::is_base_of_v<TestClass,T>
    {
        return Multiply(1,1);
    }
}
class TestClass2 : public Test::TestClass
{
       
};

MBChat2::Task<int> TestFunc(int Value)
{
    auto test = co_await Test::TestClass();
    auto test2 = co_await TestClass2();
    co_return co_await Multiply(Value,123);
}


MBChat2::Task<std::string> GetPrintString()
{
    std::string Result = "Hej Hej ";

    for(int i = 0; i < 3;i++)
    {
        Result += std::to_string(co_await GetInt(i));
    }
    std::cout<<Result<<std::endl;
    co_return Result;
}



int main(int argc, const char** argv)
{

    auto Client = MBChat2::Client::MakeClient();
    return Client->Run();
}
