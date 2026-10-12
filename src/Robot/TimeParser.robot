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

Wrong Length
    Open Port    COM6
    Send Text    12345
    ${result}=    Read Line
    Should Be Equal    ${result}    -1
    Close Port

Invalid Hour
    Open Port    COM6
    Send Text    250000
    ${result}=    Read Line
    Should Be Equal    ${result}    -3
    Close Port

Invalid Minute
    Open Port    COM6
    Send Text    006001
    ${result}=    Read Line
    Should Be Equal    ${result}    -3
    Close Port

Maximum Time
    Open Port    COM6
    Send Text    235959
    ${result}=    Read Line
    Should Be Equal    ${result}    86399
    Close Port