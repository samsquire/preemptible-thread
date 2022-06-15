import java.util.*;

public class Scheduler {

    public static class KernelThread extends Thread {
        public Map<LightWeightThread, Boolean> scheduled = new HashMap<>();

        public void preempt() {
            for (LightWeightThread thread : scheduled.keySet()) {
                scheduled.put(thread, false);
                for (int loop = 0 ; loop < thread.getLoops(); loop++) {
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
        void registerLoop(int name, int defaultValue, int limit);
        int increment(int name);
        boolean isPreempted(int name);
        int getLimit(int name);
        int getValue(int name);
        void preempt(int id);
        int getLoops();
    }

    public static abstract class LightWeightThread implements Preemptible {
        public int kernelThreadId;
        public int threadId;
        public KernelThread parent;
        int[] values = new int[1];
        int[] limits = new int[1];
        boolean[] preempted = new boolean[1];
        int[] remembered = new int[1];
        public LightWeightThread(int kernelThreadId, int threadId, KernelThread parent) {
            this.kernelThreadId = kernelThreadId;
            this.threadId = threadId;
            this.parent = parent;
        }
        public void run() {

        }

        public void registerLoop(int name, int defaultValue, int limit) {
            if (preempted.length > name && remembered[name] < limit) {
                values[name] = remembered[name];
                limits[name] = limit;
            } else {
                values[name] = defaultValue;
                limits[name] = limit;
            }
            preempted[name] = false;

        }

        public int increment(int name) {
            int value = values[name];
            value++;
            values[name] = value;
            return value;
        }

        public boolean isPreempted(int name) {
            return preempted[name];
        }

        public int getLimit(int name) {
            return limits[name];
        }

        public int getValue(int name) {
            return values[name];
        }

        public int initialVar(int name, int value) {
            values[name] = value;
            return value;
        }

        public void preempt(int id) {
            remembered[id] = values[id];
            preempted[id] = true;
            values[id] = limits[id];
        }

        public int getLoops() {
            return values.length;
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
                                registerLoop(0, 0, 10000000);
                                for (initialVar(0, 0); getValue(0) < getLimit(0); increment(0)) {
                                    Math.sqrt(getValue(0));
                                }

                                if (isPreempted(0)) {
                                    System.out.println(String.format("%d %d: %d was preempted !%d < %d", this.kernelThreadId, this.threadId, 0, values[0], limits[0]));
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
