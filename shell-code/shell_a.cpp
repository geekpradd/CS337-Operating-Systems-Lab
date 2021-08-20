#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

int background_count = 0;
int foreground_parallel_count = 0;
bool foreground_running = 0;
unordered_set<int> background_processes;
unordered_set<int> foreground_parallel;
bool killed = false;

vector<string> split(string s, string delimiter){
    vector<string> result;

    int pos = 0;
    string token;
    while ((pos = s.find(delimiter)) != string::npos) {
        token = s.substr(0, pos);
        result.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    result.push_back(s);
    return result;
}
void sigIntHandler(int signal){
    killed = true;
    cout << "\n";
}

void async_wait(int signal) {
    pid_t pid;
    
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        if (background_processes.count(pid)){
            cout << "Shell: Background process " << pid << " finished" << endl;
            background_count--;
            background_processes.erase(pid);

            if (!foreground_running)
                cout << "$ ";
        }
        else if (foreground_parallel.count(pid)){
            foreground_parallel_count--;
            foreground_parallel.erase(pid);
        }
    }
    
    fflush(stdout);
}
void tokenize_commands(vector< vector<string> > &commands_tokenized, vector<string>& commands){
    for (string command: commands){
        stringstream input_stream(command);
        vector<string> tokens;
        string buffer;

        while (input_stream >> buffer) {
            tokens.push_back(buffer);
        }
        commands_tokenized.push_back(tokens);
    }
}

void execute_command(vector<string> & tokens, bool parallel = false){
    if (!tokens.size()) return;
    int sz = tokens.size();

    bool background = 0;
    if (tokens[sz-1] == "&"){
        background = 1;
        sz--;
    }

    if (!sz) return;    
    if (tokens[0] == "cd"){
        if (tokens.size() != 2){
            cout << "Shell: Incorrect command" << endl;
            return;
        }
        char* path = strdup(tokens[1].c_str());
        int errcode = chdir(path);

        if (errcode == -1){
            cout << "Shell: Incorrect command" << endl;
        }
        return;
    }
    
    if (tokens[0] == "exit"){
        for (int pid: background_processes){
            kill(pid, SIGKILL);
        }

        for (int pid: foreground_parallel){
            kill(pid, SIGKILL);
        }

        exit(0);
    }

    int rc = fork();
    if (rc < 0){
        cout << "Shell: Fork failed in Shell for current command execution " << endl;
    }

    if (rc == 0){
        // child
        char* args[sz+1];

        for (int i=0; i<sz; ++i) args[i] = strdup(tokens[i].c_str());
        args[sz] = NULL;

        execvp(args[0], args);

        for (int i=0; i<=sz; ++i) free(args[i]);

        cout << "Shell: Command execution failed" << endl;
        exit(1);
        
    }
    else {
        if (!background && !parallel){
            foreground_running = 1;
            waitpid(rc, NULL, 0); // wait for child
            foreground_running = 0;
        }
        else if (!background) {
            foreground_parallel_count++;
            foreground_parallel.insert(rc);
        }
        else {
            background_count++;
            setpgid(rc, rc);
            cout << "Background Process " << rc << " started " << endl;
            background_processes.insert(rc);
        }
    }
}

int main(){
    signal(SIGCHLD, async_wait);
    signal(SIGINT, sigIntHandler);

    while (true){
        cout << "$ ";
        string input; 
        getline(cin, input);

        
        int mode = 0; // 0 for single background/foreground
        vector< vector<string> > commands_tokenized;

        if (input.find("&&&") != string::npos){
            mode = 1;
            vector<string> commands = split(input, "&&&");
            tokenize_commands(commands_tokenized, commands);

            foreground_running = true;
            for (vector<string> & tokens: commands_tokenized){
                execute_command(tokens, true);
            }
            while (foreground_parallel_count > 0){
                // wait until foreground processes are reaped
            }
            foreground_running = false;
        }
        else if (input.find("&&") != string::npos){
            mode = 2;
            vector<string> commands = split(input, "&&");
            tokenize_commands(commands_tokenized, commands);

            foreground_running = true;
            killed = false;
            for (vector<string> & tokens: commands_tokenized){
                if (killed) break;
                execute_command(tokens);
            }
            foreground_running = false;
            killed = false;

        }
        else {
            vector<string> commands = {input};
            tokenize_commands(commands_tokenized, commands);
            vector<string> tokens = commands_tokenized[0];
            execute_command(tokens);
        }

        
    }
    
    return 0;
}