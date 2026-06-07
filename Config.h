#pragma once
#include <string>
#include <variant>

#include <stdexcept>
#include <vector>
#include <MBUtility/Optional.h>
#include <unordered_map>
#include <MBParsing/MBParsing.h>
#include <MBParsing/MBParsing.h>
namespace MBChat2
{
extern const int i_TypeIndexEndConfig_h[];
template<typename T> inline int GetTypeIndex();
struct Config
{
    private:
    public:
    MBUtility::Optional<int> Port = 1338;
    static Config Parse(MBParsing::JSONObject const& ObjectToParse);
    void FillObject(MBParsing::JSONObject const& ObjectToParse);
    MBParsing::JSONObject GetJSON() const;
    
};
template<> inline int GetTypeIndex<Config>()
{
return 0;
}

}