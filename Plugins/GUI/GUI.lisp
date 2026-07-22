(import TML.TML)
(import TML.Utils)
(import TML.file-picker)
(import mbchat chat)
(import text text)
(import TML.toaster)



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

(set error-handler @tml-emit <toaster visible-time=3000 border-color="red"></toaster>)
(chat:mount-window error-handler)
(chat:add-on-error-callback (lambda (err) (toast error-handler err)))

(chat:add-command "open" open-command)
(chat:add-completion "open" open-completion)
