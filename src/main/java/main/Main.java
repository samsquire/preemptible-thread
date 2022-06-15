import java.util.*;

class Main {

    public interface Premptible {
        void registerLoop(String name, Integer defaultValue, Integer limit);
        int increment(String name);
        boolean isPreempted(String name);
        int getLimit(String name);
        int getValue(String name);
    }

    public static class Worker extends Thread implements Premptible {
        Map<String, Integer> values = new HashMap<>();
        Map<String, Integer> limits = new HashMap<>();
        Map<String, Boolean> preempted = new HashMap<>();
        public void run() {
            registerLoop("i", 0, 10000000);
            for (initialVar("i", 0); getValue("i") < getLimit("i"); increment("i")) {
                Math.sqrt(getValue("i"));
            }
            if (isPreempted("i")) {
                System.out.println(String.format("%s was prempted !%d < %d", "i", values.get("i"), limits.get("i")));
            }
        }

        public void registerLoop(String name, Integer defaultValue, Integer limit) {
            values.put(name, defaultValue);
            limits.put(name, limit);
            preempted.put(name, false);

        }

        public int increment(String name) {
            int value = values.get(name);
            value++;
            values.put(name, value);
            return value;
        }

        public boolean isPreempted(String name) {
            return preempted.get(name);
        }

        public int getLimit(String name) {
            return limits.get(name);
        }

        public int getValue(String name) {
            return values.get(name);
        }

        public int initialVar(String name, int value) {
            values.put(name, value);
            return value;
        }

        public void prempt(String name) {
            preempted.put(name, true);
            values.put(name, limits.get("i"));
        }

    }

    public static void main(String[] args) throws InterruptedException {
        System.out.println("Hello world!");
        Worker worker = new Worker();
        worker.start();
        Thread.sleep(10);
        worker.prempt("i");
        worker.join();
    }
}