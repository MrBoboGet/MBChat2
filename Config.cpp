#include "Config.h"
namespace MBChat2
{
Config Config::Parse(MBParsing::JSONObject const& ObjectToParse)
{
    Config ReturnValue;
    ReturnValue.FillObject(ObjectToParse);return ReturnValue;
}
void Config::FillObject(MBParsing::JSONObject const& ObjectToParse)
{
    if(ObjectToParse.GetType() != MBParsing::JSONObjectType::Aggregate)
    {
        throw std::runtime_error("Error parsing Config: Object is not of aggregate type");
    }
    if(ObjectToParse.HasAttribute("Port") && ObjectToParse["Port"].GetType() != MBParsing::JSONObjectType::Null)
    {
        if(ObjectToParse["Port"].GetType() != MBParsing::JSONObjectType::Integer)
        {
            throw std::runtime_error("Error parsing object of type Config: member Port isn't of type integer");
        }
        Port = ObjectToParse["Port"].GetIntegerData();
        
    }
    
}
MBParsing::JSONObject Config::GetJSON() const
{
    MBParsing::JSONObject ReturnValue(MBParsing::JSONObjectType::Aggregate);
    if(Port)
    {
        ReturnValue["Port"] = MBParsing::JSONObject(Port.Value());
        
    }
    return ReturnValue;
}
const int i_TypeIndexEndConfig_h[] = {1,};

}