#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

int background_count = 0;


int main(){
    while (true){
        pid_t pid;
    
        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
            cout << "[" << background_count << "] " << pid <<  " done " << endl;
            background_count--;
        }

        cout << "$ ";
        string input; 
        getline(cin, input);

        stringstream input_stream(input);
        vector<string> tokens;
        string buffer;

        while (input_stream >> buffer) {
            tokens.push_back(buffer);
        }

        if (!tokens.size()) continue;
        int sz = tokens.size();

        bool background = 0;
        if (tokens[sz-1] == "&"){
            background = 1;
            sz--;
        }

        if (!sz) continue;    
        if (tokens[0] == "cd"){
            if (tokens.size() != 2){
                cout << "Shell: Incorrect command" << endl;
                continue;
            }
            char* path = strdup(tokens[1].c_str());
            int errcode = chdir(path);

            if (errcode == -1){
                cout << "Shell: Incorrect command" << endl;
            }
            continue;
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

            cout << "Shell: Command execution failed" << endl;
            exit(1);
            
        }
        else {
            if (!background)
                waitpid(rc, NULL, 0); // wait for child
            else {
                background_count++;
                cout << "[" << background_count << "] " << rc << endl;

            }
        }
        
    }
    
    return 0;
}