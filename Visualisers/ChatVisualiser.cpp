#include "ChatVisualiser.h"
#include <MBTUI/BufferWindow.h>

namespace MBChat2
{
       
    ChatVisualiser::ChatVisualiser()
    {
        m_Stacker = std::make_shared<MBTUI::Stacker>();
        m_InputField = std::make_shared<MBTUI::REPL>();
        m_TopWindowManager.SetVertical(true);
        //std::vector<MBCLI::WindowManager::WindowContainer> Windows;
        //Windows.push_back(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(m_Stacker))); 
        //Windows.push_back(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(m_InputField))); 
        //m_TopWindowManager = MBCLI::WindowManager({},m_Dims);
        m_TopWindowManager.AddWindow(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(m_Stacker)),
                MBCLI::WindowStretchInfo());
        m_TopWindowManager.AddWindow(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(m_InputField)),
                MBCLI::WindowStretchInfo(10,1,3));
        m_TopWindowManager.MoveDown();
        m_InputField->SetOnEnterFunc([&](std::string const& Line)
                {
                    ResourceContent NewResource;
                    NewResource.Content = Line;
                    NewResource.UpType = UploadType::New;

                    std::shared_ptr<MBTUI::BufferWindow> BufferWindow = std::make_shared<MBTUI::BufferWindow>();
                    MBCLI::TerminalWindowBuffer Buffer(40,1);
                    Buffer.WriteCharacters(0,0,Line);
                    BufferWindow->SetBuffer(std::move(Buffer));
                    //BufferWindow->SetDimensions(MBCLI::Dimensions(40,1));
                    m_Stacker->AddElement(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(BufferWindow)));
                    GetConnection().UploadResource(std::move(NewResource));
                });
    }
    void ChatVisualiser::ResourcePublished(NewMessage const& NewHeader) 
    {
        std::shared_ptr<MBTUI::BufferWindow> BufferWindow = std::make_shared<MBTUI::BufferWindow>();
        MBCLI::TerminalWindowBuffer Buffer(40,1);
        if(NewHeader.Content.size() > 0)
        {
            Buffer.WriteCharacters(0,0,NewHeader.Content);
        }
        BufferWindow->SetBuffer(std::move(Buffer));
        m_Stacker->AddElement(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(BufferWindow)));
    }
    void ChatVisualiser::Init()
    {
        auto Stmt = GetConnection().GetDB()->
            GetSQLStatement("SELECT * FROM Resources WHERE "
                    "DatabaseHash=:DBHash ORDER BY RecievedTimestamp ASC");
        Stmt.BindBlob("DBHash",GetConnection().GetDBID());
        auto Rows = GetConnection().GetDB()->GetAllRows(Stmt);
        for(auto&  Row :  Rows)
        {
            if(std::holds_alternative<std::monostate>(Row["Content"]))
            {
                continue;
            }
            std::shared_ptr<MBTUI::BufferWindow> BufferWindow = std::make_shared<MBTUI::BufferWindow>();
            MBCLI::TerminalWindowBuffer Buffer(40,1);
            std::string Content = std::get<std::string>(Row["Content"]);
            Buffer.WriteCharacters(0,0,Content);
            BufferWindow->SetBuffer(std::move(Buffer));
            m_Stacker->AddElement(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(BufferWindow)));
        }
    }
    bool ChatVisualiser::HandleInput(MBCLI::ConsoleInput const& Input) 
    {
        return m_TopWindowManager.HandleInput(Input);
    }
    void ChatVisualiser::SetFocus(bool IsFocused) 
    {
        m_TopWindowManager.SetFocus(IsFocused);
    }
    MBCLI::CursorInfo ChatVisualiser::GetCursorInfo() 
    {
        return m_TopWindowManager.GetCursorInfo();
    }
    void ChatVisualiser::WriteBuffer(MBCLI::BufferView View,bool Redraw)
    {
        return m_TopWindowManager.WriteBuffer(View.SubView(0,0),Redraw);
    }
}
