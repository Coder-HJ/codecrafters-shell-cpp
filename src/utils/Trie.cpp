#include <iostream>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

class Trie {
    struct Node {
        unordered_map<char, Node*> children;
        bool isEnd = false;
    };
    Node* root;

    // Collects all words in the trie that start with the given prefix.
    void collect(Node* node, string prefix, vector<string>& result) {
        if (node->isEnd) result.push_back(prefix);
        for (auto& [ch, child] : node->children) {
            collect(child, prefix + ch, result);
        }
    }

public:
    // Constructs an empty Trie.
    Trie() : root(new Node()) {}

    // Inserts all strings from the input vector into the trie.
    void add(const vector<string>& words) {
        for (const string& word : words) {
            Node* node = root;
            for (char ch : word) {
                if (!node->children.count(ch))
                    node->children[ch] = new Node();
                node = node->children[ch];
            }
            node->isEnd = true;
        }
    }

    // Returns true if the given word exists in the trie.
    bool find(const string& word) {
        Node* node = root;
        for (char ch : word) {
            if (!node->children.count(ch)) return false;
            node = node->children[ch];
        }
        return node->isEnd;
    }

    // Returns all words in the trie that start with the given prefix.
    vector<string> getAllByPrefix(const string& prefix) {
        Node* node = root;
        for (char ch : prefix) {
            if (!node->children.count(ch)) return {};
            node = node->children[ch];
        }
        vector<string> result;
        collect(node, prefix, result);
        return result;
    }

    // Empties the entire trie by deleting all nodes and resetting the root.
    void clear() {
        function<void(Node*)> cleanup = [&](Node* node) {
            for (auto& [_, child] : node->children) cleanup(child);
            delete node;
        };
        cleanup(root);
        root = new Node();
    }


    // Destructor: Recursively deletes all nodes in the trie.
    ~Trie() {
        function<void(Node*)> cleanup = [&](Node* node) {
            for (auto& [_, child] : node->children) cleanup(child);
            delete node;
        };
        cleanup(root);
    }
};
