# S12URootkit
User Mode Windows Rootkit able to hide **Processes, Files, Directories, Registry Key, Registry Value**.

Undetectable at the moment in Windows Defender and BitDefender Free Version Antivirus s

[ ! ] **You need Administrator Privileges!**

[ ! ] **Before execute commands to hide some values, if in this victim machine never is executed this binary execute first the UserModeR00tkit.exe without arguments of the command**

[ ! ] **In some machines the process hide feature only work after restart the machine once the base EXE are executed without arguments**

# Hide Files & Directories Video

https://youtu.be/CJ7oBdPjSvQ

# Hide Process Video

[ ! ] **In some machines the process hide feature only work after restart the machine once the base EXE are executed without arguments**

[ ! ] **Some process are dont hide, if this occurs try executing first the process than the Task Manager**

https://youtu.be/6yCC_IIjWTI

# Hide Registry Video

https://www.youtube.com/watch?v=AhS1ofR_pJc

# Features

Process:

- Hide Processes in Task Manager

Files & Directories:
        
- Hide Files & Directories in File Explorer (explorer.exe)

Registry:
        
- Registries and Values in regedit.exe

![image](https://github.com/S12cybersecurity/S12URootkit/assets/79543461/31e9c6f6-d5c2-465a-b04b-e729522394ec)

# Commands

Process:

- rootkit.exe process hide processname.exe

Path:
      
- rootkit.exe path hide C:\Users\Public\Music

Registry:
      
- rootkit.exe registry hide valuetohide

![image](https://github.com/S12cybersecurity/S12URootkit/assets/79543461/41f21755-5027-48cb-803f-64d0493b48c4)

# Detection

Evade Windows Defender:
- [x] Static Analysis:
      
![image](https://github.com/S12cybersecurity/S12URootkit/assets/79543461/4525e871-b05a-457e-90b8-9efec1097d5c)

- [x] Execution/Dynamic Analysis:
  
**Not detected in Execution Time! (4/1/2024)**

Detected After restart!

![image](https://github.com/S12cybersecurity/S12URootkit/assets/79543461/b1be6632-ffae-4020-8469-0a6fc6389352)

Evade Classic AV (BitDefender Free Version):

- [x] Static Analysis:
      
![image](https://github.com/S12cybersecurity/S12URootkit/assets/79543461/93a8be14-431f-4cb3-9473-486d80e58094)

- [x] Execution/Dynamic Analysis:

**Not detected in Execution Time! (4/1/2024)**
