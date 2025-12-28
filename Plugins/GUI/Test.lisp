(import TML.TML)
@tml
<test db = "">
    <Text highlight-bg-color="green" content=(+ db "")   />
</test>

@tml
<testList>
    @{
        (doit elem (list "asdasd" "xdczxcxz" "zxczxczxc")
            (emit-child 
                @tml-emit
                <test db=elem />
            )
        )
    }

</testList>
