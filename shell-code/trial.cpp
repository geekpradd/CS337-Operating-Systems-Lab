#include <bits/stdc++.h>
using namespace std;


int main() {
    int n = 1000000000;
    vector<int> vals;
    for (int i=1; i<=n; ++i){
        vals.push_back(n-i);
    }

    sort(vals.begin(), vals.end());

    return 0;
}
