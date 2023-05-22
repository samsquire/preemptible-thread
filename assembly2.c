struct loop {
    volatile int loop_variable;
    volatile int limit;
};

int main() {
    static volatile struct loop loop = {}; 
    for (volatile int i = loop.loop_variable ; loop.loop_variable < 100000 ; loop.loop_variable++) {
        
    }
}
