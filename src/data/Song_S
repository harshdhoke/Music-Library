#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <algorithm>

using namespace std;

// A simple spinlock for thread safety.
// It uses a volatile integer to prevent compiler optimizations that might break the lock.
// In a real-world, high-contention scenario, a more sophisticated lock (like std::mutex) would be better
// to avoid "busy-waiting" which consumes CPU cycles.
struct SpinLock {
    volatile int locked; // 0 for unlocked, 1 for locked. Volatile tells the compiler this value can change unexpectedly.

    SpinLock() : locked(0) {} // Constructor initializes the lock as unlocked.

    // Acquires the lock by continuously checking the 'locked' flag.
    void lock() {
        while (true) {
            // Attempt to acquire the lock. If it was 0, it becomes 1 and we own the lock.
            if (locked == 0) {
                locked = 1;
                return; // Exit the loop once the lock is acquired.
            }
        }
    }

    // Releases the lock.
    void unlock() {
        locked = 0; // Simply reset the flag to 0.
    }
};

// Main class to handle the tree locking logic.
struct TreeLocker {
    int n, m; // n: total nodes, m: number of children per node (m-ary).
    vector<int> parent; // Stores the parent index for each node. parent[i] = (i-1)/m.
    vector<int> lockedBy; // Stores the user ID (uid) that has locked a node. 0 means unlocked.
    vector<int> descLocked; // A counter for each node, storing how many of its descendants are currently locked. This is a key optimization.
    vector<SpinLock> nodeLock; // A spinlock for each node to manage concurrent access to its state.

    // Helper function to get the path from a node 'v' up to the root.
    // This is used to identify all ancestors that need to be checked or locked.
    vector<int> getPathToRoot(int v, bool includeSelf = true) {
        vector<int> path;
        if (includeSelf) path.push_back(v); // Optionally include the starting node itself.
        int p = parent[v];
        while (p != -1) { // -1 would be the parent of the root.
            path.push_back(p);
            p = parent[p]; // Move up to the next parent.
        }
        return path;
    }

    // Acquires spinlocks for a given set of nodes.
    // IMPORTANT: It sorts the node indices first to ensure a consistent locking order.
    // This prevents deadlocks (e.g., Thread 1 locks A then waits for B, while Thread 2 locks B and waits for A).
    void acquireSet(const vector<int>& nodes) {
        vector<int> a = nodes;
        sort(a.begin(), a.end()); // Establish a global locking order.
        a.erase(unique(a.begin(), a.end()), a.end()); // Remove duplicates.
        for (int u : a) nodeLock[u].lock(); // Lock each node in the sorted order.
    }

    // Releases the spinlocks for a given set of nodes. The order doesn't matter here.
    void releaseSet(const vector<int>& nodes) {
        for (int u : nodes) nodeLock[u].unlock();
    }

    // Finds all locked descendants of node 'v' for the upgrade operation.
    // It uses a stack for non-recursive tree traversal (to avoid stack overflow on deep trees).
    void getDescendants(int v, int uid, vector<int>& toUnlock, bool& foreignLockFound) {
        stack<int> st;
        st.push(v); // Start traversal from node 'v'.

        while (!st.empty()) {
            int u = st.top(); st.pop();
            // Calculate the index range of children for node 'u'.
            long long base = 1LL * u * m + 1;
            for (long long j = 0; j < m; ++j) {
                long long c = base + j;
                if (c >= n) break; // Stop if child index is out of bounds.
                int w = (int)c;

                // Check if this child is locked.
                if (lockedBy[w] != 0) {
                    if (lockedBy[w] != uid) {
                        foreignLockFound = true; // Found a descendant locked by a different user.
                        return; // Abort immediately, upgrade is not possible.
                    }
                    toUnlock.push_back(w); // This descendant needs to be unlocked during the upgrade.
                }

                // If a descendant has locked children of its own, we need to explore its subtree.
                if (descLocked[w] > 0) st.push(w);
            }
        }
    }

    // Constructor to initialize the TreeLocker.
    TreeLocker(int n_, int m_) : n(n_), m(m_), nodeLock(n_) {
        parent.assign(n, -1);
        lockedBy.assign(n, 0);
        descLocked.assign(n, 0);
        // Pre-calculate the parent for each node based on its index. Root (0) has no parent.
        for (int i = 1; i < n; ++i) parent[i] = (i - 1) / m;
    }

    // Helper to check if any ancestor of 'v' is locked.
    bool hasLockedAncestor(int v) {
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

    // Updates the `descLocked` count for all ancestors of 'v'.
    // 'delta' is +1 for locking and -1 for unlocking. This is O(log_m N).
    void addToAncestors(int v, int delta) {
        int p = parent[v];
        while (p != -1) {
            descLocked[p] += delta;
            p = parent[p];
        }
    }

    // Implements the lock operation.
    bool lockNode(int v, int uid) {
        // We need to lock the node itself and all its ancestors to check their state atomically.
        vector<int> need = getPathToRoot(v);
        acquireSet(need);

        // Conditions for locking to fail:
        // 1. Node 'v' is already locked.
        // 2. Any ancestor of 'v' is locked.
        // 3. Any descendant of 'v' is locked (checked via the descLocked counter).
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] != 0) {
            releaseSet(need); // Release locks before returning.
            return false;
        }

        // If all conditions pass, perform the lock.
        lockedBy[v] = uid;
        addToAncestors(v, 1); // Increment the locked descendant count for all ancestors.
        releaseSet(need); // Release the locks.
        return true;
    }

    // Implements the unlock operation.
    bool unlockNode(int v, int uid) {
        vector<int> need = getPathToRoot(v);
        acquireSet(need);

        // Condition for unlocking to fail:
        // 1. The node is not locked by the same user.
        if (lockedBy[v] != uid) {
            releaseSet(need);
            return false;
        }

        // Perform the unlock.
        lockedBy[v] = 0;
        addToAncestors(v, -1); // Decrement the locked descendant count for all ancestors.
        releaseSet(need);
        return true;
    }

    // Implements the upgrade lock operation. This is the most complex.
    bool upgradeNode(int v, int uid) {
        vector<int> path = getPathToRoot(v);
        acquireSet(path); // Initial lock on ancestors.

        // Conditions for upgrade to fail immediately:
        // 1. Node 'v' is already locked.
        // 2. An ancestor is locked.
        // 3. Node 'v' has no locked descendants to upgrade from.
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] == 0) {
            releaseSet(path);
            return false;
        }

        // Find all descendants to be unlocked and check for foreign locks.
        vector<int> toUnlock;
        bool foreignLockFound = false;
        getDescendants(v, uid, toUnlock, foreignLockFound);
        if (foreignLockFound) {
            releaseSet(path); // Found a lock by another user.
            return false;
        }

        // --- Critical Section: Re-locking ---
        // We must lock the ancestors AND the descendants we are about to modify.
        vector<int> allNodes = path;
        allNodes.insert(allNodes.end(), toUnlock.begin(), toUnlock.end());
        releaseSet(path); // Release the initial, smaller lock set.
        acquireSet(allNodes); // Acquire the comprehensive lock set.

        // Double-check conditions. The state could have changed in the tiny window
        // between releasing and acquiring locks. This is vital for correctness.
        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] == 0) {
            releaseSet(allNodes);
            return false;
        }
        // Re-run descendant check to ensure no new foreign locks appeared.
        toUnlock.clear();
        foreignLockFound = false;
        getDescendants(v, uid, toUnlock, foreignLockFound);
        if (foreignLockFound) { // This check should ideally not fail if logic is correct, but is a good safeguard.
             releaseSet(allNodes);
             return false;
        }


        // ---- Perform the atomic upgrade ----
        // 1. Unlock all descendants that were locked by this user.
        for (int u : toUnlock) {
            lockedBy[u] = 0;
            addToAncestors(u, -1);
        }

        // 2. Lock the target node 'v'.
        lockedBy[v] = uid;
        addToAncestors(v, 1);

        releaseSet(allNodes);
        return true;
    }
};

int main() {
    // Fast I/O
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, m, Q;
    if (!(cin >> N)) return 0; // Read number of nodes.
    cin >> m >> Q; // Read m-ary factor and number of queries.

    vector<string> names(N);
    unordered_map<string, int> id; // Map string names to integer indices for fast lookup.
    id.reserve(N * 2);
    for (int i = 0; i < N; ++i) {
        cin >> names[i];
        id[names[i]] = i;
    }

    TreeLocker tl(N, m); // Initialize the tree locker instance.

    // Process all queries.
    for (int i = 0; i < Q; ++i) {
        int op;
        string node;
        long long uid_long;
        cin >> op >> node >> uid_long;

        int v = id[node]; // Get node index from its name.
        int uid = (int)uid_long;
        bool res = false;

        if (op == 1) res = tl.lockNode(v, uid);
        else if (op == 2) res = tl.unlockNode(v, uid);
        else if (op == 3) res = tl.upgradeNode(v, uid);

        cout << (res ? "true" : "false") << "\n";
    }

    return 0;
}












///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////















#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <algorithm>

using namespace std;

// Simple spinlock using volatile int
struct SpinLock {
    volatile int locked;
    SpinLock() : locked(0) {}

    void lock() {
        while (true) {
            if (locked == 0) {
                locked = 1;
                return;
            }
        }
    }

    void unlock() {
        locked = 0;
    }
};

struct TreeLocker {
    int n, m;
    vector<int> parent;
    vector<int> lockedBy;
    vector<int> descLocked;
    vector<SpinLock> nodeLock; // renamed to match original code


    vector<int> getPathToRoot(int v, bool includeSelf = true) { // renamed
        vector<int> path;
        if (includeSelf) path.push_back(v);
        int p = parent[v];
        while (p != -1) {
            path.push_back(p);
            p = parent[p];
        }
        return path;
    }

    void acquireSet(const vector<int>& nodes) {
        vector<int> a = nodes;
        sort(a.begin(), a.end());
        a.erase(unique(a.begin(), a.end()), a.end());
        for (int u : a) nodeLock[u].lock();
    }

    void releaseSet(const vector<int>& nodes) {
        for (int u : nodes) nodeLock[u].unlock();
    }

    void getDescendants(int v, int uid, vector<int>& toUnlock, bool& foreignLockFound) { // renamed
        stack<int> st;
        st.push(v);

        while (!st.empty()) {
            int u = st.top(); st.pop();
            long long base = 1LL * u * m + 1;
            for (long long j = 0; j < m; ++j) {
                long long c = base + j;
                if (c >= n) break;
                int w = (int)c;

                if (lockedBy[w] != 0) {
                    if (lockedBy[w] != uid) {
                        foreignLockFound = true;
                        return;
                    }
                    toUnlock.push_back(w);
                }

                if (descLocked[w] > 0) st.push(w);
            }
        }
    }

    TreeLocker(int n_, int m_) : n(n_), m(m_), nodeLock(n_) {
        parent.assign(n, -1);
        lockedBy.assign(n, 0);
        descLocked.assign(n, 0);
        for (int i = 1; i < n; ++i) parent[i] = (i - 1) / m;
    }

    bool hasLockedAncestor(int v) { // renamed
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

    void addToAncestors(int v, int delta) { // renamed
        int p = parent[v];
        while (p != -1) {
            descLocked[p] += delta;
            p = parent[p];
        }
    }

    bool lockNode(int v, int uid) {
        vector<int> need = getPathToRoot(v);
        acquireSet(need);

        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] != 0) {
            releaseSet(need);
            return false;
        }

        lockedBy[v] = uid;
        addToAncestors(v, 1);
        releaseSet(need);
        return true;
    }

    bool unlockNode(int v, int uid) {
        vector<int> need = getPathToRoot(v);
        acquireSet(need);

        if (lockedBy[v] != uid) {
            releaseSet(need);
            return false;
        }

        lockedBy[v] = 0;
        addToAncestors(v, -1);
        releaseSet(need);
        return true;
    }

    bool upgradeNode(int v, int uid) {
        vector<int> path = getPathToRoot(v);
        acquireSet(path);

        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] == 0) {
            releaseSet(path);
            return false;
        }

        vector<int> toUnlock;
        bool foreignLockFound = false;
        getDescendants(v, uid, toUnlock, foreignLockFound);
        if (foreignLockFound) {
            releaseSet(path);
            return false;
        }

        vector<int> allNodes = path;
        allNodes.insert(allNodes.end(), toUnlock.begin(), toUnlock.end());
        releaseSet(path);
        acquireSet(allNodes);

        if (lockedBy[v] != 0 || hasLockedAncestor(v) || descLocked[v] == 0) {
            releaseSet(allNodes);
            return false;
        }

        toUnlock.clear();
        foreignLockFound = false;
        getDescendants(v, uid, toUnlock, foreignLockFound);

        for (int u : toUnlock) {
            lockedBy[u] = 0;
            addToAncestors(u, -1);
        }

        lockedBy[v] = uid;
        addToAncestors(v, 1);

        releaseSet(allNodes);
        return true;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, m, Q;
    if (!(cin >> N)) return 0;
    cin >> m >> Q;

    vector<string> names(N);
    unordered_map<string, int> id;
    id.reserve(N * 2);
    for (int i = 0; i < N; ++i) {
        cin >> names[i];
        id[names[i]] = i;
    }

    TreeLocker tl(N, m);

    for (int i = 0; i < Q; ++i) {
        int op;
        string node;
        long long uid_long;
        cin >> op >> node >> uid_long;

        int v = id[node];
        int uid = (int)uid_long;
        bool res = false;

        if (op == 1) res = tl.lockNode(v, uid);
        else if (op == 2) res = tl.unlockNode(v, uid);
        else if (op == 3) res = tl.upgradeNode(v, uid);

        cout << (res ? "true" : "false") << "\n";
    }

    return 0;
}
