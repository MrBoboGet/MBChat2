#pragma once
#include <MBCLI/MBCLI.h>
#include <MBTUI/MBTUI.h>
#include <MBTUI/Layerer.h>

#include "DBVisualiser.h"
#include "Connection.h"

#include <MBLisp/Evaluator.h>


namespace MBChat2
{

    typedef MBUtility::MOFunction<std::unique_ptr<DBVisualiser>()> VisualiserFactory;
    typedef MBUtility::MOFunction<void(std::vector<MBLisp::Value> const&)> CommandFunc;

    class Client : public std::enable_shared_from_this<Client>
    {
        std::mutex m_InternalsMutex;
        MBCLI::WindowManager m_TopWindow;
        MBTUI::Layerer m_TopLayerer;

        MBCLI::MBTerminal m_Terminal;
        bool m_LayerHandleInput = false;

        std::shared_ptr<MBLisp::Evaluator> m_Evaluator;

        MBTUI::REPL m_CommandRepl;
        std::vector<std::string> p_CompletionFunc(MBTUI::REPL_Line  const& LineInfo);

        //struct CommandWindow
        //{
        //    MBTUI::REPL Repl;
        //};

        //MBCL
        std::shared_ptr<MBDB::MrBoboDatabase> m_LocalDB;
        std::filesystem::path m_DBPath;

        std::vector<MBUtility::MOFunction<void()>> m_RecievedEvents;
        struct DBVisualiserInfo
        {
            std::shared_ptr<DBConnection> Connection;
            std::vector<std::unique_ptr<DBVisualiser>> Visualiser;
        };

        
        std::unordered_map<ID,DBVisualiserInfo> m_ActiveVisualiser;
        std::unordered_map<std::string,VisualiserFactory> m_RegisteredVisualisers;
        std::unordered_map<std::string,CommandFunc> m_RegisteredCommands;

        ID m_LocalID;

        ID m_VisualisedDB;
        std::shared_ptr<ConnectionManager> m_ConnectionManager;


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
            
            virtual bool HandleInput(MBCLI::ConsoleInput const& Input);
            virtual void SetFocus(bool IsFocused);
            virtual MBCLI::CursorInfo GetCursorInfo();
            virtual void WriteBuffer(MBCLI::BufferView View,bool Redraw);
        };

        std::shared_ptr<DBWindow> m_DBVisualiserWindow;

        static DatabaseDefinition p_CreateChatDB(ID const& LocalID,ID const& PeerID);

        
        void p_StartChat(ID const& PeerID);
        void p_DisplayError(std::string const& ErrorMessage);

        void p_AddVisualiser(DatabaseDefinition const& Database);
        DatabaseDefinition p_LoadDatabase(ID const& DBID);


        void p_AddEvent(MBUtility::MOFunction<void()> Event);
        void p_ResourceRecievedHandler(NewMessage const& Header);
        void p_RPCHandler(PeerInfo const& Peers, MBParsing::JSONObject const& Object,MBUtility::Promise<MBParsing::JSONObject> Response);
        std::vector<std::string> p_TokenizeCommand(std::string const& NewCommand);
        void p_HandleCommandString(std::string const& NewCommand);
        void p_HandleEvent(MBCLI::TTYEvent const& NewInput);
        void p_SetNewVisualiserWindow(std::shared_ptr<DBVisualiser> Visualiser);


        Client(){};
    public:
        void AddVisualiser(std::string const& DatabaseType,
                VisualiserFactory Factory);
        void AddCommand(std::string const& CommandName, CommandFunc Result);
        void DisplayOverlay(MBUtility::SmartPtr<MBCLI::Window> TopWindow);
        void OpenDatabase(ID const& DatabaseID);
        DatabaseDefinition CreateDatabase(DatabaseDefinition Definition);
        std::vector<DatabaseDefinition> GetDatabases();


        static std::shared_ptr<Client> MakeClient();
        Client(Client&&) = delete;
        Client& operator=(Client&&) = delete;
        Client& operator=(Client const&) = delete;
        Client(Client const&) = delete;

        int Run();
    };
}
