/*
To Run This File:
Press F5, then type this One-By-One:
  g++ Notification.cpp -o Notification.exe
  .\Notification.exe
*/
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
    MessageBox(NULL, "Hello, from BashByAbeer!", "NOTIFICATION", MB_OK | MB_ICONINFORMATION);
    return 0;
}