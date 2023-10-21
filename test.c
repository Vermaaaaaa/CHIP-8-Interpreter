#include <stdio.h>

int tester(int n){
    if(!n){
        printf("The value of !n is %d" , !n);
    }
    else{
        printf("The value of n is %d" , n);
    }
}


int main(){
    tester(1);

}