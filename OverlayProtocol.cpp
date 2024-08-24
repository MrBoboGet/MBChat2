#include "OverlayProtocol.h"

namespace MBChat2
{
       
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
