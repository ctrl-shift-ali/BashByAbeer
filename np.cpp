/*
THIS IS A NOTIFICATION FILE!.
FIRST IN TERMINAL TYPE "    g++ np.cpp -o np.exe -mwindows    "
THEN TYPE THIS IN NEXT LINE "    .\np.exe    "
Click Enter and File will run! :)
*/

#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
    MessageBox(NULL, "Hello there, I am Ali Abeer.", "NOTIFICATION", MB_OK | MB_ICONINFORMATION);
    return 0;
}
