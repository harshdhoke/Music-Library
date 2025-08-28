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












//////////////////////////////////////////////////////////////////////////////////////////////////////////////







#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <thread>

// --- Thread Safety Primitives ---

// A SpinLock implementation using GCC/Clang compiler intrinsics.
// This is a low-level lock that repeatedly checks if it can acquire the lock.
// It's used here because the prompt forbids using standard libraries like <mutex>.
struct SpinLock {
    // 'volatile' tells the compiler that this value can change unexpectedly,
    // preventing certain optimizations that might break the lock's logic.
    volatile int lock_flag;

    // Constructor: Initializes the lock as 'unlocked' (0).
    SpinLock() : lock_flag(0) {}

    // Acquires the lock.
    void lock() {
        // '__sync_lock_test_and_set' is a compiler built-in function that performs
        // an atomic "test-and-set" operation. It sets lock_flag to 1 and
        // returns the *previous* value. The loop continues ("spins") as long
        // as the previous value was 1 (meaning the lock was already held).
        while (__sync_lock_test_and_set(&lock_flag, 1)) {
            // This is a busy-wait loop. The thread does nothing but check the lock
            // repeatedly, consuming CPU cycles.
        }
    }

    // Releases the lock.
    void unlock() {
        // '__sync_lock_release' is a compiler built-in that atomically sets
        // the lock_flag to 0 and ensures that all previous memory writes
        // are visible to other threads before the lock is released.
        __sync_lock_release(&lock_flag);
    }
};

// --- Query Data Structure ---

// A simple struct to hold the data for a single query.
// This makes it easy to pass all the necessary information through the queue.
struct Query {
    int op;          // The operation type (1: lock, 2: unlock, 3: upgrade).
    int node_id;     // The integer ID of the node to operate on.
    int uid;         // The user ID performing the operation.
    bool is_sentinel = false; // A special flag to signal the end of the query stream.
};

// --- Custom Thread-Safe Queue ---

// A queue that can be safely accessed by multiple threads at the same time.
// It uses our SpinLock to ensure that only one thread can modify the queue at a time.
class ThreadSafeQueue {
    std::vector<Query> data; // The underlying storage for the queue.
    size_t head = 0;         // An index pointing to the front of the queue.
    SpinLock spinlock;       // The lock to protect access to 'data' and 'head'.

public:
    // Pushes a new query to the back of the queue.
    void push(const Query& q) {
        spinlock.lock();         // Acquire the lock to prevent other threads from interfering.
        data.push_back(q);       // Add the new query to the end of the vector.
        spinlock.unlock();       // Release the lock so other threads can use the queue.
    }

    // Tries to pop a query from the front of the queue.
    bool pop(Query& q) {
        spinlock.lock();         // Acquire the lock.
        // Check if there are any unread items in the queue.
        if (head < data.size()) {
            q = data[head];      // Copy the query from the front.
            head++;              // Move the head forward to the next item.
            spinlock.unlock();   // Release the lock.
            return true;         // Return true to indicate success.
        }
        spinlock.unlock();       // Release the lock if the queue was empty.
        return false;            // Return false to indicate the queue is empty.
    }
};


// --- Tree Locking Mechanism (Thread-Safe) ---

// This struct manages the state of the tree and all locking operations.
// It is designed to be thread-safe by using a SpinLock internally.
struct TreeLocker {
    int n, m;                     // n: number of nodes, m: number of children per node.
    std::vector<int> parent;      // Stores the parent of each node.
    std::vector<int> lockedBy;    // Stores the UID of the user who locked a node (0 if unlocked).
    std::vector<int> descLocked;  // A count of how many locked descendants each node has.
    SpinLock spinlock;            // A lock to protect all the vectors above from concurrent access.

    // Constructor: Initializes the tree structure.
    TreeLocker(int n_, int m_) : n(n_), m(m_) {
        parent.assign(n, -1);
        lockedBy.assign(n, 0);
        descLocked.assign(n, 0);
        // Pre-calculates the parent of every node based on its index.
        for (int i = 1; i < n; ++i) {
            parent[i] = (i - 1) / m;
        }
    }

    // Helper function to check if any ancestor of a node is locked.
    // This must be called only after acquiring the spinlock.
    bool hasLockedAncestor(int v) {
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

    // Helper function to update the locked-descendant count for all ancestors.
    // 'delta' is +1 for locking and -1 for unlocking.
    // This must be called only after acquiring the spinlock.
    void updateAncestorDescLockCount(int v, int delta) {
        int p = parent[v];
        while (p != -1) {
            descLocked[p] += delta;
            p = parent[p];
        }
    }

    // Tries to lock a node for a given user.
    bool lockNode(int v, int uid) {
        spinlock.lock(); // Lock to ensure exclusive access to the tree's state.
        // A node can be locked only if it's not already locked, has no locked
        // ancestors, and has no locked descendants.
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] != 0) {
            spinlock.unlock();
            return false;
        }
        // If conditions are met, lock the node and update ancestor counts.
        lockedBy[v] = uid;
        updateAncestorDescLockCount(v, 1);
        spinlock.unlock(); // Release the lock.
        return true;
    }

    // Tries to unlock a node for a given user.
    bool unlockNode(int v, int uid) {
        spinlock.lock(); // Lock for exclusive access.
        // A node can only be unlocked if it was locked by the same user.
        if (lockedBy[v] != uid) {
            spinlock.unlock();
            return false;
        }
        // Unlock the node and update ancestor counts.
        lockedBy[v] = 0;
        updateAncestorDescLockCount(v, -1);
        spinlock.unlock(); // Release the lock.
        return true;
    }

    // Tries to upgrade a lock on a node for a given user.
    bool upgradeNode(int v, int uid) {
        spinlock.lock(); // Lock for exclusive access.
        // Upgrade is possible only if the node is not locked, has no locked ancestors,
        // and has at least one locked descendant.
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] == 0) {
            spinlock.unlock();
            return false;
        }

        std::vector<int> descendantsToUnlock;
        std::stack<int> nodesToVisit;
        nodesToVisit.push(v);
        bool canUpgrade = true;

        // Traverse the descendants to check if they are all locked by the same user.
        while (!nodesToVisit.empty()) {
            int u = nodesToVisit.top();
            nodesToVisit.pop();
            long long firstChild = 1LL * u * m + 1;
            for (long long j = 0; j < m; ++j) {
                long long childIndex = firstChild + j;
                if (childIndex >= n) break;
                int w = static_cast<int>(childIndex);
                if (lockedBy[w] != 0) {
                    // If any descendant is locked by a different user, fail the upgrade.
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

        // If the check failed, abort the operation.
        if (!canUpgrade) {
            spinlock.unlock();
            return false;
        }

        // If the check passed, unlock all descendants...
        for (int u : descendantsToUnlock) {
            lockedBy[u] = 0;
            updateAncestorDescLockCount(u, -1);
        }
        // ...and lock the current node.
        lockedBy[v] = uid;
        updateAncestorDescLockCount(v, 1);
        spinlock.unlock(); // Release the lock.
        return true;
    }
};

// --- Consumer/Worker Function ---

// This is the function that will run on the separate worker thread.
// It takes references to the shared queue and tree locker.
void process_queries(ThreadSafeQueue& queue, TreeLocker& tl) {
    while (true) {
        Query q;
        // Continuously try to pop a query from the queue.
        if (queue.pop(q)) {
            // If the popped query is the sentinel, it's a signal to stop.
            if (q.is_sentinel) {
                break; // Exit the loop and terminate the thread.
            }

            // Process the query based on its operation type.
            bool res = false;
            if (q.op == 1) {
                res = tl.lockNode(q.node_id, q.uid);
            } else if (q.op == 2) {
                res = tl.unlockNode(q.node_id, q.uid);
            } else if (q.op == 3) {
                res = tl.upgradeNode(q.node_id, q.uid);
            }
            // Print the result to standard output.
            std::cout << (res ? "true" : "false") << "\n";
        }
        // If the queue was empty, the loop continues, effectively "spinning"
        // and re-checking the queue for work.
    }
}


// --- Main Execution (Producer) ---

int main() {
    // Standard C++ optimization for faster input/output.
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int N, m, Q;
    if (!(std::cin >> N)) return 0;
    std::cin >> m >> Q;

    // Create a map to convert string node names to integer IDs for efficiency.
    std::unordered_map<std::string, int> name_to_id;
    name_to_id.reserve(N); // Pre-allocate memory to avoid rehashes.
    for (int i = 0; i < N; ++i) {
        std::string name;
        std::cin >> name;
        name_to_id[name] = i;
    }

    // Create the shared resources that both threads will use.
    TreeLocker tl(N, m);
    ThreadSafeQueue queue;

    // Launch the consumer/worker thread. It starts running 'process_queries' immediately.
    // 'std::ref' is used to pass the queue and tree locker by reference to the new thread.
    std::thread worker_thread(process_queries, std::ref(queue), std::ref(tl));

    // The main thread now acts as the producer. It reads input and adds it to the queue.
    for (int i = 0; i < Q; ++i) {
        int op;
        std::string node_name;
        long long uid;
        std::cin >> op >> node_name >> uid;
        
        // Create a Query object with the input data.
        Query q;
        q.op = op;
        q.node_id = name_to_id[node_name];
        q.uid = (int)uid;
        
        // Push the query into the thread-safe queue. The worker thread can now access it.
        queue.push(q);
    }

    // After all real queries have been pushed, push a special 'sentinel' query.
    // This tells the worker thread that there is no more work and it should shut down.
    Query sentinel_query;
    sentinel_query.is_sentinel = true;
    queue.push(sentinel_query);

    // The main thread waits here until the worker thread has finished its execution.
    // This is crucial to ensure all queries are processed before the program exits.
    worker_thread.join();

    return 0;
}

