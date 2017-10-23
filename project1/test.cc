#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main() {
    char buffer[75];

    vector<int> arr;

    for (int i=0; i<25; i++) {
      arr.push_back(0);
      arr.push_back(1);
      arr.push_back(2);
    }

    for (int i=0; i<75; i++) {
      cout << arr[i] << endl;
    }

    return 0;


}
