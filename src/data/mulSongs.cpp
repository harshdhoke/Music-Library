#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <thread> // Required for multithreading
#include <mutex>  // Required for thread safety

using namespace std;

// A mutex to protect cout for synchronized output
mutex cout_mutex;

struct TreeLocker {
    int n, m;
    vector<int> parent;
    vector<int> lockedBy;
    vector<int> descLocked;
    // Mutex to make the TreeLocker thread-safe
    mutex mtx;

    TreeLocker(int n_, int m_) : n(n_), m(m_) {
        parent.assign(n, -1);
        lockedBy.assign(n, 0);
        descLocked.assign(n, 0);
        for (int i = 1; i < n; ++i) parent[i] = (i - 1) / m;
    }

    // Helper function to check for locked ancestors.
    // This function is called from within locked methods, so it doesn't need its own lock.
    bool hasLockedAncestor(int v) {
        int p = parent[v];
        while (p != -1) {
            if (lockedBy[p] != 0) return true;
            p = parent[p];
        }
        return false;
    }

    // Helper function to update ancestor descendant lock counts.
    // This is also called from within locked methods.
    void addToAncestors(int v, int delta) {
        int p = parent[v];
        while (p != -1) {
            descLocked[p] += delta;
            p = parent[p];
        }
    }

    // Locks a node if possible.
    // This is a critical section and needs to be protected by a lock.
    bool lockNode(int v, int uid) {
        // lock_guard ensures the mutex is unlocked when the function returns
        lock_guard<mutex> lock(mtx);
        if (lockedBy[v] != 0) return false;
        if (hasLockedAncestor(v)) return false;
        if (descLocked[v] != 0) return false;
        lockedBy[v] = uid;
        addToAncestors(v, 1);
        return true;
    }

    // Unlocks a node if it was locked by the same user.
    // This is a critical section.
    bool unlockNode(int v, int uid) {
        lock_guard<mutex> lock(mtx);
        if (lockedBy[v] != uid) return false;
        lockedBy[v] = 0;
        addToAncestors(v, -1);
        return true;
    }

    // Upgrades a lock on a node.
    // This is the most complex critical section.
    bool upgradeNode(int v, int uid) {
        lock_guard<mutex> lock(mtx);
        if (lockedBy[v] != 0) return false;
        if (hasLockedAncestor(v)) return false;
        if (descLocked[v] == 0) return false;

        vector<int> toUnlock;
        stack<int> st;
        st.push(v);
        bool ok = true;

        // Check if all locked descendants are locked by the current user
        while (!st.empty() && ok) {
            int u = st.top(); st.pop();
            long long base = 1LL * u * m + 1;
            for (long long j = 0; j < m; ++j) {
                long long c = base + j;
                if (c >= n) break;
                int w = (int)c;
                if (lockedBy[w] != 0) {
                    if (lockedBy[w] != uid) {
                        ok = false;
                        break;
                    }
                    toUnlock.push_back(w);
                } else if (descLocked[w] > 0) {
                    st.push(w);
                }
            }
        }
        if (!ok) return false;

        // If the check passes, perform the upgrade
        for (int u : toUnlock) {
            // The lockedBy[u] is already checked to be equal to uid
            lockedBy[u] = 0;
            addToAncestors(u, -1);
        }
        lockedBy[v] = uid;
        addToAncestors(v, 1);
        return true;
    }
};

// Function to be executed by each thread
void process_query(TreeLocker& tl, int op, int v, int uid, vector<bool>& results, int query_index) {
    bool res = false;
    if (op == 1) res = tl.lockNode(v, uid);
    else if (op == 2) res = tl.unlockNode(v, uid);
    else if (op == 3) res = tl.upgradeNode(v, uid);
    results[query_index] = res;
}

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

    vector<thread> threads;
    vector<bool> results(Q);

    // Create a thread for each query
    for (int i = 0; i < Q; ++i) {
        int op;
        string node;
        long long uid;
        cin >> op >> node >> uid;
        int v = id[node];
        // Launch a new thread to process the query
        threads.emplace_back(process_query, ref(tl), op, v, (int)uid, ref(results), i);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Print results in order
    for (int i = 0; i < Q; ++i) {
        cout << (results[i] ? "true" : "false") << "\n";
    }

    return 0;
}
