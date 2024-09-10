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
        std::mutex m_InternalsMutex;
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

        
        struct DBVisualiserInfo
        {
            std::shared_ptr<DBConnection> Connection;
            std::vector<std::unique_ptr<DBVisualiser>> Visualiser;
        };

        
        std::unordered_map<ID,DBVisualiserInfo> m_ActiveVisualiser;

        ID m_LocalID;

        ID m_VisualisedDB;
        std::shared_ptr<ConnectionManager> m_ConnectionManager;
        std::shared_ptr<MBDB::MrBoboDatabase> m_Database;


        class DBWindow : public MBCLI::Window
        {
            Client* m_AssociatedClient = nullptr;

            MBCLI::Window* p_GetActiveWindow();
            MBCLI::Dimensions m_Dims;
        public:   
            DBWindow(Client* AssociatedClient)
            {
                m_AssociatedClient = AssociatedClient;   
            }
            MBCLI::Dimensions GetDimensions();
            
            virtual bool Updated();
            virtual void HandleInput(MBCLI::ConsoleInput const& Input);
            virtual void SetDimensions(MBCLI::Dimensions NewDimensions);
            virtual void SetFocus(bool IsFocused);
            virtual MBCLI::CursorInfo GetCursorInfo();
            virtual MBCLI::TerminalWindowBuffer GetBuffer();
        };

        std::shared_ptr<DBWindow> m_DBVisualiserWindow;

        static DatabaseDefinition p_CreateChatDB(ID const& LocalID,ID const& PeerID);

        
        void p_StartChat(ID const& PeerID);
        void p_DisplayError(std::string const& ErrorMessage);

        void p_AddVisualiser(ID const& DatabaseID);
        void p_ResourceRecievedHandler(NewMessage const& Header);
        void p_RPCHandler(PeerInfo const& Peers, MBParsing::JSONObject const& Object,MBUtility::Promise<MBParsing::JSONObject> Response);

        std::vector<std::string> p_TokenizeCommand(std::string const& NewCommand);
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
