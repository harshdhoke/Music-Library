#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <thread>

// --- Thread Safety Primitives ---

// A SpinLock implementation using GCC/Clang compiler intrinsics.
// This avoids using <atomic> or <mutex> libraries.
struct SpinLock {
    volatile int lock_flag;

    SpinLock() : lock_flag(0) {}

    void lock() {
        while (__sync_lock_test_and_set(&lock_flag, 1)) {
            // Busy-wait (spin) until the lock is acquired.
        }
    }

    void unlock() {
        __sync_lock_release(&lock_flag);
    }
};

// --- Query Data Structure ---

// A struct to hold query data, making it easy to pass through the queue.
struct Query {
    int op;
    int node_id;
    int uid;
    bool is_sentinel = false; // Flag to signal the end of work.
};

// --- Custom Thread-Safe Queue ---

// A simple, custom thread-safe queue for the producer-consumer model.
// It uses our SpinLock for synchronization.
// Note: All members are public by default in a struct.
class ThreadSafeQueue {
    std::vector<Query> data;
    size_t head = 0;
    SpinLock spinlock;

public: // Explicitly marking public for clarity, though not strictly necessary
    // Pushes a query to the back of the queue.
    void push(const Query& q) {
        spinlock.lock();
        data.push_back(q);
        spinlock.unlock();
    }

    // Tries to pop a query from the front of the queue.
    // Returns true if a query was popped, false otherwise.
    bool pop(Query& q) {
        spinlock.lock();
        if (head < data.size()) {
            q = data[head];
            head++;
            spinlock.unlock();
            return true;
        }
        spinlock.unlock();
        return false;
    }
};


// --- Tree Locking Mechanism (Thread-Safe) ---

// Note: All members are public by default in a struct.
struct TreeLocker {
    int n, m;
    std::vector<int> parent;
    std::vector<int> lockedBy;
    std::vector<int> descLocked;
    SpinLock spinlock; // A spinlock to protect the tree's internal state.

    TreeLocker(int n_, int m_) : n(n_), m(m_) {
        parent.assign(n, -1);
        lockedBy.assign(n, 0);
        descLocked.assign(n, 0);
        for (int i = 1; i < n; ++i) {
            parent[i] = (i - 1) / m;
        }
    }

    // Helper function to check for locked ancestors.
    bool hasLockedAncestor(int v) {
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

    // Helper function to update the descendant lock count for all ancestors.
    void updateAncestorDescLockCount(int v, int delta) {
        int p = parent[v];
        while (p != -1) {
            descLocked[p] += delta;
            p = parent[p];
        }
    }

    bool lockNode(int v, int uid) {
        spinlock.lock();
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] != 0) {
            spinlock.unlock();
            return false;
        }
        lockedBy[v] = uid;
        updateAncestorDescLockCount(v, 1);
        spinlock.unlock();
        return true;
    }

    bool unlockNode(int v, int uid) {
        spinlock.lock();
        if (lockedBy[v] != uid) {
            spinlock.unlock();
            return false;
        }
        lockedBy[v] = 0;
        updateAncestorDescLockCount(v, -1);
        spinlock.unlock();
        return true;
    }

    bool upgradeNode(int v, int uid) {
        spinlock.lock();
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] == 0) {
            spinlock.unlock();
            return false;
        }

        std::vector<int> descendantsToUnlock;
        std::stack<int> nodesToVisit;
        nodesToVisit.push(v);
        bool canUpgrade = true;

        while (!nodesToVisit.empty()) {
            int u = nodesToVisit.top();
            nodesToVisit.pop();
            long long firstChild = 1LL * u * m + 1;
            for (long long j = 0; j < m; ++j) {
                long long childIndex = firstChild + j;
                if (childIndex >= n) break;
                int w = static_cast<int>(childIndex);
                if (lockedBy[w] != 0) {
                    if (lockedBy[w] != uid) {
                        canUpgrade = false;
                        break;
                    }
                    descendantsToUnlock.push_back(w);
                } else if (descLocked[w] > 0) {
                    nodesToVisit.push(w);
                }
            }
            if (!canUpgrade) break;
        }

        if (!canUpgrade) {
            spinlock.unlock();
            return false;
        }

        for (int u : descendantsToUnlock) {
            lockedBy[u] = 0;
            updateAncestorDescLockCount(u, -1);
        }
        lockedBy[v] = uid;
        updateAncestorDescLockCount(v, 1);
        spinlock.unlock();
        return true;
    }
};

// --- Consumer/Worker Function ---

// The worker function that will run on a separate thread.
void process_queries(ThreadSafeQueue& queue, TreeLocker& tl) {
    while (true) {
        Query q;
        // Continuously try to pop from the queue.
        if (queue.pop(q)) {
            // If it's the sentinel value, stop processing.
            if (q.is_sentinel) {
                break;
            }

            // Process the actual query.
            bool res = false;
            if (q.op == 1) {
                res = tl.lockNode(q.node_id, q.uid);
            } else if (q.op == 2) {
                res = tl.unlockNode(q.node_id, q.uid);
            } else if (q.op == 3) {
                res = tl.upgradeNode(q.node_id, q.uid);
            }
            std::cout << (res ? "true" : "false") << "\n";
        }
        // If the queue is empty, this thread will effectively "spin,"
        // repeatedly checking the queue until there's work to do.
    }
}


// --- Main Execution (Producer) ---

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int N, m, Q;
    if (!(std::cin >> N)) return 0;
    std::cin >> m >> Q;

    std::unordered_map<std::string, int> name_to_id;
    name_to_id.reserve(N);
    for (int i = 0; i < N; ++i) {
        std::string name;
        std::cin >> name;
        name_to_id[name] = i;
    }

    // Create shared resources.
    TreeLocker tl(N, m);
    ThreadSafeQueue queue;

    // Launch the consumer/worker thread.
    std::thread worker_thread(process_queries, std::ref(queue), std::ref(tl));

    // Main thread acts as the producer.
    for (int i = 0; i < Q; ++i) {
        int op;
        std::string node_name;
        long long uid;
        std::cin >> op >> node_name >> uid;
        
        Query q;
        q.op = op;
        q.node_id = name_to_id[node_name];
        q.uid = (int)uid;
        
        queue.push(q);
    }

    // After pushing all real queries, push a sentinel value
    // to signal the worker thread to terminate.
    Query sentinel_query;
    sentinel_query.is_sentinel = true;
    queue.push(sentinel_query);

    // Wait for the worker thread to finish its job.
    worker_thread.join();

    return 0;
}
