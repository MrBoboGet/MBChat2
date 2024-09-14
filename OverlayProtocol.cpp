#include "OverlayProtocol.h"

namespace MBChat2
{
    ID Distance(ID const& Lhs,ID const& Rhs)
    {
        assert(Lhs.size() == Rhs.size());
        ID ReturnValue = ID(Lhs.size(),0);
        for(size_t i = 0; i < Lhs.size();i++)
        {
            ReturnValue[i] = Lhs[i] ^ Rhs[i];
        }
        return  ReturnValue;
    }
    std::string IDToString(ID const& IDToConvert)
    {
        std::string ReturnValue(IDToConvert.size(),0);
        std::memcpy(ReturnValue.data(),IDToConvert.data(),IDToConvert.size());

        return  ReturnValue;
    }
    ID StringToID(std::string const& StringToConvert)
    {
        ID ReturnValue(StringToConvert.size(),0);
        std::memcpy(ReturnValue.data(),StringToConvert.data(),StringToConvert.size());
        return  ReturnValue;
    }
       
    void Message::ReadMessageHeader(MBUtility::IndeterminateInputStream& InputStream)
    {
        ParseHeader(InputStream,*this);
    }
    void Message::ReadBody(MBUtility::IndeterminateInputStream& InputStream)
    {
        ParseBody(InputStream,*this);
    }
    void Message::ReadMessage(MBUtility::IndeterminateInputStream& InputStream)
    {
        Parse(InputStream,*this);
    }
    void Message::WriteMessage(MBUtility::MBOctetOutputStream& OutStream)
    {
        Parse(OutStream,*this);
    }
}
