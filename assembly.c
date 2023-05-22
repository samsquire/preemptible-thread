struct loop {
    volatile int loop_variable;
    volatile int limit;
};

int main() {
    static struct loop loop = {}; 
    loop.limit = 100000; 
    loop.loop_variable = loop.limit;
    loop.loop_variable++;
}
