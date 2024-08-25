#include "Client.h"

#include <MBTUI/BufferWindow.h>

#include "DBVisualiser.h"

#include <MBSystem/MBSystem.h>

namespace MBChat2
{
       
    void Client::p_HandleEvent(MBCLI::TTYEvent const& NewInput)
    {
        if(NewInput.IsType<MBCLI::ResizeEvent>())
        {
            m_Terminal.Clear();
            auto const& ResizeEvent = NewInput.GetType<MBCLI::ResizeEvent>();
            m_TopLayerer.SetDimensions( MBCLI::Dimensions(ResizeEvent.NewWidth,ResizeEvent.NewHeight));
        }
        else if(NewInput.IsType<MBCLI::ConsoleInput>())
        {
            auto const& Character = NewInput.GetType<MBCLI::ConsoleInput>();
            if(m_LayerHandleInput)
            {
                m_TopLayerer.HandleInput(Character);
            }
            else
            {
                if(Character.CharacterInput == ':')
                {
                    m_LayerHandleInput = true;
                    m_CommandRepl.SetMaxDims(MBCLI::Dimensions(-1,1));
                    m_CommandRepl.SetOnEnterFunc([&](std::string const& String){p_HandleCommandString(String);});
                    m_TopLayerer.AddLayer(MBUtility::SmartPtr<MBCLI::Window>(&m_CommandRepl));
                }
            }
        }
    }
    void Client::p_SetNewVisualiserWindow(std::shared_ptr<DBVisualiser> Visualiser)
    {
        m_TopWindow.AddWindow(MBUtility::SmartPtr<MBCLI::Window>(std::static_pointer_cast<MBCLI::Window>(Visualiser)));
    }
    void Client::p_HandleCommandString(std::string const& NewCommand)
    {
        m_TopLayerer.PopLayer();
        m_LayerHandleInput = false;
    }
    int Client::Run()
    {
        int ReturnValue = 0;
        std::filesystem::path DBPath = MBSystem::GetUserHomeDirectory()/".mbchat/localdb.db";
        if(!std::filesystem::exists(DBPath))
        {
            std::filesystem::create_directories(DBPath.parent_path());
            m_LocalDB = std::make_shared<MBDB::MrBoboDatabase>(MBUnicode::PathToUTF8(DBPath),MBDB::DBOpenOptions::ReadWrite);
            MBError Result = true;
            m_LocalDB->GetAllRows(
    #include "DBDefinition.inc"
                    ,
                    &Result);
            if(!Result)
            {
                std::cout<<"Failed creating database with following error: "+Result.ErrorMessage<<std::endl;
                std::exit(1);
            }
        }
        else
        {
            m_LocalDB = std::make_shared<MBDB::MrBoboDatabase>(MBUnicode::PathToUTF8(DBPath),MBDB::DBOpenOptions::ReadWrite);
        }


        m_Terminal.SetExitHandler([]{ std::exit(0);});
        m_Terminal.InitializeWindowMode();
        m_Terminal.SetBufferRenderMode(true);

        MBCLI::Dimensions Dims;
        Dims.Width = m_Terminal.GetTerminalInfo().Width;
        Dims.Height = m_Terminal.GetTerminalInfo().Height;

        MBCLI::TerminalWindowBuffer SideBar(12,Dims.Height);
        SideBar.WriteBorder(10,Dims.Height,0,0,MBCLI::ANSITerminalColor::Green);
        MBCLI::TerminalWindowBuffer Rest(Dims.Width,Dims.Height);

        auto SideWindow = MBTUI::BufferWindow(SideBar);
        auto RestWindow = MBTUI::BufferWindow(Rest);

        std::vector<MBCLI::WindowManager::WindowContainer> Windows;
        Windows.emplace_back(MBUtility::SmartPtr( (MBCLI::Window*)&SideWindow));
        //Windows.emplace_back(MBUtility::SmartPtr(MBUtility::SmartPtr( (MBCLI::Window*) &RestWindow)));
        
        m_TopWindow = MBCLI::WindowManager(std::move(Windows) , Dims,false,1);
        m_TopLayerer.SetDimensions(Dims);
        m_TopLayerer.AddLayer(MBUtility::SmartPtr((MBCLI::Window*)&m_TopWindow));
        bool ShouldRun = true;
        //m_Terminal.PrintWindowBuffer(m_TopWindow.GetBuffer(),0,0);
        m_Terminal.PrintWindowBuffer(m_TopLayerer.GetBuffer(),0,0);
        m_Terminal.HideCursor();
        while(ShouldRun)
        {
            auto NewInput = m_Terminal.GetNextEvent();
            p_HandleEvent(NewInput);

            auto NewBuffer = m_TopLayerer.GetBuffer();
            ///SideWindow.SetDimensions(MBCLI::Dimensions(30,25));
            //auto NewBuffer = SideWindow.GetBuffer();
            m_Terminal.PrintWindowBuffer(NewBuffer,0,0);
            m_Terminal.SetCursorInfo(m_TopLayerer.GetCursorInfo());
            m_Terminal.Refresh();
        }
        return ReturnValue;
    }
}
