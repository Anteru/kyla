rmdir /S /Q install v1-i v2-i
kcl build --source-directory data-1 v-1.xml v1-i
kcl build --source-directory data-2 v-2.xml v2-i
kcl install v1-i install 23ba19b4-5162-41b3-8ff1-4d104604901e 25d05924-a2f9-488f-988d-40d6e64706c1
REM kcl configure v2-i install b1b913b6-c903-48c0-88be-f4863a599484 d842ad8b-eaf2-4ae4-83fa-59fbe0939eb9