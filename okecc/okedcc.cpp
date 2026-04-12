#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <cstdio>
#include <iostream>

#ifndef IDX_NONE
#define IDX_NONE 0xFFFFFFFFu
#endif

#ifndef IDX_EXIT
#define IDX_EXIT 0xFFFFFFFEu
#endif

#ifndef IDX_SYN_EXIT
#define IDX_SYN_EXIT 0xFFFFFFFDu
#endif

// ------------------------------------------------------------
// 1. CFG Node
// ------------------------------------------------------------
struct CFGNode {
    UINT id;
    UINT nextG = IDX_NONE;
    UINT nextR = IDX_NONE;
    std::vector<UINT> succ;
    std::vector<UINT> pred;
    bool isExit = false;
};

// ------------------------------------------------------------
// 2. CFG Builder
// ------------------------------------------------------------
class CFG {
public:
    std::map<UINT, CFGNode> nodes;
    UINT start = IDX_NONE;
    UINT syntheticExit = IDX_NONE;

    void build(CChipPool& pool) {
        start = pool.m_start;

        // -----------------------------
        // 1. ノード作成
        // -----------------------------
        for (UINT i = 0; i < pool.size(); ++i) {
            CChip* chip = pool[i];
            if (!chip) continue;

            CFGNode n;
            n.id = i;
            nodes[i] = n;
        }

        // -----------------------------
        // 2. syntheticExit ノード作成
        // -----------------------------
        syntheticExit = IDX_SYN_EXIT;
        CFGNode ex;
        ex.id = syntheticExit;
        ex.isExit = true;
        nodes[syntheticExit] = ex;

        // -----------------------------
        // 3. nextG / nextR を正規化
        //    EXIT → syntheticExit に統一
        // -----------------------------
        for (auto& kv : nodes) {
            UINT i = kv.first;
            if (i == syntheticExit) continue;

            CChip* chip = (i < pool.size()) ? pool[i] : nullptr;
            if (!chip) continue;

            UINT g = chip->m_NextG;
            UINT r = chip->m_NextR;

            // EXIT → syntheticExit
            if (g == IDX_EXIT) g = syntheticExit;
            if (r == IDX_EXIT) r = syntheticExit;

            nodes[i].nextG = g;
            nodes[i].nextR = r;
        }

        // -----------------------------
        // 4. succ/pred の構築
        // -----------------------------
        for (auto& kv : nodes) {
            UINT i = kv.first;
            if (i == syntheticExit) continue;

            UINT g = nodes[i].nextG;
            UINT r = nodes[i].nextR;

            auto add_edge = [&](UINT nxt) {
                if (nxt == IDX_NONE) return;
                if (nodes.find(nxt) == nodes.end()) return;
                nodes[i].succ.push_back(nxt);
                nodes[nxt].pred.push_back(i);
                };

            add_edge(g);
            add_edge(r);
        }
    }
};

// ------------------------------------------------------------
// 3. Dominator / PostDominator
// ------------------------------------------------------------
class Dominator {
public:
    CFG* cfg;
    std::map<UINT, std::set<UINT>> dom;
    std::map<UINT, std::set<UINT>> pdom;
    std::map<UINT, UINT> idom;   // immediate dominator

    Dominator(CFG* c) : cfg(c) {}

    void computeDom() {
        std::set<UINT> all;
        for (auto& kv : cfg->nodes) all.insert(kv.first);

        for (auto& kv : cfg->nodes) {
            UINT n = kv.first;
            if (n == cfg->start) dom[n] = { n };
            else dom[n] = all;
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& kv : cfg->nodes) {
                UINT n = kv.first;
                if (n == cfg->start) continue;

                auto& preds = cfg->nodes[n].pred;
                if (preds.empty()) continue;

                std::set<UINT> newDom = dom[preds[0]];
                for (size_t i = 1; i < preds.size(); ++i) {
                    std::set<UINT> tmp;
                    for (UINT x : newDom)
                        if (dom[preds[i]].count(x)) tmp.insert(x);
                    newDom.swap(tmp);
                }
                newDom.insert(n);

                if (newDom != dom[n]) {
                    dom[n] = newDom;
                    changed = true;
                }
            }
        }
    }

    void computeIDom() {
        for (auto& kv : dom) {
            UINT n = kv.first;
            auto& S = kv.second;

            std::set<UINT> candidates;
            for (UINT x : S) if (x != n) candidates.insert(x);

            UINT best = IDX_NONE;
            for (UINT x : candidates) {
                bool ok = true;
                for (UINT y : candidates) {
                    if (y == x) continue;
                    if (dom[y].count(x)) {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    best = x;
                    break;
                }
            }

            idom[n] = best;
        }
    }

    void computePostDom() {
        std::set<UINT> all;
        for (auto& kv : cfg->nodes) all.insert(kv.first);

        for (auto& kv : cfg->nodes) {
            UINT n = kv.first;
            if (n == cfg->syntheticExit) pdom[n] = { n };
            else pdom[n] = all;
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& kv : cfg->nodes) {
                UINT n = kv.first;
                if (n == cfg->syntheticExit) continue;

                auto& succ = cfg->nodes[n].succ;
                if (succ.empty()) continue;

                std::set<UINT> newPD = pdom[succ[0]];
                for (size_t i = 1; i < succ.size(); ++i) {
                    std::set<UINT> tmp;
                    for (UINT x : newPD)
                        if (pdom[succ[i]].count(x)) tmp.insert(x);
                    newPD.swap(tmp);
                }
                newPD.insert(n);

                if (newPD != pdom[n]) {
                    pdom[n] = newPD;
                    changed = true;
                }
            }
        }
    }

    void dump() {
        auto dump_dom = [&](std::map<UINT, std::set<UINT>>& domMap) {
            for (const auto& [key, value] : domMap) {
                printf("%3d: ", key);
                for (const auto& elem : value) {
                    printf("%3d ", elem);
                }
                printf("\n");
            }
            };

        printf("=== dominator ===\n");
        dump_dom(dom);

        printf("=== post dominator ===\n");
        dump_dom(pdom);
    }
};

// ------------------------------------------------------------
// 4. AST Node
// ------------------------------------------------------------
struct Node {
    enum Kind {
        K_Block,
        K_If,
        K_Seq,
        K_Exit,
        K_While   // ★ 追加
    };

    Kind kind;

    UINT id = IDX_NONE;

    // If
    std::string cond;
    Node* thenPart = nullptr;
    Node* elsePart = nullptr;

    // Seq
    std::vector<Node*> seq;

    // While
    Node* body = nullptr;

    static Node* Block(UINT id) {
        Node* n = new Node();
        n->kind = K_Block;
        n->id = id;
        return n;
    }

    static Node* If(const std::string& cond, Node* t, Node* e) {
        Node* n = new Node();
        n->kind = K_If;
        n->cond = cond;
        n->thenPart = t;
        n->elsePart = e;
        return n;
    }

    static Node* Seq(const std::vector<Node*>& s) {
        Node* n = new Node();
        n->kind = K_Seq;
        n->seq = s;
        return n;
    }

    static Node* Exit() {
        Node* n = new Node();
        n->kind = K_Exit;
        return n;
    }

    static Node* While(const std::string& cond, Node* body) {
        Node* n = new Node();
        n->kind = K_While;
        n->cond = cond;
        n->body = body;
        return n;
    }
};

// ------------------------------------------------------------
// 5. Decompiler
// ------------------------------------------------------------
class Decompiler {
public:
    CChipPool& pool;
    CFG        cfg;
    Dominator  dom;
    std::set<UINT> used;

    Decompiler(CChipPool& p)
        : pool(p), dom(&cfg) {
    }

    // ループヘッダ判定（back-edge: pred → n で n が pred を支配）
    bool isLoopHeader(UINT n) {
        auto& node = cfg.nodes[n];

        for (UINT succ : node.pred) {
            // succ が n を支配していれば n→succ は back-edge
            if (dom.dom[succ].count(n)) {
                // ★ back-edge の source がループヘッダ
                printf("LoopHdr(%d): true -> %d\n", n, succ);
                return true;
            }
        }
        printf("LoopHdr(%d): false\n", n);
        return false;
    }

    // ループ構築
    Node* buildLoop(UINT header) {
        std::string cond = "1";

        auto& node = cfg.nodes[header];

        // ループ本体開始ノード（back-edge でない側）
        UINT bodyStart = IDX_NONE;
        if (node.nextG != IDX_NONE && node.nextG != cfg.syntheticExit)
            bodyStart = node.nextG;
        else if (node.nextR != IDX_NONE && node.nextR != cfg.syntheticExit)
            bodyStart = node.nextR;

        used.insert(header);

        // ★ header に戻ったら停止
        Node* body = extractUntilJoin(bodyStart, header);

        return Node::While(cond, body);
    }

    // ----------------- CFG 構築 -----------------
    void buildCFG() {
        cfg.build(pool);
        dom.computeDom();
        dom.computeIDom();
        dom.computePostDom();   // 今は join には使わないが、他用途のため残す
        //dom.dump();
    }

    // ----------------- 条件ノード判定 -----------------
    bool isCondNode(UINT id) const {
        auto it = cfg.nodes.find(id);
        if (it == cfg.nodes.end()) return false;
        const CFGNode& node = it->second;
        // 条件チップは必ず R 側出口を持つ
        return (node.nextR != IDX_NONE);
    }

    // ----------------- 到達集合 -----------------
    void dfsReach(UINT n, std::set<UINT>& out) {
        if (n == IDX_NONE) return;
        if (out.count(n)) return;
        out.insert(n);

        if (n == cfg.syntheticExit) return;

        auto it = cfg.nodes.find(n);
        if (it == cfg.nodes.end()) return;
        const CFGNode& node = it->second;

        UINT g = node.nextG;
        UINT r = node.nextR;

        if (g != IDX_NONE) dfsReach(g, out);
        if (r != IDX_NONE) dfsReach(r, out);
    }

    // ----------------- join 探索（postdom を使わない） -----------------
    UINT findPostJoin(UINT n) {
        auto it = cfg.nodes.find(n);
        if (it == cfg.nodes.end()) return IDX_NONE;
        const CFGNode& node = it->second;

        UINT g = node.nextG;
        UINT r = node.nextR;

        if (g == IDX_NONE || r == IDX_NONE) return IDX_NONE;

        // 到達集合
        std::set<UINT> reachG, reachR;
        dfsReach(g, reachG);
        dfsReach(r, reachR);

        // 共通到達点
        std::set<UINT> cand;
        for (UINT x : reachG) {
            if (reachR.count(x)) cand.insert(x);
        }

        // syntheticExit は join にしない
        cand.erase(cfg.syntheticExit);

        if (cand.empty()) return IDX_NONE;

        // dominator を使って「一番手前」を選ぶ：
        // 他の候補をすべて支配する（またはより多く支配する）ものを選ぶ
        UINT best = *cand.begin();
        for (UINT x : cand) {
            if (x == best) continue;
            // x が best を支配しているなら x の方が手前
            if (!dom.dom[x].count(best)) {
                best = x;
            }
        }
        return best;
    }

    // ----------------- then/else を join まで切り出す -----------------
    Node* extractUntilJoin(UINT start, UINT join) {
        std::vector<Node*> seq;
        UINT cur = start;

        auto isValid = [&](UINT x) {
            return (x != IDX_NONE && x != cfg.syntheticExit);
            };

        while (true) {
            if (cur == cfg.syntheticExit) break;
            if (cur == join) break;
            if (used.count(cur)) break;
            
            // ★ ループヘッダなら while を構築して終わり
            if (isLoopHeader(cur)) {
                Node* loop = buildLoop(cur);
                seq.push_back(loop);
                break;
            }

            used.insert(cur);

            auto it = cfg.nodes.find(cur);
            if (it == cfg.nodes.end()) break;
            const CFGNode& node = it->second;

            UINT g = node.nextG;
            UINT r = node.nextR;

            bool hasG = isValid(g);
            bool hasR = isValid(r);
            bool cond = isCondNode(cur);

            // 条件ノードなら if にする（R＝true, G＝false）
            if (cond && (hasG || hasR)) {
                UINT falseBranch = hasG ? g : cfg.syntheticExit;
                UINT trueBranch = hasR ? r : cfg.syntheticExit;
                Node* subIf = buildIf(cur, falseBranch, trueBranch);
                seq.push_back(subIf);
                break;
            }

            // 非条件ノードで 2 分岐 → if（R＝true, G＝false）
            if (hasG && hasR) {
                Node* subIf = buildIf(cur, g, r);
                seq.push_back(subIf);
                break;
            }

            // Block を追加
            seq.push_back(Node::Block(cur));

            // 0 分岐（終端）
            if (!hasG && !hasR) break;

            // 1 分岐（直列）
            UINT next = hasG ? g : r;

            // join に到達したら終了
            if (next == join) break;

            // dominator を使って直列判定
            if (dom.idom[next] != cur) break;

            cur = next;
        }

        if (seq.empty()) return nullptr;
        if (seq.size() == 1) return seq[0];
        return Node::Seq(seq);
    }

    // ----------------- if ノード構築 -----------------
    // g = false 分岐（G 側）、r = true 分岐（R 側）
    Node* buildIf(UINT n, UINT g, UINT r) {
        CChip* condChip = pool[n];
        std::string cond = condChip ? condChip->GetDslText() : "";

        UINT join = findPostJoin(n);
        //printf("BuildIf(%d) r%d g%d j%d\n", n, r, g, join);

        // ★ この if が初めて join を used にマークするかどうか
        bool markedJoinHere = false;
        if (join != IDX_NONE && join != cfg.syntheticExit) {
            if (!used.count(join)) {
                used.insert(join);
                markedJoinHere = true;
            }
        }

        // ① まず R 側（then）だけ構造化
        Node* thenPart = (r != cfg.syntheticExit) ? extractUntilJoin(r, join) : nullptr;

        // ★ 外側 if だけが join を解放する
        if (markedJoinHere) {
            used.erase(join);
        }

        // ② G 側（else 候補）を構造化
        Node* elsePart = nullptr;
        if (g != cfg.syntheticExit) {
            elsePart = extractUntilJoin(g, join);
        }

        // ③ then が空で else だけある場合 → 条件反転
        if (!thenPart && elsePart) {
            std::swap(thenPart, elsePart);
            cond = "!(" + cond + ")";
        }

        Node* ifNode = Node::If(cond, thenPart, elsePart);

        // ④ join の後を構造化
        //    ただし「この if が join を直接支配している場合だけ」
        if (join != IDX_NONE && join != cfg.syntheticExit) {
            auto it = dom.idom.find(join);
            if (it != dom.idom.end() && it->second == n) {
                if (!used.count(join)) {
                    Node* after = buildStructured(join);
                    if (after) return Node::Seq({ ifNode, after });
                }
            }
        }

        return ifNode;
    }

    // ----------------- 直列（Sequence）復元 -----------------
    Node* buildSequence(UINT start) {
        std::vector<Node*> seq;
        UINT cur = start;

        auto isValid = [&](UINT x) {
            return (x != IDX_NONE && x != cfg.syntheticExit);
            };

        while (true) {
            //printf("BuildSequence(%d) used=%d\n", cur, (int)used.count(cur));
            if (cur == cfg.syntheticExit) break;
            if (used.count(cur)) break;

            // ★ ループヘッダなら while を構築して終わり
            if (isLoopHeader(cur)) {
                Node* loop = buildLoop(cur);
                used.insert(cur);
                seq.push_back(loop);
                break;
            }

            used.insert(cur);

            auto it = cfg.nodes.find(cur);
            if (it == cfg.nodes.end()) break;
            const CFGNode& node = it->second;

            UINT g = node.nextG;
            UINT r = node.nextR;

            bool hasG = isValid(g);
            bool hasR = isValid(r);
            bool cond = isCondNode(cur);

            // 条件ノードなら if にする（R＝true, G＝false）
            if (cond && (hasG || hasR)) {
                UINT falseBranch = hasG ? g : cfg.syntheticExit;
                UINT trueBranch = hasR ? r : cfg.syntheticExit;
                Node* ifNode = buildIf(cur, falseBranch, trueBranch);
                seq.push_back(ifNode);
                break;
            }

            // 非条件ノードで 2 分岐 → if（R＝true, G＝false）
            if (hasG && hasR) {
                Node* ifNode = buildIf(cur, g, r);
                seq.push_back(ifNode);
                break;
            }

            // Block を追加
            seq.push_back(Node::Block(cur));

            // 0 分岐（終端）
            if (!hasG && !hasR) break;

            // 1 分岐（直列）
            UINT next = hasG ? g : r;

            // dominator を使って直列判定
            if (dom.idom[next] != cur) break;

            cur = next;
        }

        if (seq.size() == 1) return seq[0];
        return Node::Seq(seq);
    }

    // ----------------- 構造化エントリ -----------------
    Node* buildStructured(UINT n) {
        //printf("BuildStructured(%d)\n", n);
        if (n == cfg.syntheticExit) return nullptr;
        if (used.count(n)) return nullptr;
        return buildSequence(n);
    }

    // ----------------- ツリー整形（空ノード削除） -----------------
    Node* cleanup(Node* n) {
        if (!n) return nullptr;

        switch (n->kind) {
            case Node::K_If: {
                n->thenPart = cleanup(n->thenPart);
                n->elsePart = cleanup(n->elsePart);
                if (!n->thenPart && !n->elsePart) return nullptr;
                return n;
            }
            case Node::K_Seq: {
                std::vector<Node*> out;
                for (Node* c : n->seq) {
                    Node* cc = cleanup(c);
                    if (cc) out.push_back(cc);
                }
                if (out.empty()) return nullptr;
                if (out.size() == 1) return out[0];
                n->seq = out;
                return n;
            }
            case Node::K_Block:
                return n;
            case Node::K_Exit:
                return n;
            case Node::K_While:
                n->body = cleanup(n->body);
                return n;
        }
        return n;
    }

    // ----------------- 出力 -----------------
    void emit(Node* s, int indent = 0) {
        if (!s) return;
        auto IND = [&](int n) { for (int i = 0; i < n; ++i) printf("    "); };

        switch (s->kind) {
        case Node::K_Block: {
            if (s->id >= pool.size()) return;
            CChip* chip = pool[s->id];
            if (!chip) return;

            // ★ EXIT チップ判定は CFG の nextG/nextR で行う
            bool isExitChip =
                (chip->m_NextG == IDX_EXIT) ||
                (chip->m_NextR == IDX_EXIT);

            IND(indent);
            printf("%s;\n", chip->GetDslText().c_str());
            if (isExitChip) {
                IND(indent);
                printf("exit();\n");
            }
     
            break;
        }
        case Node::K_If: {
            IND(indent);
            printf("if (%s) {\n", s->cond.c_str());
            emit(s->thenPart, indent + 1);
            IND(indent);
            printf("}\n");
            if (s->elsePart) {
                IND(indent);
                printf("else {\n");
                emit(s->elsePart, indent + 1);
                IND(indent);
                printf("}\n");
            }
            break;
        }
        case Node::K_Seq: {
            for (auto* c : s->seq) emit(c, indent);
            break;
        }
        case Node::K_Exit:
            IND(indent);
            printf("exit();\n");
            break;
        case Node::K_While:
            IND(indent);
            printf("while (%s) {\n", s->cond.c_str());
            emit(s->body, indent + 1);
            IND(indent);
            printf("}\n");
            break;
        }
    }

    // ----------------- 実行 -----------------
    void run() {
        buildCFG();
        used.clear();
        Node* root = buildStructured(cfg.start);
        root = cleanup(root);

        printf("void AI() {\n");
        emit(root, 1);
        printf("}\n");
    }
};
