#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

int main(){
    while (true){
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
            char* args[tokens.size()+1];

            for (int i=0; i<tokens.size(); ++i) args[i] = strdup(tokens[i].c_str());
            args[tokens.size()] = NULL;

            execvp(args[0], args);

            cout << "Shell: Command execution failed" << endl;
            exit(1);
            
        }
        else {
            int wc = wait(NULL); // wait for child
        }
        
    }
    
    return 0;
}