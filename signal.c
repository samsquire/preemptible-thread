#include<stdio.h>
#include<signal.h>
#include<unistd.h>

void sigalrm_handler(int);
int loop_function();

static int counter = 1;
static long int n = 1;

int main()
{
    signal(SIGALRM, sigalrm_handler);
    //This will set the alarm for the first time.
    //Subsequent alarms will be set from sigalrm_handler() function
    alarm(1);

    printf("Inside main\n");

    while(1)
    {
        printf("Inside loop");
        loop_function();
    }
    return(0);
}

void sigalrm_handler(int sig)
{
    printf("inside alarm signal handler\n");
    printf("%d: %d\n",counter,n);
    //exit(1);
    alarm(1);
}

int loop_function()
{
        ++counter;
        n++;
}
