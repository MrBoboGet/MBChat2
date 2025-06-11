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
