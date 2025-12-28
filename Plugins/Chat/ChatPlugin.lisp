(import TML.TML)
(import db)
(import cli)
(import TML.modal)
(import TML.file-picker)
(import mbchat chat)
(import json js)


(defclass file-upload ()
    (name "")
    (id "")
)

@tml
<Chat Width="100%" >
    <stacker @content Width=Width>
    </stacker>
    <repl @input 
        onenter=(lambda (line) (add-message this (send-message this line)))   >
    </repl>
    @connection = null

    <modal @mod
        orientation="center" 
        height="50%" 
        width="50%" 
        >
        
    </modal>
</Chat>

(set init chat:init)
(set resource-published chat:resource-published)
(chat:add-visualiser chat:client "Chat" Chat)

(defmethod add-message ((this Chat) (resource chat:resource-handle_t))
    (add-child :content this (get-resource-element this resource))
)

(defmethod init ((window Chat) connection)
    (set :connection window connection)


    (setl message-dir (chat:get-resource connection "/messages"))
    (if (eq message-dir null) (chat:add-resource connection "/messages" ""))

    (setl messages (map _(add-message window _) (chat:get-resources connection "/messages/*")))
    #(doit child messages (add-child :content window child))
)

(defmethod get-resource-element ((window Chat) (resource chat:resource-handle_t))
    (if (eq (chat:get-type resource) "file")
        (setl file (js:read-json (chat:get-content :connection window resource)))
        @tml-emit
        <Text content=(+ "[file: " :"name" file "]") />
    else
        @tml-emit
        <Text content=(+ "[" (chat:get-user :connection window resource) "]: " (chat:get-content :connection window resource)) />
    )
)



(defmethod start-upload ((this Chat))
    (set-window :mod this this 
        @tml-emit
        <filePicker height="100%" width="100%" 
                on-pick=(progn _(upload-file this _))
            >
        
        </filePicker>
    )
)

(defmethod upload-file ((this Chat) file-path)
    (setl new-file (file-upload))
    (set :name new-file file-path)
    (add-message this (chat:add-child :connection this "/messages" (js:to-json-string new-file) "file"))
)

(defmethod handle-input ((this Chat) input)
    (if (ctrl input "o")
        (start-upload this)
    )
    (handle-base this input)
)

(defmethod resource-published ((window Chat) resource)
    (add-message window resource)
)

(defmethod send-message ((window Chat) string)
    (chat:add-child :connection window "/messages" string)
)
