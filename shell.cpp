#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <fstream>

static const char* kPathColor = "\x1b[38;2;255;190;83m";
static const char* kResetColor = "\x1b[0m";

static void doType(const std::string& filename);

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string expandEnvVars(const std::string& in) {
    std::string out;
    size_t i = 0;
    while (i < in.size()) {
        if (in[i] == '%') {
            size_t end = in.find('%', i + 1);
            if (end != std::string::npos) {
                std::string name = in.substr(i + 1, end - i - 1);
                char buf[32767];
                DWORD n = GetEnvironmentVariableA(name.c_str(), buf, sizeof(buf));
                if (n > 0) {
                    out += buf;
                } else {
                    out += '%' + name + '%';
                }
                i = end + 1;
                continue;
            }
        }
        out += in[i++];
    }
    return out;
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && isspace((unsigned char)c)) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

static std::string g_lastExitCode = "0";

static bool runBuiltin(const std::vector<std::string>& args, bool& shouldExit) {
    if (args.empty()) return true;
    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "exit") {
        shouldExit = true;
        return true;
    }
    if (cmd == "cd" || cmd == "chdir") {
        if (args.size() < 2) {
            char buf[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, buf);
            std::cout << buf << "\n";
        } else {
            std::string target = expandEnvVars(args[1]);
            if (!SetCurrentDirectoryA(target.c_str())) {
                std::cout << "The system cannot find the path specified.\n";
            }
        }
        return true;
    }
    if (cmd == "pwd") {
        char buf[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, buf);
        std::cout << buf << "\n";
        return true;
    }
    if (cmd == "cls" || cmd == "clear") {
        std::cout << "\x1b[2J\x1b[H";
        std::cout.flush();
        return true;
    }
    if (cmd == "echo") {
        std::string rest;
        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) rest += " ";
            rest += args[i];
        }
        std::cout << expandEnvVars(rest) << "\n";
        return true;
    }
    if (cmd == "set") {
        if (args.size() == 1) {
            LPCH env = GetEnvironmentStringsA();
            for (LPCH p = env; *p; p += strlen(p) + 1) std::cout << p << "\n";
            FreeEnvironmentStringsA(env);
        } else {
            std::string kv = args[1];
            size_t eq = kv.find('=');
            if (eq != std::string::npos) {
                std::string key = kv.substr(0, eq);
                std::string val = expandEnvVars(kv.substr(eq + 1));
                SetEnvironmentVariableA(key.c_str(), val.c_str());
            }
        }
        return true;
    }
    if (cmd == "dir") {
        std::string path = args.size() > 1 ? expandEnvVars(args[1]) : ".";
        WIN32_FIND_DATAA fd;
        std::string pattern = path + "\\*";
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) {
            std::cout << "File Not Found\n";
            return true;
        }
        do {
            bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            std::cout << (isDir ? "<DIR>  " : "       ") << fd.cFileName << "\n";
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        return true;
    }
    if (cmd == "mkdir" || cmd == "md") {
        if (args.size() > 1) CreateDirectoryA(expandEnvVars(args[1]).c_str(), NULL);
        return true;
    }
    if (cmd == "rmdir" || cmd == "rd") {
        if (args.size() > 1) RemoveDirectoryA(expandEnvVars(args[1]).c_str());
        return true;
    }
    if (cmd == "del" || cmd == "erase") {
        if (args.size() > 1) DeleteFileA(expandEnvVars(args[1]).c_str());
        return true;
    }
    if (cmd == "copy") {
        if (args.size() >= 3) {
            std::string source = expandEnvVars(args[1]);
            std::string dest = expandEnvVars(args[2]);
            if (!CopyFileA(source.c_str(), dest.c_str(), FALSE)) {
                std::cout << "The system cannot find the file specified.\n";
            }
        } else {
            std::cout << "The syntax of the command is incorrect.\n";
        }
        return true;
    }
    if (cmd == "type") {
        if (args.size() > 1) {
            std::string path;
            for (size_t i = 1; i < args.size(); i++) {
                if (i > 1) path += " ";
                path += args[i];
            }
            doType(expandEnvVars(path));
        } else {
            std::cout << "The syntax of the command is incorrect.\n";
        }
        return true;
    }
    if (cmd == "help") {
        std::cout <<
            "Built-in commands: cd, pwd, dir, cls, echo, set, mkdir, rmdir, del,\n"
            "                   copy, type, exit\n"
            "Anything else is run as an external program found on PATH.\n"
            "Supports: redirection (>, >>, <) and piping (|)\n";
        return true;
    }

    return false;
}

static void doType(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f) { std::cout << "The system cannot find the file specified.\n"; return; }
    std::cout << f.rdbuf();
}

static HANDLE openRedirectFile(const std::string& path, DWORD access, DWORD disposition, DWORD shareMode) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), NULL, TRUE };
    return CreateFileA(path.c_str(), access, shareMode, &sa, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
}

static int runExternal(const std::string& commandLine, HANDLE hStdIn, HANDLE hStdOut, HANDLE hStdErr) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdIn;
    si.hStdOutput = hStdOut;
    si.hStdError = hStdErr;

    std::string mutableCmd = commandLine;
    std::vector<char> buf(mutableCmd.begin(), mutableCmd.end());
    buf.push_back('\0');

    BOOL ok = CreateProcessA(
        NULL, buf.data(), NULL, NULL, TRUE,
        0, NULL, NULL, &si, &pi);

    if (!ok) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

struct Redirection {
    std::string cmd;
    std::string outFile;
    bool append = false;
    std::string inFile;
};

static Redirection parseRedirection(const std::string& segment) {
    Redirection r;
    std::vector<std::string> parts;
    {
        std::string cur;
        bool inQuotes = false;
        for (size_t i = 0; i < segment.size(); i++) {
            char c = segment[i];
            if (c == '"') { inQuotes = !inQuotes; cur += c; continue; }
            if (!inQuotes && (c == '>' || c == '<')) {
                if (!cur.empty()) { parts.push_back(trim(cur)); cur.clear(); }
                if (c == '>' && i + 1 < segment.size() && segment[i + 1] == '>') {
                    parts.push_back(">>"); i++;
                } else {
                    parts.push_back(std::string(1, c));
                }
                continue;
            }
            if (!inQuotes && isspace((unsigned char)c)) {
                if (!cur.empty()) { parts.push_back(trim(cur)); cur.clear(); }
                continue;
            }
            cur += c;
        }
        if (!cur.empty()) parts.push_back(trim(cur));
    }

    for (size_t i = 0; i < parts.size(); i++) {
        if (parts[i] == ">" || parts[i] == ">>") {
            r.append = (parts[i] == ">>");
            if (i + 1 < parts.size()) {
                std::string f = parts[++i];
                if (f.size() >= 2 && f.front() == '"' && f.back() == '"') f = f.substr(1, f.size() - 2);
                r.outFile = f;
            }
        } else if (parts[i] == "<") {
            if (i + 1 < parts.size()) {
                std::string f = parts[++i];
                if (f.size() >= 2 && f.front() == '"' && f.back() == '"') f = f.substr(1, f.size() - 2);
                r.inFile = f;
            }
        } else {
            if (!r.cmd.empty()) r.cmd += " ";
            r.cmd += parts[i];
        }
    }
    return r;
}

static void enableAnsiIfPossible() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

int main() {
    enableAnsiIfPossible();
    std::cout << "\n\t\t\t======================\n"
              << "\t\t\t\tHello!\n"
              << "\t\t\t     \"BashByAbeer\"\n"
              << "\t\t\t======================\n"
              << "Hello There!\n"
              << "Welcome to the terminal made by Me. Type 'exit' to quit.\n\n"
              << std::flush;

    std::string line;
    bool shouldExit = false;
    while (!shouldExit) {
        char buf[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, buf);
        std::cout << kResetColor << "# " << kPathColor << "[" << buf << "]" << kResetColor << " ";
        if (!std::getline(std::cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;

        std::vector<std::string> args = tokenize(line);
        if (!runBuiltin(args, shouldExit) && !shouldExit) {
            Redirection r = parseRedirection(line);
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);

            bool errorOccurred = false;

            if (!r.inFile.empty()) {
                hIn = openRedirectFile(r.inFile, GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ);
                if (hIn == INVALID_HANDLE_VALUE) {
                    std::cout << "The system cannot find the file specified.\n";
                    errorOccurred = true;
                }
            }

            if (!errorOccurred && !r.outFile.empty()) {
                DWORD disposition = r.append ? OPEN_ALWAYS : CREATE_ALWAYS;
                hOut = openRedirectFile(r.outFile, GENERIC_WRITE, disposition, FILE_SHARE_READ | FILE_SHARE_WRITE);
                if (hOut != INVALID_HANDLE_VALUE && r.append) {
                    SetFilePointer(hOut, 0, NULL, FILE_END);
                }
            }

            if (!errorOccurred) {
                int code = runExternal(r.cmd, hIn, hOut, hErr);
                if (code == -1) {
                    std::cout << "'" << (args.empty() ? r.cmd : args[0]) << "' is not recognized as an internal or external command, operable program or batch file.\n";
                    g_lastExitCode = "1";
                } else {
                    g_lastExitCode = std::to_string(code);
                }
            }

            if (hIn != GetStdHandle(STD_INPUT_HANDLE) && hIn != INVALID_HANDLE_VALUE) {
                CloseHandle(hIn);
            }
            if (hOut != GetStdHandle(STD_OUTPUT_HANDLE) && hOut != INVALID_HANDLE_VALUE) {
                CloseHandle(hOut);
            }
        }
    }
    return 0;
}