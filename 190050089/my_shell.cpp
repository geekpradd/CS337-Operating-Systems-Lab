#include <unistd.h>
#include <iostream>
#include <unordered_set>
#include <sstream>
#include <signal.h>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
using namespace std;


int background_count = 0;
int foreground_parallel_count = 0;
bool foreground_running = 0;
unordered_set<int> background_processes;
unordered_set<int> foreground_parallel;
bool killed = false;

vector<string> split(string s, string delimiter){
    // splits string by delimiter
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
    killed = true; // killed = true is used for sequential fg processes to stop
    // bg processes are in seprate process group so they wont be killed 

    cout << "\n";
    if (!foreground_running)
        cout << "$ ";
    fflush(stdout);
}

void async_wait(int signal) {
    pid_t pid;
    
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) // this part of the code reaps child processes
    {
        if (background_processes.count(pid)){ // if child process was a backgroun process, notify and then add shell prompt
            cout << "Shell: Background process " << pid << " finished" << endl;
            background_count--;
            background_processes.erase(pid);

            if (!foreground_running)
                cout << "$ ";
        }
        else if (foreground_parallel.count(pid)){
            foreground_parallel_count--;
            foreground_parallel.erase(pid); // remove from running fg process set
        }
    }
    
    fflush(stdout);
}
void tokenize_commands(vector< vector<string> > &commands_tokenized, vector<string>& commands){
    // split by space here
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
    if (!tokens.size()) return; // empty command, do nothing
    int sz = tokens.size();

    bool background = 0;
    if (tokens[sz-1] == "&"){ // if last token is & this is a single bg command
        background = 1; // we set background flag
        sz--; // and reduce effective size
    }

    if (!sz) return; // if after this command is empty do nothing

    if (tokens[0] == "cd"){
        if (tokens.size() < 2){
            cout << "Shell: Incorrect command" << endl; // cd needs atleast two args
            return;
        }
        bool multiple_correct = true; // this flag is used for support of spaces where \ is used as delimiter

        string path_s = tokens[1]; // base path

        for (int i=2; i<tokens.size(); ++i){ // keep adding other arguments subject to \ being there as delimiter
            if (path_s[path_s.size()-1] == '\\') path_s.pop_back(); // remove the last \ char

            if (tokens[i-1][tokens[i-1].size()-1] != '\\'){ // if argument i is present then argument i-1 must have \ at end
                multiple_correct = false;
                break;
            }
            path_s += " " + tokens[i];
        }

        if (!multiple_correct){
            cout << "Shell: cd has too many arguments" << endl; // if multiple correct failed then too many arguments without \ separator
            return;
        }

        char* path = strdup(path_s.c_str());

        int errcode = chdir(path); // change the path

        if (errcode == -1){
            cout << "Shell: cd incorrect path "  << endl;
        }
        return;
    }
    
    if (tokens[0] == "exit"){
        for (int pid: background_processes){
            kill(pid, SIGKILL); // if exit command is there iterate over bg and kill them
        }

        for (int pid: foreground_parallel){
            kill(pid, SIGKILL); // exit can also be run in parallel fg mode, in this case we need to kill parallel fg processes as well
        }

        exit(0);
    }

    int rc = fork();
    if (rc < 0){
        cout << "Shell: Fork failed in shell for current command execution " << endl;
    }

    if (rc == 0){
        // this code runs in child

        char* args[sz+1];

        for (int i=0; i<sz; ++i) args[i] = strdup(tokens[i].c_str());
        args[sz] = NULL;

        execvp(args[0], args);

        for (int i=0; i<=sz; ++i) free(args[i]); // if execvp failed we need to manually free

        cout << "Shell: Command execution failed" << endl;
        exit(1);
        
    }
    else { // parent code
        if (!background && !parallel){// simple foreground sequential, here we have to wait in parent
            foreground_running = 1;
            waitpid(rc, NULL, 0); // wait for child
            foreground_running = 0;
        }
        else if (!background) {
            foreground_parallel_count++; // foreground parallel, don't wait but add to set so that we can say kill process later
            foreground_parallel.insert(rc);
        }
        else {
            background_count++; // insert background process into set here as well and also change the process group 
            setpgid(rc, rc);
            cout << "Background Process " << rc << " started " << endl;
            background_processes.insert(rc);
        }
    }
}

int main(){
    signal(SIGCHLD, async_wait); // this is the signal handler for the signal when child has terminated
    // we are using this to reap children on the fly
    signal(SIGINT, sigIntHandler); // this is the signal handler for the interrupt signal

    while (true){
        cout << "$ ";
        string input; 
        getline(cin, input); // get the input

        vector< vector<string> > commands_tokenized; // this vector contains individual commands, each stored as a 
        // vector of tokens

        if (input.find("&&&") != string::npos){
            vector<string> commands = split(input, "&&&"); // if &&& is there, this is parallel foreground
            // the split function defined above splits commands by delimiter

            tokenize_commands(commands_tokenized, commands);
            // this function will then break individual commands by spaces into tokens

            foreground_running = true; // just indictaes if there is some command running in fg
            for (vector<string> & tokens: commands_tokenized){
                execute_command(tokens, true); // execute with parallel mode on (no waiting for one command to end before going to next)
            }
            while (foreground_parallel_count > 0){
                // wait until foreground processes are reaped
            }
            foreground_running = false;
        }
        else if (input.find("&&") != string::npos){
            vector<string> commands = split(input, "&&"); // this is for sequential fg processes
            tokenize_commands(commands_tokenized, commands);

            foreground_running = true;

            killed = false; // this is used to indicate if sigint was sent to prevent next in sequence processes from running

            for (vector<string> & tokens: commands_tokenized){
                if (killed) break; // if killed then stop
                execute_command(tokens); // execute in sequential mode
            }
            foreground_running = false;
            killed = false;

        }
        else {
            // this is for single mode
            vector<string> commands = {input}; // only one command
            tokenize_commands(commands_tokenized, commands);
            vector<string> tokens = commands_tokenized[0]; // get the individual tokenized command
            execute_command(tokens); // and execute 
        }

        
    }
    
    return 0;
}