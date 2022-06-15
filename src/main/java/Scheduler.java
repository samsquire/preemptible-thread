import java.util.*;

public class Scheduler {

    public static class KernelThread extends Thread {
        public Map<LightWeightThread, Boolean> scheduled = new HashMap<>();

        public void preempt() {
            for (LightWeightThread thread : scheduled.keySet()) {
                scheduled.put(thread, false);
                for (String loop : thread.getLoops()) {

                    thread.preempt(loop);
                }
            }

        }
        public void addLightWeightThread(LightWeightThread thread) {
            scheduled.put(thread, false);
        }

        public boolean isScheduled(LightWeightThread lightWeightThread) {
            return scheduled.get(lightWeightThread);
        }
        public void run() {

            while (true) {
                LightWeightThread previous = null;
                for (LightWeightThread thread : scheduled.keySet()) {
                    scheduled.put(thread, false);
                }
                for (LightWeightThread thread : scheduled.keySet()) {
                    if (previous != null) {
                        scheduled.put(previous, false);
                    }
                    scheduled.put(thread, true);
                    thread.run();

                    previous = thread;
                }
            }
        }
    }

    public interface Preemptible {
        void registerLoop(String name, Integer defaultValue, Integer limit);
        int increment(String name);
        boolean isPreempted(String name);
        int getLimit(String name);
        int getValue(String name);
        void preempt(String name);
        Set<String> getLoops();
    }

    public static abstract class LightWeightThread implements Preemptible {
        public int kernelThreadId;
        public int threadId;
        public KernelThread parent;
        Map<String, Integer> values = new HashMap<>();
        Map<String, Integer> limits = new HashMap<>();
        Map<String, Boolean> preempted = new HashMap<>();
        Map<String, Integer> remembered = new HashMap<>();
        public LightWeightThread(int kernelThreadId, int threadId, KernelThread parent) {
            this.kernelThreadId = kernelThreadId;
            this.threadId = threadId;
            this.parent = parent;
        }
        public void run() {

        }

        public void registerLoop(String name, Integer defaultValue, Integer limit) {
            if (remembered.containsKey(name) && remembered.get(name) < limit) {
                values.put(name, remembered.get(name));
                limits.put(name, limit);
            } else {
                values.put(name, defaultValue);
                limits.put(name, limit);
            }
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

        public void preempt(String name) {
            remembered.put(name, values.get(name));
            preempted.put(name, true);
            values.put(name, limits.get(name));
        }

        public Set<String> getLoops() {
            return values.keySet();
        }
    }

    public static void main(String[] args) throws InterruptedException {
        List<KernelThread> kernelThreads = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            KernelThread kt = new KernelThread();
            for (int j = 0 ; j < 5; j++) {
                LightWeightThread lightWeightThread = new LightWeightThread(i, j, kt) {
                    @Override
                    public void run() {

                            while (this.parent.isScheduled(this)) {
                                System.out.println(String.format("%d %d", this.kernelThreadId, this.threadId));
                                registerLoop("i", 0, 10000000);
                                for (initialVar("i", 0); getValue("i") < getLimit("i"); increment("i")) {
                                    Math.sqrt(getValue("i"));
                                }

                                if (isPreempted("i")) {
                                    System.out.println(String.format("%d %d: %s was preempted !%d < %d", this.kernelThreadId, this.threadId, "i", values.get("i"), limits.get("i")));
                                }
                            }

                        }

                };
                kt.addLightWeightThread(lightWeightThread);
            }

            kernelThreads.add(kt);
        }
        for (KernelThread kt : kernelThreads) {
            kt.start();
        }
        Timer timer = new Timer();
        timer.schedule(new TimerTask() {
            @Override
            public void run() {
                for (KernelThread kt : kernelThreads) {

                    kt.preempt();
                }
            }
        }, 10, 10);

        for (KernelThread kt : kernelThreads) {
            kt.join();
        }
    }

}
