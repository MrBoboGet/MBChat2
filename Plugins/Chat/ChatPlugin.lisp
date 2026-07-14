(import TML.TML)
(import db)
(import cli)
(import TML.modal)
(import TML.file-picker)
(import mbchat chat)
(import json js)
(import TML.highlighter)
(import text)
(import TML.loading-bar)


(defclass file-upload ()
    (name "")
    (id "")
)

@tml
<Chat Width="100%" >
    <stacker @content passthrough="d" Width=Width>

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


@tml
<File resource="" db=null>
    @name = ""
    @id = ""
    @observer=null
    @{
        (setl file (js:read-json (chat:get-content db resource)))
        (setl :name this :"name" file)
        (setl :id this :"id" file)
        (set :observer this (chat:get-state-observer :db this (hex-decode :id this)))
        (if (eq (chat:download-percent :observer this) 100)
            (set-base-atr this "text-color" "green")
        )
    }
    <Text content=(+ "[file: " :name this "] ") />
    <stacker justification="between" @status />
</File>

(defmethod get-status-string ((this File))
    (if (eq :observer this null) (return ""))
    (return (+ (str (chat:download-percent :observer this)) "%"))
)


(defmethod handle-input ((this File) input)
    (if (eq input "d")
        (download-file this)
        (return true)
    )
)

(defmethod download-file ((file File))
    (setl on-state-changed  (lambda () 
            (if (eq (chat:download-percent :observer file) 100)
                (set-base-atr file "text-color" "green")
                (set-children :status file (list))
             else
                (set-children :status file (list 
                    @tml-emit
                    <stacker width="100%" direction="right" justification="between">
                        <Text content=(get-status-string file)/>
                        <loadingBar percent=(chat:download-percent :observer file) />
                    </stacker>
                    ))
            )
            
            (update file)))
    (chat:on-state-changed :observer file on-state-changed)
    (on-state-changed)
    (chat:start-download :observer file)
    (update file)
)

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
    (setl file-dir (chat:get-resource connection "/files"))
    (if (eq file-dir null) (chat:add-resource connection "/files" ""))

    (setl messages (map _(add-message window _) (chat:get-resources connection "/messages/*")))
    #(doit child messages (add-child :content window child))
)

(defmethod get-resource-element ((window Chat) (resource chat:resource-handle_t))
    (if (eq (chat:get-type resource) "file")
        (setl file (js:read-json (chat:get-content :connection window resource)))
        @tml-emit
        <Highlighter>
            <File resource=resource db=:connection window /> 
        </Highlighter>
    else
        @tml-emit
        <Highlighter>
            <Text content=(+ "[" (chat:get-user :connection window resource) "]: " (chat:get-content :connection window resource)) />
        </Highlighter>
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
    (set :name new-file (file-name file-path))
    (setl file-id (hex-encode (chat:get-id (chat:upload-file :connection this "/files" file-path))))
    (set :id new-file file-id)
    (add-message this 
        (chat:add-child :connection this "/messages" (js:to-json-string new-file) "file"))
)

(defmethod handle-input ((this Chat) input)
    (if (ctrl input "o")
        (start-upload this)
        (return true)
     else if (ctrl input "i")
        #(if (not (eq (get-selected-index :content this) -1))
        #    (set-selected-index :content this 0)
        #)
        (set-selected-index :content this (+ (child-count :content this) -1))
        (push this :content this _(set-focus :content this false))
        
    )
    (handle-base this input)
)

(defmethod resource-published ((window Chat) resource)
    (if (eq (substr (chat:get-path-string resource) 0 (len "/messages")) "/messages")
        (add-child :content window (get-resource-element window resource))
    )
)

(defmethod send-message ((window Chat) string)
    (chat:add-child :connection window "/messages" string)
)
