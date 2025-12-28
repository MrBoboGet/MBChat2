(import TML.TML)
(import db)
(import cli)
(import json js)
(import TML.Utils)
(import TML.file-picker)
(import mbchat chat)
(import MBPerl mbperl)
(import text text)




@tml 
<JiraCard>
    <stacker @container border=true>
        <div> @(:title    this)</div>
        <div> @(:type     this)</div>
        <div> @(:feature  this)</div>
        <div> @(:assignee this)</div>
        <Text bg-color="#ff0000" content="awoooga"/>
    </stacker>
    @title = "titel"
    @type = "type"
    @feature = "feature"
    @assignee = "assignee"
</JiraCard>


(defmethod set-title ((card JiraCard) (new-title string_t))
    (set :title card new-title)
    (update card)
)

(defmethod set-focus ((card JiraCard) focus)
    (if focus 
        (set-atr :container card "border-color" "green")
    else
        (set-atr :container card "border-color" "white")
    )
    (set-updated card true)
)

(defmethod updated ((card JiraCard))
    true
)

(set current-card-count 0)

@tml
<Jira>
    <stacker @content width="100%" height="100%" direction="right" overflow=true overflow-reversed=true border=true reversed=true>
    </stacker>

    <absolute @window visible=false orientation="center"> 
        <stacker border=true>
            <div>test-abs</div>
            <div>test-abs1</div>
            <div>test-abs2</div>
        </stacker>
    </absolute>
    @connection = null
</Jira>

#<repl @input 
#    onenter=(lambda (line) (setl new-card (Card)) (incr current-card-count 1) (set-title new-card (+ "number " (str current-card-count)))  (add-child :content this new-card))  >
#</repl>
(defmethod handle-input ((jira Jira) input)
    (if (|| (eq input "j") (eq input "h") (eq input "k") (eq input "l"))
        (handle-input :content jira input)
    else if (eq input "enter")
        (setl window (get-selected :content jira))
        (set-atr :window jira "visible" true)
        (if (not (eq window null))
            (add-child :container window (Text "awooga"))
        )
    else if (eq input "a")
        (set-atr :window jira "visible" false)
    else
        (setl new-card (JiraCard)) 
        (incr current-card-count 1) 
        (set-title new-card (+ "number " (str current-card-count)))  
        (add-child :content jira new-card)
        (set-updated jira true)
    )
)

(defclass card-field ()
    (name "")
    (type "")
    (defined-fields (dict))
    (thumbnail false)
)

(defclass text-field (card-field)
    (non-empty false)
    (multiline false)
    (bg-color "")
    (color "")
    (content "")
)
(defclass path-field (card-field)
    (root "")
)
(defclass list-field (card-field)
    (sub-type (dict))
)
(defclass alternative-field (card-field)
    (alternatives (list))
    (sub-type "")
    (alternative 0)
)
(defclass alternative ()
    (index 0)
    (value null)
    (constructor (new-index new-field)
        (set :index this new-index)
        (set :value this new-field)
    )
)

(defclass card-template ()
    (id "")
    (field-indexes (dict))
    (fields (list))
)
(defclass Card ()
    (template-id "")
    (fields (list))
    (path null)
)

(defmethod from-json ((json card-template))
    (card-template)
)
(defmethod to-json ((json card-template))
    (dict)
)

(set type-map (make-dict 
    ("text" text-field) 
    ("path" path-field)
    ("list" list-field) 
    ("alternative" alternative-field))
)

(defmethod field-parsed ((res any_t))
    null
)

(defmethod field-parsed ((res alternative-field))
    (set :alternatives res (map 
        _(progn (set :"type" ;_ :alternatives res :sub-type res)  
            (alternative _ (parse-field ; _ :alternatives res))) (range 0 (len :alternatives res)))
                )
)

(defmethod parse-field ((field dict_t) (res any_t))
    (setl field-slots (slots res))
    (doit slot field-slots 
        (if (in (str slot) field)
            (set ;(str slot) :defined-fields res true)
            (setl value ;(str slot) field)
            (setl slot-type (type ;slot res))
            (if (eq slot-type dict)
                (if (not (eq slot-type dict_t))
                    (error (+ 
                        "Invalid field value " 
                        (str value) 
                        " in field " 
                        (str slot) 
                        " of type " 
                        (str slot-type)
                        ))
                )
                (setl dict-result (dict))
                (doit key (keys value)
                    (set ;key dict-result (parse-field ;key value))
                )
             else if (eq slot-type type_t)
                (set ;slot res (parse-field value))
             else if(is slot-type object_t)
                (set ;slot res (parse-field value ;slot res))
             else
                (if (not (eq (type value) slot-type))
                    (error (+ 
                        "Invalid field value " 
                        (str value) 
                        " in field " 
                        (str slot) 
                        " of type " 
                        (str slot-type)
                        ))
                )
                (set ;slot res value)
            )
        )
    )
    (field-parsed res)
    res
)
(defmethod parse-card-field ((template-field card-field) (serialised-field dict_t))
   (setl :"type" serialised-field :type template-field)
   (setl :"name" serialised-field :name template-field)
   (setl current-value (parse-field serialised-field))
   (doit key (keys :defined-fields template-field) 
       (setl ;(symbol key) current-value ;(symbol key) template-field)
   )
   (return current-value)
)
#(defmethod parse-card-field ((template-field alternative-field) (serialised-field dict_t))
#    (setl alternative-index 0)
#    (if (&& (in "alternative" serialised-field) (eq (type :"alternative" serialised-field) int_t))
#        (setl alternative-index :"alternative" serialised-field)
#    )
#    (if (<= alternative-index (len :alternatives template-field)) 
#        (return ;alternative-index :alternatives template-field)
#    )
#    (text-field)
#)
#(defmethod parse-card-field ((template-field list-field) (serialised-field dict_t))
#    (text-field)
#)


(defmethod parse-field ((field dict_t))
    (setl res null)
    (setl field-type :"type" field)
    (if (not (in field-type type-map))
        (error (+ "invalid field type " field-type))
    )
    (setl value (;field-type type-map))
    (setl :type value :"type" field)
    (parse-field field value)
)
(defmethod parse-template ((template dict_t))
    (setl res (card-template))
    (setl fields :"fields" template)
    (setl i 0)
    (doit field fields
        (setl name :"name" field)
        (set ;name :field-indexes res i)
        (setl new-field (parse-field field))
        (setl :name new-field name)
        (append :fields res new-field)
        (incr i 1)
    )
    res
)
#(defmethod parse-card ((template card-template) (card dict_t))

(defmethod edit ((field card-field))
    @tml-emit
    <Text content="placeholder"/>
)
(defmethod display ((field card-field))
    @tml-emit
    <Text content="placeholder"/>
)
(defmethod edit ((field text-field))
    (setl hmmm 123)
    @tml-emit
    <Text content="placeholder"/>
)
(defmethod display ((field text-field))
    (setl hmm 123)
    @tml-emit
    <Text content=(index field 'content) multiline=:multiline field bg-color=:bg-color field color=:color field />
)
(defmethod display ((field alternative))
    (display :value field)
)
(defmethod display ((field alternative-field))
    (if (< :alternative field (len :alternatives field))
        #(display : :alternative field :alternatives field)
        @tml-emit
        <dropdown 
                value = : :alternative field :alternatives field
                alternatives = :alternatives field
                display-func = (progn display)
                filter-value = (progn _(progn :content :value _))
            />
    else
        @tml-emit
        <Text content="Invalid alternative" color="red"/>
    )
)

@tml
<DisplayedCard>
    <stacker @input border=true  width=40 border-color="white">
        @children
    </stacker>
    @card = null
    @index = 0
</DisplayedCard>

(defmethod edit ((template Card))
    @tml-emit
    <DisplayedCard @card=template >
        @{
            (doit child :fields template 
                (setl child-value  
                    @tml-emit
                    <Highlighter />
                )
                (set-child :input child-value (display child))
                (emit-child child-value)
            )
            #(emit-child (Text "letsa fackinga gåå"))
        }
    </DisplayedCard>
)

(defmethod set-value (field displayed-field)
    null
)
(defmethod set-value ((field path-field) displayed-field)
    null
)
(defmethod set-value ((field list-field) displayed-field)
    null
)
(defmethod set-value ((field alternative-field) displayed-field)
    (setl ;"alternative" :defined-fields field true)
    (setl :alternative field :index (get-value displayed-field))
)
(defmethod set-value ((field text-field) displayed-field)
    (setl ;"content" :defined-fields field true)
    (setl :content field (get-content displayed-field))
)


(defmethod get-card ((this DisplayedCard))
    (setl ret (copy-deep :card this))
    (setl elements (map _(progn :child :input _) (children this)))
    (setl i 0)
    (doit field :fields ret
        (set-value field ;i elements)
        (incr i 1)
    )
    ret
)
(defmethod get-card-json ((template card-template)  (card Card))
    (setl obj (dict))
    (set :"template" obj (text:hex-encode :id template))
    (setl fields (dict))
    (doit field :fields card
        (setl new-field (dict))
        (setl field-index ;:name field :field-indexes template)
        (setl template-field ;field-index :fields template)
        (doit key (map symbol (keys :defined-fields field))
            (if (not (eq ;key template-field ;key field))
                (set ;(str key) new-field ;key field)
            )
        )
        (set ;:name field fields new-field)
    )
    (set :fields obj fields)
    (js:to-json-string obj)
)


@tml
<Highlighter>
    <stacker @content border-color="white">
        <placeholder @input/>
    </stacker>
</Highlighter>

(defmethod set-focus ((this Highlighter) focus)
    (if focus
        (set-atr :content this "border" true)
     else 
        (set-atr :content this "border" false)
    )
    (set-focus :input this focus)

)

(defmethod set-focus ((card DisplayedCard) focus)
    (if focus 
        (set-atr :input card "border-color" "green")
    else
        (set-atr :input card "border-color" "white")
    )
    (set-updated card true)
)

(defmethod display ((template Card) card-index)
    @tml-emit
    <DisplayedCard @card=template @index=card-index >
        @{
            (doit child :fields template 

                (if :thumbnail child (emit-child 
                    (display child)
                ))
            )
            #(emit-child (Text "letsa fackinga gåå"))
        }
    </DisplayedCard>
)

@tml
<Board>

    <stacker @content width="100%" 
             height="100%"  
             direction="right" 
             overflow=true 
             overflow-reversed=true 
             border=true reversed=true>


    </stacker>


    <absolute @window visible=false orientation="center"> 
    </absolute>

    <absolute @confirm visible=false orientation="center"> 
        <stacker border=true>
            <Text content="Delete resource?" />
            <stacker @confirmDiv justification="evenly" width="16" direction="right" passthrough="enter"  reversed=true >
                <button on-enter=(lambda () (delete-confirm this :path this))>
                    <Text content="yes" />
                </button>
                <button on-enter=(lambda () (pop this))>
                    <Text content="no" />
                </button>
            </stacker>
        </stacker >
        @path = ""
    </absolute>

    <absolute @picker-container visible=false orientation="center"> 
        <filePicker @picker on-pick=(lambda (file) 
                (setl json (js:read-json (open file)))
                (setl template (parse-template json))
                (set :template this template)
                (set-atr :picker-container this "visible" false)
                (setl resource (chat:add-child :connection this "/templates" (read-bytes (open file) 123123123)))
                (setl :id template (chat:get-resource-id resource))
                (pop this)
            )/>
    </absolute>

    <absolute @search-container visible=false > 
        <repl @search-repl onenter=(lambda (x) (display-cards this (filter-cards-expr this x)) (pop this)) />
    </absolute>

    @connection = null
    @cards = (list)
    @filterActive = false
    @template = null
    @templateMap = (dict)
</Board>


(defmethod get-template ((this Board) (card dict_t))
    (setl res null)
    (if (not (in "template" card))
        (return :template this)
    )
    (if (in :"template" card  :templateMap this)
        (return ; :"template" card :templateMap this)
    )
    (setl template-resource (chat:get-resource-by-id :connection this (text:hex-decode :"template" card)))
    (if (eq template-resource null)
        (error "Invalid card: tempalte does not exist")
    )
    (setl res (parse-template (js:read-json (in-stream (chat:get-content :connection this template-resource)))))
    (setl :id res (chat:get-resource-id template-resource))
    (set ; :id res :templateMap this res)
    res
)
(defmethod get-template ((this Board) (card Card))
    (setl res null)
    (if (in :template-id card  :templateMap this)
        (return ; :template-id card :templateMap this)
    )
    (setl template-resource (chat:get-resource-by-id :connection this :template-id card))
    (if (eq template-resource null)
        (error "Invalid card: tempalte does not exist")
    )
    (setl res (parse-template (js:read-json (in-stream (chat:get-content :connection this template-resource)))))
    (setl :id res (chat:get-resource-id template-resource))
    (set ; :id res :templateMap this res)
    res
)
(defmethod parse-card ((this Board) (card dict_t))
    (setl res (Card))
    (setl fields :"fields" card)
    (setl template (get-template this card))
    (doit field :fields template
        (if (in :name field fields)
            (append :fields res (parse-card-field field ;:name field fields))
        )
    )
    (set :template-id res :id template)
    res
)

(defmethod delete-confirm ((this Board) path)
    (chat:remove-resource :connection this path)
    (pop this)
)


(defmethod get-value (template (field card-field))
    ""
)
(defmethod get-value (template (field text-field))
    :content field
)
(defmethod get-value (template (field alternative-field))
    (get-value template ; :alternative field :alternatives field)
)
(defmethod get-value (template (field alternative))
    (get-value template :value field)
)

(defmethod get-card-dict (template (card Card))
    (setl res (dict))
    (doit field :fields card
        (setl ;(symbol :name field) res (get-value template field))
    )
    res
)

(defmethod filter-cards-expr ((this Board) string)
    (setl file-path load-filepath)
    (setl sym (gensym))
    (setl expr (eval `(lambda (,sym) (add-parent (environment) ,sym) ,(let ((load-filepath file-path)) (mbperl:parse-expr (in-stream string)))  )))
    #(setl expr (lambda (card) (doit key (keys card) (if (in string ;key card) (return true))) false))
    (setl ret (filter _(expr (get-card-dict (get-template this _) _)) :cards this))
    ret
)
(defmethod filter-cards ((this Board) string)
    (setl file-path load-filepath)
    (setl sym (gensym))
    #(setl expr (eval `(lambda (,sym) (add-parent (environment) ,sym) ,(let ((load-filepath file-path)) (read-term (in-stream string)))  )))
    (setl expr (lambda (card) (doit key (keys card) (if (in string ;key card) (return true))) false))
    (setl ret (filter _(expr (get-card-dict (get-template this _) _)) :cards this))
    ret
)

(defmethod empty-card ((this Board))
    (setl card-fields (dict))
    (doit field :fields :template this
       (set ; :name field card-fields (dict))
    )
    (setl card-definition (make-dict ("fields" card-fields)))
    (parse-card this card-definition)
)

(defmethod display-cards ((this Board) (cards list_t))
    (clear-children :content this)
    (set-children :content this (map _(display (index cards _) _) (range 0 (len cards))  ))
)
(defmethod set-cards ((this Board) (cards list_t))
    (set :cards this cards)
    (display-cards this cards)
)
(defmethod handle-input ((board Board) input)
    (handle-base board input)
    (if (|| (eq input "j") (eq input "h") (eq input "k") (eq input "l"))
        (handle-input :content board input)
    else if (eq input "c")
        (setl new-top (edit (empty-card board)))
        (set-child :window board new-top)
        (set-atr :window board "visible" true)
        (push board new-top 
            _(progn
                (set-atr :window board "visible" false)
                (setl new-card (get-card new-top))
                (setl :path new-card (chat:get-path (chat:add-child :connection board "/cards" 
                    (get-card-json :template board new-card))))
                (append :cards board new-card)
                (set-cards board :cards board)
            )
       )
    else if (eq input "/")
        (set-atr :search-container board "visible" true)
        (set :filterActive board true)
        (push board :search-repl board
            _(progn
                (set-atr :search-container board "visible" false)
            )
        )
    else if (eq input "u")
        (if :filterActive board 
            (set :filterActive board false)
            (display-cards board :cards board)
        )
    else if (eq input "d")
        (setl window (get-selected :content board))
        (if (not (eq window null))
            (setl card-path :path :card window)
            (set :path board card-path)
            (set-atr :confirm board "visible" true)
            (push board :confirmDiv board _(progn (set-atr :confirm board "visible" false) (load-cards board)))
        )
    else if (eq input "D")
        (setl window (get-selected :content board))
        (if (not (eq window null))
            (setl card-path :path :card window)
            (chat:remove-resource :connection board card-path)
            (load-cards board)
        )
    else if (eq input "enter")
        (setl window (get-selected :content board))
        (if (not (eq window null))
            (setl new-top (edit :card window))
            (set-child :window board new-top)
            (set-atr :window board "visible" true)
            (push board new-top 
                _(progn
                    (set-atr :window board "visible" false)
                    (setl new-card (get-card new-top))
                    (chat:add-resource :connection board :path :card new-top (get-card-json (get-template board new-card) new-card))
                    (set ; :index window :cards board new-card)
                    (set-cards board :cards board)
                    #(set-cards board (list new-card))
                )
           )
        )
    )
)
        #(setl json (js:read-json (open "CardTemplate.json")))
        #(setl card-json (js:read-json (open "Card.json")))
        #(setl template (parse-template json))
        #(setl new-card (parse-card template card-json))
        #(print (get-card-json template new-card))


(chat:add-visualiser chat:client "Board" Board)

(set init chat:init)

(defmethod load-cards ((board Board))
    (setl connection :connection board)
    (setl cards (map _(progn (setl ret 
        (parse-card board (js:read-json (in-stream (chat:get-content connection _))))) (set :path ret (chat:get-path _)) ret ) (chat:get-resources connection "/cards/*")))
    (set-cards board cards)
)

(defmethod init ((board Board) connection)
    (set :connection board connection)
    (setl template-dir (chat:get-resource connection "/templates"))
    (setl card-dir (chat:get-resource connection "/cards"))
    (if (eq template-dir null) (chat:add-resource connection "/templates" ""))
    (if (eq card-dir null) (chat:add-resource connection "/cards" ""))

    (setl templates (chat:get-resources connection "/templates/*"))
    (if (eq (len templates) 0)
        (set-atr :picker-container board "visible" true)
        (push board :picker board)
        (return null)
     else
        (setl template (parse-template (js:read-json (in-stream (chat:get-content connection (back templates))))))
        (setl :id template (chat:get-resource-id (back templates)))
        (set :template board template)
        (load-cards board)
    )
    #(setl cards (map _(get-content (statement connection))))
    #(setl messages (map _(Text _) messages))
    #(doit child messages (add-child :content board child))
)

(defmethod resource-published ((window Board) resource)
    #(add-child :content window (Text :content resource))
)

(defmethod board-command (&rest args)
    (setl board-dbs (filter _(eq :type _ "Board") (chat:get-databases)))
    (setl display-board null)
    (if (eq (len board-dbs) 0)
        (setl display-board (chat:create-db (chat:new-db "Board")))
     else 
        (setl display-board :0 board-dbs)
    )
    (chat:display-db display-board)
)

(chat:add-command "board" board-command)


#(setl json (js:read-json (open "CardTemplate.json")))
#(setl card-json (js:read-json (open "Card.json")))
#
#(setl template (parse-template json))
#(setl new-card (parse-card template card-json))
#(setl new-card2 (parse-card template card-json))
#(setl :content :0 :fields new-card "awoooga")
#
##(display-window (display new-card))
#(setl test-board (Board))
#(set :template test-board template)
##(push test-board null)
#(empty-card test-board)
#(set-cards test-board (list new-card new-card2))
#(display-window test-board)


#(display-window (Text "awooooga"))


#(import mbchat)
#(add-visualiser client "Chat" Chat)
#
#(defmethod init ((window Chat) connection)
#    (set :connection window connection)
#    (print "Initing")
#    (setl messages (map _(index _ "Content") (statement (db connection) "SELECT * FROM Resources ORDER BY RecievedTimestamp ASC")))
#    (print messages)
#    (setl messages (map _(Text _) messages))
#    (print messages)
#    (doit child messages (add-child :content window child))
#)
#
#(defmethod resource-published ((window Chat) resource)
#    (print resource)
#    (print :content resource)
#    (add-child :content window (Text :content resource))
#)
#
#(defmethod send-message ((window Chat) string)
#    (upload-resource :connection window string)
#)
