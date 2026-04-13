
#include<iostream>
#include<string>
#include <vector>
#include<csignal>
#include <sstream>

#ifdef _WIN32
//windows specific headers
#include<Windows.h>
#include<conio.h>
#include <namedpipeapi.h>
#include <unordered_set>
#else
//linux specifc headers
#include <sys/types.h>
#include<sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <array>
#endif

#ifdef _WIN32
struct Pipe {
	HANDLE read;
	HANDLE write;

	Pipe(HANDLE r, HANDLE w) : read(r), write(w) {};
		};

#endif

volatile std::sig_atomic_t signal_received = 0;
void SGINT_Received(int signal) {

	signal_received = 1;
}

std::unordered_set<std::string> builtins = {
	"dir", "echo", "copy", "del", "mkdir",
	"rmdir", "type", "cls", "set", "cd",
	"move", "ren", "attrib", "find", "findstr"
};



int main() {
	bool loop = true;
	std::string input = "";
	std::vector<std::string> history;
	int currentIndexHistory = 0;
	int c;
	int cursor = input.length();
#ifdef _WIN32
	wchar_t buffer[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, buffer);
	std::string currentDir(buffer, buffer + wcslen(buffer));
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(h, &mode);
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(h, mode);
#else
	char buffer[4096];
	getcwd(buffer,sizeof(buffer));
	std::string currentDir(buffer);

#endif
#ifndef _WIN32
	termios origin;
	tcgetattr(STDIN_FILENO, &origin);

	termios raw = origin;
	raw.c_lflag &= ~(ECHO | ICANON| ISIG);
	//raw.c_oflag &= ~(OPOST);
	tcsetattr(STDIN_FILENO,TCSAFLUSH, &raw);

#endif // !_WIN32

	std::signal(SIGINT, SGINT_Received);

	while (loop) {

		if (input.empty()) {
			std::cout << '\n';
			std::cout <<"\r" << currentDir << " myshell->" << std::flush;

		}
#ifdef _WIN32
		 c = _getch();
#else

		char temp;
		read(STDIN_FILENO, &temp, 1);
		c = temp;
#endif	

		if (c == 3) {
			std::cout << "\nctrl+c received\n";
#ifndef _WIN32
			tcsetattr(STDIN_FILENO, TCSANOW, &origin);
#endif
			return 0;
		}
#ifdef _WIN32
		else if (c == 13) {
#else
		else if( c==10 ) {
#endif
			if (input.empty()) {
				std::cout << '\n';
			}
			else {
				if (input == "exit") {
					loop = false;
#ifndef _WIN32
					tcsetattr(STDIN_FILENO, TCSANOW, &origin);
#endif
					break;
				}
				else if (input.substr(0, 3) == "cd ") {
#ifdef _WIN32
					std::wstring wInput(input.begin(), input.end());
					std::wstring extractedPath = wInput.substr(3);
					wchar_t* wpath = &extractedPath[0];
					if (SetCurrentDirectory(wpath)) {
						GetCurrentDirectory(MAX_PATH, buffer);
						currentDir = std::string(buffer, buffer + wcslen(buffer));
						history.emplace_back(input);
						currentIndexHistory = history.size();
						input.clear();
						cursor = 0;
					}
					else {
						std::cout << "\nfailed to find directory.\n";
					}
#else
					std::string extractedPath = input.substr(3);
					if (chdir(extractedPath.c_str())==0) {
						getcwd(buffer, sizeof(buffer));
						currentDir = std::string(buffer);
						history.emplace_back(input);
						currentIndexHistory = history.size();
						input.clear();
						cursor = 0;
					}
					else {
						std::cout << "\nfailed to find directory.\n";
						std::cout << "\r" << currentDir << " myshell->";
						std::cout << std::string(input.size() + 1, ' ');
						std::cout << "\r" << currentDir << " myshell->";
						std::cout << input << std::flush;
					}


#endif
				}

#ifdef _WIN32
				else if (input.find("|") != std::string::npos) {
					std::cout << '\n';
					std::string originalInput = input;
					std::stringstream ss(input);
					std::vector <std::string> commands;
					std::string command;
					while (std::getline(ss, command, '|'))
						commands.push_back(command);

					for (auto& cmd : commands) {
						// trim leading spaces
						size_t start = cmd.find_first_not_of(" \t");
						if (start != std::string::npos)
							cmd = cmd.substr(start);
						// trim trailing spaces
						size_t end = cmd.find_last_not_of(" \t");
						if (end != std::string::npos)
							cmd = cmd.substr(0, end + 1);
					}

					std::vector<Pipe>pipes;
					for (int i = 0;i < commands.size() - 1;i++) {
						HANDLE readH, writeH;
						SECURITY_ATTRIBUTES sa;
						sa.bInheritHandle = false;
						sa.nLength = sizeof(SECURITY_ATTRIBUTES);
						sa.lpSecurityDescriptor = NULL;

						if (!CreatePipe(&readH, &writeH, &sa, 0)) {
							std::cerr << "\n error creating pipe\n";
							return 1;
						}
						else
							pipes.emplace_back(Pipe(readH, writeH));
					}
					std::vector<PROCESS_INFORMATION> processes;
					for (int i = 0;i <= commands.size()-1;i++) {
						STARTUPINFO si;
						PROCESS_INFORMATION pi;
						ZeroMemory(&si, sizeof(si));
						ZeroMemory(&pi, sizeof(pi));
						si.cb = sizeof(si);
						si.dwFlags |= STARTF_USESTDHANDLES;
						if (i == 0) {
							SetHandleInformation(pipes[i].write, HANDLE_FLAG_INHERIT, 1);
							si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
							si.hStdOutput = pipes[i].write;
							si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
						}
						else if (i == (commands.size() - 1)) {
							SetHandleInformation(pipes[i-1].read, HANDLE_FLAG_INHERIT, 1);
							si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
							si.hStdInput = pipes[i - 1].read;
							si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
						}
						else {
							SetHandleInformation(pipes[i - 1].read, HANDLE_FLAG_INHERIT, 1);
							SetHandleInformation(pipes[i ].write, HANDLE_FLAG_INHERIT, 1);
							si.hStdInput = pipes[i-1].read;
							si.hStdOutput = pipes[i].write;
							si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
						}
						std::string firstWord = commands[i].substr(0, commands[i].find(' '));
						std::cout << "first word :" << firstWord;
						wchar_t* processCommand;
						std::wstring wInput = std::wstring(commands[i].begin(), commands[i].end());
						if (builtins.count(firstWord) > 0) {
							wInput = L"cmd.exe /C " + wInput;
							processCommand = &wInput[0];
						}
						else {
							processCommand = &wInput[0];
						}
						
						if (!CreateProcess(NULL, processCommand, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
							std::cerr << "\nerror creating process: " << GetLastError() << '\n';
							return 1;
						}
						else {
							if (i == 0)
								SetHandleInformation(pipes[i].write, HANDLE_FLAG_INHERIT, 0);
							else if (i == commands.size() - 1)
								SetHandleInformation(pipes[i - 1].read, HANDLE_FLAG_INHERIT, 0);
							else {
								SetHandleInformation(pipes[i - 1].read, HANDLE_FLAG_INHERIT, 0);
								SetHandleInformation(pipes[i].write, HANDLE_FLAG_INHERIT, 0);
							}
							processes.push_back(pi);
						}
					}

					for (auto& p : pipes) {
						CloseHandle(p.read);
						CloseHandle(p.write);
					}

					for (auto& process : processes) {
						WaitForSingleObject(process.hProcess,INFINITE);
						CloseHandle(process.hProcess);
						CloseHandle(process.hThread);
					}

					history.push_back(std::move(originalInput));
					currentIndexHistory = history.size();
					input.clear();
					cursor = 0;
				
				}
				else {
					std::cout << '\n';
					std::string originalInput = input;
					std::string firstWord = input.substr(0, input.find(' '));
					wchar_t* processCommand;
					std::wstring wInput = std::wstring(input.begin(), input.end());
					if (builtins.count(firstWord) > 0) {
						wInput = L"cmd.exe /C " + wInput;
						processCommand = &wInput[0];
					}
					else {
						processCommand = &wInput[0];
					}
					STARTUPINFO si;
					PROCESS_INFORMATION pi;
					ZeroMemory(&pi, sizeof(pi));
					ZeroMemory(&si, sizeof(si));
					si.cb = sizeof(si);
					if (!CreateProcess(NULL, processCommand, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
						std::cout << "\nfailed to execute command. error: " << GetLastError() << '\n';
					}
					else {
						WaitForSingleObject(pi.hProcess, INFINITE);
						CloseHandle(pi.hProcess);
						CloseHandle(pi.hThread);
						history.push_back(std::move(originalInput));
						currentIndexHistory = history.size();
						input.clear();
						cursor = 0;
					}
#else	
					if (input.find("|") == std::string::npos) {
						pid_t pid = fork();
						if (pid < 0) {
							std::cout << "current fork failed \n";
							return 1;
						}
						else if (pid == 0) {
							std::stringstream ss(input);
							std::vector<std::string> words;
							std::string word;

							while (ss >> word)
								words.push_back(word);

							std::vector<char*>args;
							for (auto& word : words)
								args.push_back(&word[0]);

							args.push_back(nullptr);
							std::cout << '\n';
							execvp(args[0], args.data());

							std::cerr << "\ncommand not found\n";
							exit(1);

						}
						else {
							wait(NULL);
							history.push_back(originalInput);
							currentIndexHistory = history.size();
							input.clear();
							cursor = 0;
						}
					}
					else {
						//extracting commands from input
						std::stringstream ss(input);
						std::vector<std::string>commands;
						std::string command;
						while (std::getline(ss, command, '|'))
							commands.emplace_back(command);
						// commands-1 pipes
						std::vector<std::array<int,2>>pipes;
						
						for (int i = 0;i < commands.size() - 1;i++) {
							pipes.push_back({});
							if (pipe(pipes.back().data()) == -1) {  
								std::cerr << "pipe failed\n";
								return 1;
							}
						}

						//forking and piping  children
						for (int i = 0;i <= commands.size()-1;i++) {
							pid_t pid = fork();
							if (pid < 0) {
								std::cout << "\nfork failed\n";
								return 1;
							}
							else if (pid == 0) {
								if (i == 0) {
									dup2(pipes[i][1], STDOUT_FILENO);
									//close other pipes
									for (int j = 0; j < pipes.size();j++) {
										if (j == 0) {
											close(pipes[j][0]);
									}
										else {
											close(pipes[j][0]);
											close(pipes[j][1]);
										}
									}
									//extracting command words and executing
									std::stringstream ss(commands[i]);
									std::vector<std::string> words;
									std::string word;
									while (ss >> word)
										words.push_back(word);
									std::vector<char*> args;
									for (auto& word : words)
										args.push_back(&word[0]);
									std::cout << '\n';
									args.push_back(nullptr);
									execvp(args[0], args.data());
									std::cerr<<" \nerror executing\n";
									exit(1);
								}
								else if (i == (commands.size() - 1)) {
									//piping
									dup2(pipes[i-1][0], STDIN_FILENO);
									//closing unused pipes
									for (int j = 0;j < pipes.size();j++) {
										if (j == (i - 1))
											close(pipes[j][1]);
										else {
											close(pipes[j][0]);
											close(pipes[j][1]);
										}
									}
									//extracting command words and executing
									std::stringstream ss(commands[i]);
									std::vector<std::string> words;
									std::string word;
									while (ss >> word)
										words.push_back(word);
									std::vector<char*> args;
									for (auto& word : words)
										args.push_back(&word[0]);
									args.push_back(nullptr);
									execvp(args[0], args.data());
									std::cerr<<" \nerror executing\n";
									exit(1);

								}
								else {
									//piping
									dup2(pipes[i - 1][0], STDIN_FILENO);
									dup2(pipes[i][1], STDOUT_FILENO);
									//closing unsused pipes
									for (int j = 0;j < pipes.size();j++) {
										if (j == (i - 1)) {
											close(pipes[j][1]);
										}
										else if (j == i) {
											close(pipes[j][0]);
										}
										else
										{
											close(pipes[j][0]);
											close(pipes[j][1]);
										}
									}
									//extracting command words and executing
									std::stringstream ss(commands[i]);
									std::vector<std::string> words;
									std::string word;
									while (ss >> word)
										words.push_back(word);
									std::vector<char*> args;
									for (auto& word : words)
										args.push_back(&word[0]);
									args.push_back(nullptr);
									execvp(args[0], args.data());
									std::cerr<<" \nerror executing\n";
									exit(1);
								}
							}

						}
						//closing Parent
						for (int i = 0;i < pipes.size();i++) {
							close(pipes[i][0]);
							close(pipes[i][1]);
						}
						//waiting for children
						for (int i = 0;i < commands.size();i++) {
							wait(NULL);
						}
						history.push_back(originalInput);
						currentIndexHistory = history.size();
						input.clear();
						cursor = 0;

							

					}
#endif
				}

			}
		}
#ifdef _WIN32
		else if (c == 8) {
#else
		else if( c==127 || c==8 ) {
#endif
			if (!input.empty()&& cursor>0) {
				input.erase(cursor-1, 1);
				//std::cout << "input right now:" << input;
				cursor--;
				std::cout << "\r" << currentDir << " myshell->";
				std::cout << std::string(input.size()+1, ' ');
				std::cout << "\r" << currentDir << " myshell->";
				std::cout << input << std::flush;
				if (cursor < input.length()) {
					std::string moveleft("\x1b[D");
					size_t bracketP = moveleft.find_first_of("[");
					moveleft.insert(bracketP + 1, std::to_string(input.length() - cursor));
					std::cout << moveleft << std::flush;
				}

			}
		}
#ifdef _WIN32
		else if (c == 224) {
			c = _getch();



				if (c == 72 && !history.empty()) { //
					currentIndexHistory--;
					if (currentIndexHistory < 0)
						currentIndexHistory = 0;
					std::cout << "\r" << currentDir << " myshell->";
					std::cout << std::string(input.size(), ' ');
					std::cout << "\r" << currentDir << " myshell->";
					input = history[currentIndexHistory];
					std::cout << input;
					cursor = input.length();
				}
				else if (c == 80 &&  !history.empty()) {
					currentIndexHistory++;
					if (currentIndexHistory >= (int)history.size())
						currentIndexHistory = history.size();
					std::cout << "\r" << currentDir << " myshell->";
					std::cout << std::string(input.size(), ' ');
					std::cout << "\r" << currentDir << " myshell->";
					if (currentIndexHistory < (int)history.size())
						input = history[currentIndexHistory];
					else
						input = "";
					std::cout << input;
					cursor = input.length();
				}
				else if (c == 75) { //left key
					if (cursor > 0) {
						cursor--;
						std::cout << "\x1b[D" <<std::flush;
					}
				}
				else if (c == 77) { // right key
					if (cursor < input.length()) {
						cursor++;
						std::cout << "\x1b[C" << std::flush;
					}
				}
			
		}
#else

		else if (c == 27) {
			char seq1, seq2;
			read(STDIN_FILENO, &seq1, 1);
			if (seq1 == 91) {
				read(STDIN_FILENO, &seq2, 1);

				if (seq2 == 65 && !history.empty()) {  // up — check history here
					currentIndexHistory--;
					if (currentIndexHistory < 0)
						currentIndexHistory = 0;
					std::cout << "\r" << currentDir << " myshell->";
					std::cout << std::string(input.size(), ' ');
					std::cout << "\r" << currentDir << " myshell->";
					input = history[currentIndexHistory];
					cursor = input.length();
					std::cout << input << std::flush;
				}
				else if (seq2 == 66 && !history.empty()) {  // down — check history here
					currentIndexHistory++;
					if (currentIndexHistory >= (int)history.size())
						currentIndexHistory = history.size();
					std::cout << "\r" << currentDir << " myshell->";
					std::cout << std::string(input.size(), ' ');
					std::cout << "\r" << currentDir << " myshell->";
					if (currentIndexHistory < (int)history.size())
						input = history[currentIndexHistory];
					else
						input = "";
					std::cout << input << std::flush;
					cursor = input.length();
				}
				else if (seq2 == 67) {  // right
					if (cursor < (int)input.length()) {
						write(STDOUT_FILENO, "\x1b[C", 3);
						cursor++;
					}

				}
				else if (seq2 == 68) {  // left 
					if (cursor > 0) {
						write(STDOUT_FILENO, "\x1b[D", 3);
						cursor--;
					}
					else
						std::cout << cursor;
				}
			}
}
#endif
		else {
#ifdef _WIN32
			char C = static_cast<char>(c);			
			std::string start = input.substr(0, cursor);
			std::string end = input.substr(cursor);
			std::string nInput = start +C+ end;
			std::cout << "\r" << currentDir << " myshell->";
			std::cout << std::string(nInput.size(), ' ');
			std::cout << "\r" << currentDir << " myshell->";
			input = nInput;
			std::cout << input;
			cursor++;
			if (cursor < input.length()) {
				std::string moveleft("\x1b[D");
				size_t bracketP = moveleft.find_first_of("[");
				//std::cout << " moving to left :" << (nInput.length() - cursor - 1);
				moveleft.insert(bracketP + 1, std::to_string(nInput.length() - cursor));
				std::cout << moveleft << std::flush;
			}
#else
			//input += c;
			std::string start = input.substr(0, cursor);
			std::string end = input.substr(cursor);
			std::string nInput = start + static_cast<char>(c) + end;
			std::cout << "\r" << currentDir << " myshell->";
			std::cout << std::string(nInput.size(), ' ');
			std::cout<<"\r" << currentDir<<" myshell->";
			input = nInput;
			std::cout << input <<std::flush;
			cursor++;
			if (cursor < input.length()) {
			std::string moveleft  ("\x1b[D");
			size_t bracketP = moveleft.find_first_of("[");
			moveleft.insert(bracketP + 1, std::to_string((input.length() - cursor)));
			std::cout << moveleft <<std::flush;
			}
#endif
		}
	}
	return 0;
}