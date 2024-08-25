#pragma once
#include <MBCLI/MBCLI.h>
#include <MBTUI/MBTUI.h>
#include <MBTUI/Layerer.h>

#include "DBVisualiser.h"
#include "Connection.h"


namespace MBChat2
{



    class Client
    {
        MBCLI::WindowManager m_TopWindow;
        MBTUI::Layerer m_TopLayerer;

        MBCLI::MBTerminal m_Terminal;
        bool m_LayerHandleInput = false;


        MBTUI::REPL m_CommandRepl;

        //struct CommandWindow
        //{
        //    MBTUI::REPL Repl;
        //};

        //MBCL
        std::shared_ptr<MBDB::MrBoboDatabase> m_LocalDB;


        std::unordered_map<std::string,std::vector<DBVisualiser>> m_ActiveVisualiser;

        std::unique_ptr<ConnectionManager> m_ConnectionManager;

        

        void p_HandleCommandString(std::string const& NewCommand);
        void p_HandleEvent(MBCLI::TTYEvent const& NewInput);
        void p_SetNewVisualiserWindow(std::shared_ptr<DBVisualiser> Visualiser);
    public:
        Client(){};
        Client(Client&&) = delete;
        Client& operator=(Client&&) = delete;
        Client& operator=(Client const&) = delete;
        Client(Client const&) = delete;

        int Run();
    };
}
