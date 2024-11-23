#include "Client.h"



int main(int argc, const char** argv)
{
    auto Client = MBChat2::Client::MakeClient();
    return Client->Run();
}
