#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <stack>
#include <algorithm>
#include <memory>

using namespace std;

// This struct encapsulates all the logic for the m-ary tree locking system.
struct TreeLocker {
    // n: total number of nodes, m: number of children per node (m-ary).
    int n, m; 
    
    // parent[i] stores the index of the parent of node i.
    vector<int> parent; 
    
    // lockedBy[i] stores the user ID (uid) who has locked node i. 0 means unlocked.
    vector<int> lockedBy; 
    
    // An optimization: descLocked[i] stores the count of locked nodes in the subtree of node i.
    // This avoids traversing the entire subtree to check for locked descendants.
    vector<int> descLocked; 
    
    // A mutex for each node to ensure thread safety. Operations on a node or its state
    // (like its ancestors' descLocked count) must acquire the corresponding mutex.
    vector<mutex> nodeMx; 

    // Helper function to get the path from a given node 'v' up to the root.
    // This is used to identify all nodes whose state might be affected by an operation,
    // so we can lock their mutexes.
    vector<int> getPathToRoot(int v, bool includeSelf = true) {
        vector<int> path;
        if (includeSelf) path.push_back(v);
        int p = parent[v];
        while (p != -1) {
            path.push_back(p);
            p = parent[p];
        }
        return path;
    }

    // Acquires locks for a given list of nodes in a deadlock-free manner.
    // It sorts the node IDs to ensure a consistent lock acquisition order, preventing circular waits.
    // 'unique_lock' is used for RAII-style locking, ensuring mutexes are automatically released.
    void acquireLocks(const vector<int>& nodes, vector<unique_lock<mutex>>& locks) {
        vector<int> sortedNodes = nodes;
        sort(sortedNodes.begin(), sortedNodes.end());
        // Remove duplicates as we only need to lock each node's mutex once.
        sortedNodes.erase(unique(sortedNodes.begin(), sortedNodes.end()), sortedNodes.end());
        locks.reserve(sortedNodes.size());
        for (int id : sortedNodes) {
            locks.emplace_back(nodeMx[id]); // Lock the mutex for each node.
        }
    }

    // Helper for upgradeLock. Traverses the subtree of 'v' to check for locked descendants.
    // It populates 'candidates' with descendants locked by 'uid' and sets 'foreignLockFound'
    // to true if a descendant is locked by a different user.
    void collectLockedDescendants(int v, int uid, vector<int>& candidates, bool& foreignLockFound) {
        stack<int> st; // Using a stack for iterative DFS traversal.
        st.push(v);

        while (!st.empty()) {
            int u = st.top();
            st.pop();

            // Calculate the index range for children of node 'u'.
            long long base = 1LL * u * m + 1;
            for (long long j = 0; j < m; ++j) {
                long long c = base + j;
                if (c >= n) break; // Stop if child index is out of bounds.
                
                int w = (int)c;

                // Check if the child node 'w' is locked.
                if (lockedBy[w] != 0) {
                    if (lockedBy[w] != uid) {
                        foreignLockFound = true; // Found a lock by another user.
                        return; // Abort immediately.
                    }
                    candidates.push_back(w); // It's locked by the same user, add to list.
                }

                // Optimization: Only traverse deeper if this child has locked descendants.
                if (descLocked[w] > 0) {
                    st.push(w);
                }
            }
        }
    }

    // Constructor to initialize the TreeLocker.
    TreeLocker(int n_, int m_) : n(n_), m(m_), nodeMx(n_) {
        parent.assign(n, -1);      // Root has no parent (-1).
        lockedBy.assign(n, 0);     // All nodes are initially unlocked.
        descLocked.assign(n, 0);   // No locked descendants initially.
        // Pre-calculate parent for each node based on its index in the m-ary tree.
        for (int i = 1; i < n; ++i) parent[i] = (i - 1) / m;
    }

    // Checks if any ancestor of node 'v' is locked.
    bool hasLockedAncestor(int v) {
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

    // Updates the 'descLocked' count for all ancestors of 'v' by a 'delta' (+1 for lock, -1 for unlock).
    void addToAncestors(int v, int delta) {
        int p = parent[v];
        while (p != -1) {
            descLocked[p] += delta;
            p = parent[p];
        }
    }

    // Attempts to lock node 'v' for user 'uid'.
    bool lockNode(int v, int uid) {
        // Identify and lock all mutexes for the node and its ancestors to ensure atomicity.
        vector<int> need = getPathToRoot(v, true);
        vector<unique_lock<mutex>> locks;
        acquireLocks(need, locks);

        // A lock can only be acquired if:
        // 1. The node itself is not already locked.
        if (lockedBy[v] != 0) return false;
        // 2. No ancestor is locked.
        if (hasLockedAncestor(v)) return false;
        // 3. No descendant is locked.
        if (descLocked[v] != 0) return false;

        // If all conditions pass, perform the lock.
        lockedBy[v] = uid;
        addToAncestors(v, 1); // Increment locked descendant count for all ancestors.
        return true;
    }

    // Attempts to unlock node 'v', which must have been locked by the same 'uid'.
    bool unlockNode(int v, int uid) {
        vector<int> need = getPathToRoot(v, true);
        vector<unique_lock<mutex>> locks;
        acquireLocks(need, locks);

        // An unlock can only happen if the node is currently locked by the same user.
        if (lockedBy[v] != uid) return false;

        // Perform the unlock.
        lockedBy[v] = 0;
        addToAncestors(v, -1); // Decrement locked descendant count for all ancestors.
        return true;
    }
    
    // Attempts to upgrade a lock to an ancestor node 'v' for user 'uid'.
    bool upgradeNode(int v, int uid) {
        // --- First phase: Initial checks with minimal locking ---
        vector<int> basePath = getPathToRoot(v, true);
        vector<unique_lock<mutex>> locks;
        acquireLocks(basePath, locks);

        // An upgrade can only happen if:
        // 1. The target node 'v' is not already locked.
        // 2. It has at least one locked descendant.
        // 3. None of its ancestors are locked.
        if (lockedBy[v] != 0 || descLocked[v] == 0 || hasLockedAncestor(v)) {
            return false;
        }

        // --- Second phase: Verify descendant locks ---
        // Find all descendants locked by this user and check for any locks by other users.
        vector<int> lockedDescendants;
        bool foreignLockFound = false;
        collectLockedDescendants(v, uid, lockedDescendants, foreignLockFound);

        if (foreignLockFound) {
            return false; // Fail if a descendant is locked by another user.
        }

        // --- Third phase: Re-lock and perform atomic update ---
        // The set of nodes to be modified includes the ancestors AND the descendants to be unlocked.
        // We must release the old locks and acquire all necessary locks at once.
        locks.clear(); 
        
        vector<int> allNodesToLock = basePath;
        allNodesToLock.insert(allNodesToLock.end(), lockedDescendants.begin(), lockedDescendants.end());

        acquireLocks(allNodesToLock, locks);

        // IMPORTANT: Re-check conditions after re-acquiring locks. Another thread could have
        // changed the state while we were not holding the locks.
        if (lockedBy[v] != 0 || descLocked[v] == 0 || hasLockedAncestor(v)) {
            return false;
        }
        
        // Re-run the descendant check to ensure consistency.
        foreignLockFound = false;
        vector<int> currentLockedDescendants;
        collectLockedDescendants(v, uid, currentLockedDescendants, foreignLockFound);

        if (foreignLockFound) {
            return false;
        }
        
        // Atomically unlock all found descendants.
        for (int u : currentLockedDescendants) {
            if (lockedBy[u] == uid) { // Should always be true based on checks.
                lockedBy[u] = 0;
                addToAncestors(u, -1);
            }
        }

        // Atomically lock the target ancestor node.
        lockedBy[v] = uid;
        addToAncestors(v, 1);

        return true;
    }
};

int main() {
    // Fast I/O
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int N, m, Q;
    // Read tree structure and query count.
    if (!(cin >> N)) return 0;
    cin >> m >> Q;

    // Map string node names to integer IDs for efficient array access.
    vector<string> names(N);
    unordered_map<string, int> id;
    id.reserve(N * 2); // Pre-allocate memory for performance.
    for (int i = 0; i < N; ++i) {
        cin >> names[i];
        id[names[i]] = i;
    }

    // Create the TreeLocker instance.
    TreeLocker tl(N, m);

    // Process all Q queries.
    for (int i = 0; i < Q; ++i) {
        int op;
        string node;
        long long uid_long;
        cin >> op >> node >> uid_long;
        
        int v = id[node]; // Get node ID from its name.
        int uid = (int)uid_long;
        bool ok = false;

        // Call the appropriate function based on the operation type.
        if (op == 1) ok = tl.lockNode(v, uid);
        else if (op == 2) ok = tl.unlockNode(v, uid);
        else if (op == 3) ok = tl.upgradeNode(v, uid);
        
        cout << (ok ? "true" : "false") << "\n";
    }
    
    return 0;
}













?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????







#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <stack>
#include <algorithm>
#include <memory>

using namespace std;

struct TreeLocker {
    int n, m;
    vector<int> parent;
    vector<int> lockedBy;
    vector<int> descLocked;
    vector<mutex> nodeMx;

    vector<int> getPathToRoot(int v, bool includeSelf = true) {
        vector<int> path;
        if (includeSelf) path.push_back(v);
        int p = parent[v];
        while (p != -1) {
            path.push_back(p);
            p = parent[p];
        }
        return path;
    }

    void acquireLocks(const vector<int>& nodes, vector<unique_lock<mutex>>& locks) {
        vector<int> sortedNodes = nodes;
        sort(sortedNodes.begin(), sortedNodes.end());
        sortedNodes.erase(unique(sortedNodes.begin(), sortedNodes.end()), sortedNodes.end());
        locks.reserve(sortedNodes.size());
        for (int id : sortedNodes) {
            locks.emplace_back(nodeMx[id]);
        }
    }

    void collectLockedDescendants(int v, int uid, vector<int>& candidates, bool& foreignLockFound) {
        stack<int> st;
        st.push(v);

        while (!st.empty()) {
            int u = st.top();
            st.pop();

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
                    candidates.push_back(w);
                }

                if (descLocked[w] > 0) {
                    st.push(w);
                }
            }
        }
    }

    TreeLocker(int n_, int m_) : n(n_), m(m_), nodeMx(n_) {
        parent.assign(n, -1);
        lockedBy.assign(n, 0);
        descLocked.assign(n, 0);
        for (int i = 1; i < n; ++i) parent[i] = (i - 1) / m;
    }


    bool hasLockedAncestor(int v) {
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

    void addToAncestors(int v, int delta) {
        int p = parent[v];
        while (p != -1) {
            descLocked[p] += delta;
            p = parent[p];
        }
    }

    bool lockNode(int v, int uid) {
        vector<int> need = getPathToRoot(v, true);
        vector<unique_lock<mutex>> locks;
        acquireLocks(need, locks);

        if (lockedBy[v] != 0) return false;
        if (hasLockedAncestor(v)) return false;
        if (descLocked[v] != 0) return false;

        lockedBy[v] = uid;
        addToAncestors(v, 1);
        return true;
    }

    bool unlockNode(int v, int uid) {
        vector<int> need = getPathToRoot(v, true);
        vector<unique_lock<mutex>> locks;
        acquireLocks(need, locks);

        if (lockedBy[v] != uid) return false;

        lockedBy[v] = 0;
        addToAncestors(v, -1);
        return true;
    }
    

    bool upgradeNode(int v, int uid) {
        vector<int> basePath = getPathToRoot(v, true);
        vector<unique_lock<mutex>> locks;
        acquireLocks(basePath, locks);

        if (lockedBy[v] != 0 || descLocked[v] == 0 || hasLockedAncestor(v)) {
            return false;
        }

        vector<int> lockedDescendants;
        bool foreignLockFound = false;
        collectLockedDescendants(v, uid, lockedDescendants, foreignLockFound);

        if (foreignLockFound) {
            return false;
        }

        locks.clear(); 
        
        vector<int> allNodesToLock = basePath;
        allNodesToLock.insert(allNodesToLock.end(), lockedDescendants.begin(), lockedDescendants.end());

        acquireLocks(allNodesToLock, locks);

        if (lockedBy[v] != 0 || descLocked[v] == 0 || hasLockedAncestor(v)) {
            return false;
        }
        
        foreignLockFound = false;
        vector<int> currentLockedDescendants;
        collectLockedDescendants(v, uid, currentLockedDescendants, foreignLockFound);

        if (foreignLockFound) {
            return false;
        }
        
        for (int u : currentLockedDescendants) {
            if (lockedBy[u] == uid) {
                lockedBy[u] = 0;
                addToAncestors(u, -1);
            }
        }

        lockedBy[v] = uid;
        addToAncestors(v, 1);

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
        bool ok = false;

        if (op == 1) ok = tl.lockNode(v, uid);
        else if (op == 2) ok = tl.unlockNode(v, uid);
        else if (op == 3) ok = tl.upgradeNode(v, uid);
        
        cout << (ok ? "true" : "false") << "\n";
    }
    
    return 0;
}
