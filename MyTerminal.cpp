/*
NOTE: TO RUN THIS FILE, WHEN THE TERMINAL OPENS, FIRST TYPE THIS:
    g++ MyTerminal.cpp -o MyTerminal.exe
THEN WHEN NEXT LINE APPEARS, TYPE THIS:
    .\MyTerminal.exe
AND MY TERMINAL APPEARS!
*/
#define _WIN_WINNT 0x0600
#include <iostream>
#include <string>
#include <windows.h>
void set_terminal_color() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFOEX csbi;
    csbi.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    GetConsoleScreenBufferInfoEx(hConsole, &csbi);
    csbi.ColorTable[0] = RGB(0, 33, 58); 
    csbi.ColorTable[7] = RGB(213, 230, 255); 
    SetConsoleScreenBufferInfoEx(hConsole, &csbi);
    SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | 0x07);
    system("cls"); 

}

int main() {
   
    SetConsoleTitleA("BashByAbeer");
    set_terminal_color();
    std::cout << "\t\t\t======================\n";
    std::cout << "\t\t\t\tHello!\n";
    std::cout << "\t\t\t     \"BashByAbeer\"\n";
    std::cout << "\t\t\t======================\n";
    std::cout << "Hello There!\n";
    std::cout << "Welcome to terminal made by Me. Type 'exit' to quit.\n\n";
    std::string command;

    while (true) {
       std::cout << "# ";  
        if (!std::getline(std::cin, command)) break;
        if (!command.empty() && command.back() == '\r') {
            command.pop_back();
        }
        if (command == "exit" || command == "quit") {
            break;
        }
        if (!command.empty()) {
            system(command.c_str());
            std::cout << "\n";
        }
    }

    return 0;
}
