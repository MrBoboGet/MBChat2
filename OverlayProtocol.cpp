#include "OverlayProtocol.h"

namespace MBChat2
{
       
    void Message::ReadMessage(MBUtility::IndeterminateInputStream& InputStream)
    {
        Parse(InputStream,*this);
    }
    void Message::WriteMessage(MBUtility::MBOctetOutputStream& OutStream)
    {
        Parse(OutStream,*this);
    }
}
