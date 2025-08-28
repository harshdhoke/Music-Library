#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <thread>

using namespace std;

struct SpinLock {
    volatile int lock_flag;

    SpinLock() : lock_flag(0) {}

    void lock() {
        while (__sync_lock_test_and_set(&lock_flag, 1)) {
        }
    }

    void unlock() {
        __sync_lock_release(&lock_flag);
    }
};

struct Query {
    int op;
    int node_id;
    int uid;
    bool is_sentinel = false;
};

class ThreadSafeQueue {
private:
    vector<Query> data;
    size_t head = 0;
    SpinLock spinlock;

public:
    void push(const Query& q) {
        spinlock.lock();
        data.push_back(q);
        spinlock.unlock();
    }

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


struct TreeLocker {
    int n, m;
    vector<int> parent;
    vector<int> lockedBy;
    vector<int> descLocked;
    SpinLock spinlock;

    TreeLocker(int n_, int m_) : n(n_), m(m_) {
        parent.assign(n, -1);
        lockedBy.assign(n, 0);
        descLocked.assign(n, 0);
        for (int i = 1; i < n; ++i) {
            parent[i] = (i - 1) / m;
        }
    }

    bool hasLockedAncestor(int v) {
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

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

        vector<int> descendantsToUnlock;
        stack<int> nodesToVisit;
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

void process_queries(ThreadSafeQueue& queue, TreeLocker& tl) {
    while (true) {
        Query q;
        if (queue.pop(q)) {
            if (q.is_sentinel) {
                break;
            }

            bool res = false;
            if (q.op == 1) {
                res = tl.lockNode(q.node_id, q.uid);
            } else if (q.op == 2) {
                res = tl.unlockNode(q.node_id, q.uid);
            } else if (q.op == 3) {
                res = tl.upgradeNode(q.node_id, q.uid);
            }
            cout << (res ? "true" : "false") << "\n";
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, m, Q;
    if (!(cin >> N)) return 0;
    cin >> m >> Q;

    unordered_map<string, int> name_to_id;
    name_to_id.reserve(N);
    for (int i = 0; i < N; ++i) {
        string name;
        cin >> name;
        name_to_id[name] = i;
    }

    TreeLocker tl(N, m);
    ThreadSafeQueue queue;

    thread worker_thread(process_queries, ref(queue), ref(tl));

    for (int i = 0; i < Q; ++i) {
        int op;
        string node_name;
        long long uid;
        cin >> op >> node_name >> uid;

        Query q;
        q.op = op;
        q.node_id = name_to_id[node_name];
        q.uid = (int)uid;

        queue.push(q);
    }

    Query sentinel_query;
    sentinel_query.is_sentinel = true;
    queue.push(sentinel_query);

    worker_thread.join();

    return 0;
}









////////////////////////////////////////////////////////////////////////////////////////////////////////////////////








#include <iostream>      // For input/output operations (cin, cout).
#include <vector>        // For using the dynamic array 'vector'.
#include <string>        // For using the 'string' class.
#include <unordered_map> // For using the hash-table-based 'unordered_map'.
#include <stack>         // For using the 'stack' data structure (LIFO).
#include <thread>        // For creating and managing threads.

// This line brings all names from the standard (std) namespace into the
// current scope. This allows us to use names like 'cout', 'vector', etc.,
// without the 'std::' prefix.
using namespace std;

// --- Thread Safety Primitives ---

// A SpinLock implementation using GCC/Clang compiler intrinsics.
// This is a low-level lock that repeatedly checks if it can acquire the lock.
// It's used here because the prompt forbids using standard libraries like <mutex>.
struct SpinLock {
    // 'volatile' tells the compiler that this value can change unexpectedly
    // by another thread. This prevents the compiler from making optimizations
    // (like caching the value in a register) that might break the lock's logic.
    volatile int lock_flag;

    // Constructor: Initializes the lock as 'unlocked' (0).
    SpinLock() : lock_flag(0) {}

    // Acquires the lock. This is a "blocking" call, but it busy-waits.
    void lock() {
        // '__sync_lock_test_and_set' is a compiler built-in function that performs
        // an atomic "test-and-set" operation.
        // 1. It atomically sets the value of 'lock_flag' to 1.
        // 2. It returns the *previous* value of 'lock_flag'.
        // The loop continues ("spins") as long as the previous value was 1,
        // which means another thread already held the lock.
        while (__sync_lock_test_and_set(&lock_flag, 1)) {
            // This is a busy-wait loop. The thread does nothing but check the lock
            // repeatedly, consuming CPU cycles. This is efficient for very short
            // lock durations but inefficient if locks are held for a long time.
        }
    }

    // Releases the lock.
    void unlock() {
        // '__sync_lock_release' is a compiler built-in that atomically sets
        // the lock_flag to 0. It also acts as a memory barrier, ensuring that all
        // memory writes made by this thread *before* calling unlock() are visible
        // to other threads *after* they acquire the lock.
        __sync_lock_release(&lock_flag);
    }
};

// --- Query Data Structure ---

// A simple struct to hold the data for a single query.
// This makes it easy to pass all the necessary information through the queue.
struct Query {
    int op;           // The operation type (1: lock, 2: unlock, 3: upgrade).
    int node_id;      // The integer ID of the node to operate on.
    int uid;          // The user ID performing the operation.
    bool is_sentinel = false; // A special flag to signal the end of the query stream.
};

// --- Custom Thread-Safe Queue ---

// A queue that can be safely accessed by multiple threads (one producer, one consumer).
// It uses our SpinLock to ensure that only one thread can modify the queue at a time.
class ThreadSafeQueue {
private: // Encapsulation: internal data is private.
    vector<Query> data; // The underlying storage for the queue, a dynamic array.
    size_t head = 0;    // An index pointing to the front of the queue. We don't remove elements, just move the head.
    SpinLock spinlock;  // The lock to protect access to 'data' and 'head'.

public:
    // Pushes a new query to the back of the queue.
    void push(const Query& q) {
        spinlock.lock();   // Acquire the lock to prevent other threads from interfering.
        data.push_back(q); // Add the new query to the end of the vector.
        spinlock.unlock(); // Release the lock so other threads can use the queue.
    }

    // Tries to pop a query from the front of the queue.
    bool pop(Query& q) {
        spinlock.lock();   // Acquire the lock to get exclusive access.
        // Check if there are any unread items in the queue (if the head hasn't caught up to the end).
        if (head < data.size()) {
            q = data[head];    // Copy the query from the front.
            head++;            // Move the head forward to the next item. This is faster than erasing.
            spinlock.unlock(); // Release the lock.
            return true;       // Return true to indicate a query was successfully popped.
        }
        spinlock.unlock(); // Release the lock if the queue was empty.
        return false;      // Return false to indicate the queue is currently empty.
    }
};


// --- Tree Locking Mechanism (Thread-Safe) ---

// This struct manages the state of the tree and all locking operations.
// It is designed to be thread-safe by using a single SpinLock to protect all its data.
struct TreeLocker {
    int n, m;                 // n: number of nodes, m: number of children per node.
    vector<int> parent;       // Stores the parent of each node. Index is node ID, value is parent's ID.
    vector<int> lockedBy;     // Stores the UID of the user who locked a node (0 if unlocked).
    vector<int> descLocked;   // A count of how many *directly* locked descendants each node has.
    SpinLock spinlock;        // A lock to protect all the vectors above from concurrent access.

    // Constructor: Initializes the tree structure.
    TreeLocker(int n_, int m_) : n(n_), m(m_) {
        // Resize and initialize all vectors.
        parent.assign(n, -1);     // All nodes start with no parent (-1), except the root.
        lockedBy.assign(n, 0);    // All nodes start unlocked (locked by UID 0).
        descLocked.assign(n, 0);  // All nodes start with zero locked descendants.

        // Pre-calculates the parent of every node based on its index in the m-ary tree.
        // The root is node 0. Node i's parent is at index (i-1)/m.
        for (int i = 1; i < n; ++i) {
            parent[i] = (i - 1) / m;
        }
    }

    // Helper function to check if any ancestor of a node is locked.
    // This must be called only after acquiring the spinlock to ensure consistent reads.
    bool hasLockedAncestor(int v) {
        int p = parent[v]; // Start with the immediate parent.
        while (p != -1) {  // Loop until we reach the root's parent (-1).
            if (lockedBy[p] != 0) return true; // If an ancestor is locked, return true.
            p = parent[p]; // Move up to the next ancestor.
        }
        return false; // No locked ancestors were found.
    }

    // Helper function to update the locked-descendant count for all ancestors.
    // 'delta' is +1 for locking and -1 for unlocking.
    // This must be called only after acquiring the spinlock.
    void updateAncestorDescLockCount(int v, int delta) {
        int p = parent[v]; // Start with the immediate parent.
        while (p != -1) {  // Loop up to the root.
            descLocked[p] += delta; // Increment or decrement the ancestor's count.
            p = parent[p]; // Move to the next ancestor.
        }
    }

    // Tries to lock a node for a given user. Returns true on success, false on failure.
    bool lockNode(int v, int uid) {
        spinlock.lock(); // Lock to ensure exclusive access to the tree's state.

        // A node can be locked only if all three conditions are met:
        // 1. It is not already locked by someone else.
        // 2. It has no locked ancestors (locking an ancestor locks the whole subtree).
        // 3. It has no locked descendants (a parent cannot be locked if a child is).
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] != 0) {
            spinlock.unlock(); // If conditions fail, release the lock.
            return false;      // Report failure.
        }

        // If conditions are met, perform the lock operation.
        lockedBy[v] = uid; // Mark the node as locked by the user.
        updateAncestorDescLockCount(v, 1); // Increment the locked-descendant count for all its ancestors.
        spinlock.unlock(); // Release the lock.
        return true;       // Report success.
    }

    // Tries to unlock a node for a given user. Returns true on success, false on failure.
    bool unlockNode(int v, int uid) {
        spinlock.lock(); // Lock for exclusive access.

        // A node can only be unlocked if it was locked by the *same* user.
        if (lockedBy[v] != uid) {
            spinlock.unlock(); // If not locked by this user, release the lock.
            return false;      // Report failure.
        }

        // If condition is met, perform the unlock.
        lockedBy[v] = 0; // Mark the node as unlocked.
        updateAncestorDescLockCount(v, -1); // Decrement the locked-descendant count for all ancestors.
        spinlock.unlock(); // Release the lock.
        return true;       // Report success.
    }

    // Tries to upgrade a lock on a node for a given user. Returns true on success, false on failure.
    bool upgradeNode(int v, int uid) {
        spinlock.lock(); // Lock for exclusive access, as this is a complex operation.

        // Upgrade is possible only if:
        // 1. The node itself is currently unlocked.
        // 2. It has no locked ancestors.
        // 3. It has at least one locked descendant (otherwise, there's nothing to upgrade).
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] == 0) {
            spinlock.unlock(); // If checks fail, release the lock.
            return false;      // Report failure.
        }

        vector<int> descendantsToUnlock; // To store a list of descendants that need to be unlocked.
        stack<int> nodesToVisit;         // Use a stack for a Depth-First Search (DFS) of the subtree.
        nodesToVisit.push(v);            // Start the search from the current node 'v'.
        bool canUpgrade = true;          // A flag to track if the upgrade is permissible.

        // Traverse the descendants to check if they are all locked by the same user 'uid'.
        // This is a "check-only" phase; no changes are made yet.
        while (!nodesToVisit.empty()) {
            int u = nodesToVisit.top(); // Get the next node to check from the stack.
            nodesToVisit.pop();         // Remove it from the stack.
            // Calculate the index of the first child of node 'u'.
            long long firstChild = 1LL * u * m + 1;
            // Iterate through all possible children of 'u'.
            for (long long j = 0; j < m; ++j) {
                long long childIndex = firstChild + j; // Calculate the child's index.
                if (childIndex >= n) break; // Stop if the child index is out of bounds.
                int w = static_cast<int>(childIndex); // Convert to int for vector access.

                if (lockedBy[w] != 0) { // If this child is directly locked...
                    // ...check if it's locked by a *different* user.
                    if (lockedBy[w] != uid) {
                        canUpgrade = false; // If so, the upgrade is not possible.
                        break;              // Stop checking children.
                    }
                    // If locked by the correct user, add it to the list of nodes to unlock later.
                    descendantsToUnlock.push_back(w);
                } else if (descLocked[w] > 0) {
                    // If the child is not locked but has locked descendants, we need to search its subtree.
                    nodesToVisit.push(w);
                }
            }
            if (!canUpgrade) break; // If we found a violation, exit the main DFS loop.
        }

        // If the check phase failed, abort the entire operation without making changes.
        if (!canUpgrade) {
            spinlock.unlock(); // Release the lock.
            return false;      // Report failure.
        }

        // If the check passed, proceed to the "modify" phase.
        // First, unlock all the descendants that were identified.
        for (int u : descendantsToUnlock) {
            lockedBy[u] = 0; // Unlock the descendant node.
            updateAncestorDescLockCount(u, -1); // Update ancestor counts for this unlock operation.
        }
        // Second, lock the current node itself.
        lockedBy[v] = uid; // Lock node 'v' for the user.
        updateAncestorDescLockCount(v, 1); // Update ancestor counts for this lock operation.
        spinlock.unlock(); // Finally, release the lock.
        return true;       // Report success.
    }
};

// --- Consumer/Worker Function ---

// This is the function that will run on the separate worker thread.
// It takes references to the shared queue and tree locker.
void process_queries(ThreadSafeQueue& queue, TreeLocker& tl) {
    while (true) { // Loop indefinitely, constantly checking for work.
        Query q;
        // Continuously try to pop a query from the queue. This is a non-blocking check.
        if (queue.pop(q)) {
            // If the popped query is the sentinel, it's a signal from the main thread to stop.
            if (q.is_sentinel) {
                break; // Exit the loop and terminate the thread.
            }

            // Process the query based on its operation type.
            bool res = false; // Variable to store the result of the operation.
            if (q.op == 1) { // Operation 1: Lock
                res = tl.lockNode(q.node_id, q.uid);
            } else if (q.op == 2) { // Operation 2: Unlock
                res = tl.unlockNode(q.node_id, q.uid);
            } else if (q.op == 3) { // Operation 3: Upgrade
                res = tl.upgradeNode(q.node_id, q.uid);
            }
            // Print the boolean result to standard output, followed by a newline.
            cout << (res ? "true" : "false") << "\n";
        }
        // If queue.pop(q) returned false, the queue was empty. The loop immediately
        // continues, effectively "spinning" and re-checking the queue for new work.
    }
}


// --- Main Execution (Producer) ---

int main() {
    // Standard C++ optimization for faster input/output.
    ios::sync_with_stdio(false); // Unties C++ streams from C streams.
    cin.tie(nullptr);            // Prevents 'cin' from flushing 'cout' before each input.

    int N, m, Q; // N: nodes, m: children per node, Q: queries.
    if (!(cin >> N)) return 0; // Read N; if input fails (e.g., EOF), exit gracefully.
    cin >> m >> Q; // Read m and Q.

    // Create a map to convert string node names to integer IDs for efficiency.
    // Using integer IDs is much faster than comparing strings in the tree logic.
    unordered_map<string, int> name_to_id;
    name_to_id.reserve(N); // Pre-allocate memory to avoid resizing the map, which is slow.
    for (int i = 0; i < N; ++i) {
        string name;
        cin >> name;       // Read the node name.
        name_to_id[name] = i; // Assign it a unique integer ID (0 to N-1).
    }

    // Create the shared resources that both the main and worker threads will use.
    TreeLocker tl(N, m);
    ThreadSafeQueue queue;

    // Launch the consumer/worker thread. It starts running the 'process_queries' function immediately.
    // 'ref' is used to pass the queue and tree locker by reference. Without it, the thread
    // would get copies, and the communication would fail.
    thread worker_thread(process_queries, ref(queue), ref(tl));

    // The main thread now acts as the producer. It reads input and adds it to the queue.
    for (int i = 0; i < Q; ++i) {
        int op;
        string node_name;
        long long uid;
        cin >> op >> node_name >> uid; // Read the query details.

        // Create a Query object with the input data.
        Query q;
        q.op = op;
        q.node_id = name_to_id[node_name]; // Convert the node name to its integer ID.
        q.uid = (int)uid;                  // Cast the user ID to an int.

        // Push the query into the thread-safe queue. The worker thread can now access and process it.
        queue.push(q);
    }

    // After all real queries have been pushed, push a special 'sentinel' query.
    // This tells the worker thread that there is no more work and it should shut down gracefully.
    Query sentinel_query;
    sentinel_query.is_sentinel = true;
    queue.push(sentinel_query);

    // The main thread waits here until the worker thread has finished its execution.
    // This is crucial to ensure all queries are processed before the program exits.
    // If we didn't 'join', 'main' might finish while the worker is still running,
    // causing the program to terminate prematurely.
    worker_thread.join();

    return 0; // Successful program termination.
}
