(import TML.TML)
(import TML.Utils)
(import TML.file-picker)
(import mbchat chat)
(import text text)



(defclass tempClass ()
    (type "temp temp ")
)

@tml
<databaseEntry 
    db = null
    >
    <Text highlight-bg-color="green" content=(+ :type db "")   />
</databaseEntry>


(defmethod handle-input ((this databaseEntry) input)
    (if (eq input "enter")
        (chat:display-db :db this)
    )
)


(defmethod open-command (&rest args)
    (chat:display-overlay 
        @tml-emit
        <absolute orientation="center">
            <stacker passthrough="enter" height=20 width=20 border=true>
            @{
                (doit child (chat:get-databases)
                    (emit-child 
                        @tml-emit
                        <databaseEntry db=child />
                    )
                )
                #(emit-child @tml-emit <Text content="asdasd"/>)
            }
            </stacker>
        </absolute>
     )
)
(defmethod open-completion (tokens)
    (list)
)



(chat:add-command "open" open-command)
(chat:add-completion "open" open-completion)
