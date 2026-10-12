*** Settings ***
Library    SerialLibrary

*** Test Cases ***
Correct Time
    Open Port    COM6
    Send Text    000120
    ${result}=    Read Line
    Should Be Equal    ${result}    80
    Close Port

Invalid Seconds
    Open Port    COM6
    Send Text    000060
    ${result}=    Read Line
    Should Be Equal    ${result}    -3
    Close Port