#include <algorithm>
#include <cassert>
#include <cmath>
#include <fcntl.h>
#include <format>
#include <io.h>
#include <iostream>
#include <map>
#include <random>
#include <source_location>
#include <stdexcept>
#include <stdio.h>
#include <vector>
#include <wchar.h>
#include <string>
#include <format>

enum {
	OKE_BIPED,
	OKE_QUADRUPED,
	OKE_VEHICLE,
	OKE_HOVER,
	OKE_FLIGHT,
	OKE_ALL,
};

enum {
	CHIPID_NULL,
	CHIPID_NOP,
	CHIPID_MOVE,
	CHIPID_TURN,
	CHIPID_JUMP,
	CHIPID_ACTION,
	CHIPID_ALTITUDE,
	CHIPID_AMMO,
	CHIPID_FIRE_NEAREST,
	CHIPID_FIRE_FIXED_DIR,
	CHIPID_OPTION,
	CHIPID_DET_OKE,
	CHIPID_DET_OBSTACLE,
	CHIPID_DET_PROJECTILE,
	CHIPID_MAPPOINT,
	CHIPID_SELF_STATUS,
	CHIPID_SUB,
	CHIPID_RAND,
	CHIPID_TIME,
	CHIPID_LOCKON,
	CHIPID_TGT_DISTANCE,
	CHIPID_TGT_DIR,
	CHIPID_FIRE_TGT,
	CHIPID_IS_POS,
	CHIPID_IS_ACTION,
	CHIPID_SOUND,
	CHIPID_GET_STATUS,
	CHIPID_GET_POS,
	CHIPID_GET_TGT_DIR,
	CHIPID_GET_MISC_NUM,
	CHIPID_CALC,
	CHIPID_TRUNCATE,
	CHIPID_CMP,
	CHIPID_CH_SEND,
	CHIPID_CH_RECV,
	CHIPID_FIRE_COUNTER,
	CHIPID_GOTO,
};

typedef unsigned int UINT;

static constexpr UINT IDX_NONE = 0xFFFFFFFF;
static constexpr UINT IDX_EXIT = 0xFFFFFFFE;

//////////////////////////////////////////////////////////////////////////////

inline std::source_location g_LastLocation = std::source_location::current();
#define LastLocation()	(g_LastLocation = location)
#define LastLocationArg	const std::source_location location = std::source_location::current()

class OkeccError : public std::runtime_error {
public:
	explicit OkeccError(const std::string& message)
		: std::runtime_error(FormatMessage(message)) {}

private:
	std::string FormatMessage(const std::string& message){
		return std::format("{}({}): Error: {}\n", g_LastLocation.file_name(), g_LastLocation.line(), message);
	}
};

//////////////////////////////////////////////////////////////////////////////
// bit scaled int

template<UINT _width, UINT _step = 1, int _offset = 0>
class ScaledInt {
public:
	static constexpr UINT width	= _width;
	static constexpr int offset	= _offset;
	static constexpr UINT step	= _step;
	static constexpr int min	= offset;
	static constexpr int max	= offset + (int)(((1U << width) - 1) * step);

	void set(int val){
		if(val < min || max < val){
			throw OkeccError(std::format("Value {} is out of range [{} - {}]", val, min, max));
		}

		m_value = (UINT)((double)(val - offset) / step + 0.5) & ((1 << width) - 1);
	};

	ScaledInt& operator=(int i){
		set(i);
		return *this;
	}

	int get(void){
		return m_value * step + offset;
	}

private:
	UINT	m_value = 0;
};

//////////////////////////////////////////////////////////////////////////////
// Chip class
class CChip {
public:
	CChip(){
		m_id		= CHIPID_NULL;
	}

	virtual ~CChip(){}
	
	virtual std::string GetLayoutText(int line){
		return line == 0 ? std::format("Type:{}", m_id.get()) : "";
	}
	
	bool valid_r(void){return m_next_r < IDX_EXIT;}
	bool valid_g(void){return m_next_g < IDX_EXIT;}

	ScaledInt<6>	m_id;
	ScaledInt<3>	m_raw_g;
	ScaledInt<3>	m_raw_r;

	UINT			m_next_g = IDX_NONE;
	UINT			m_next_r = IDX_NONE;
};

class CChipCond : public CChip {
public:
	virtual ~CChipCond(){}
};

//////////////////////////////////////////////////////////////////////////////
// ゲームには存在しない仮チップ

class CChipGoto : public CChip {
public:
	CChipGoto(){
		m_id = CHIPID_GOTO;
	}

	virtual ~CChipGoto(){}
};

//////////////////////////////////////////////////////////////////////////////
// Chip pool

class CChipPool {
public:
	std::vector<CChip*> m_list;

	UINT add(CChip *chip){
		m_list.push_back(chip);
		return (UINT)m_list.size() - 1;
	}

	size_t size(void){return m_list.size();}

	CChip*& operator[](size_t index) {
		return m_list[index];
	}

	CChip* operator[](size_t index) const {
		return m_list[index];
	}

	// Goto を辿って最終地点を取得
	UINT GetFinalDst(UINT idx){
		UINT dst = idx;

		// 最終地点を取得
		while(dst < m_list.size() && m_list[dst]->m_id.get() == CHIPID_GOTO) dst = m_list[dst]->m_next_g;

		// 最終地点までの Goto はすべて最終地点を指す
		while(idx != dst){
			UINT next = m_list[idx]->m_next_g;
			m_list[idx]->m_next_g = dst;
			idx = next;
		}

		return dst;
	}
};

CChipPool	g_ChipPool;

//////////////////////////////////////////////////////////////////////////////
// Chip tree

class CChipTree {
public:

	UINT m_start	= IDX_NONE;	// 開始 chip
	UINT m_last_g	= IDX_NONE;	// 最後の緑矢印を出している chip
	UINT m_last_r	= IDX_NONE; // 最後の赤矢印を出している chip

	// g, r を update
	void set_g(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(valid_g()) g_ChipPool[m_last_g]->m_next_g = idx;
		m_last_g = idx;
	}

	void set_r(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(valid_r()) g_ChipPool[m_last_r]->m_next_r = idx;
		m_last_r = idx;
	}

	bool valid_start(void) const {return m_start  < IDX_EXIT;}
	bool valid_g    (void) const {return m_last_g < IDX_EXIT;}
	bool valid_r    (void) const {return m_last_r < IDX_EXIT;}

	// チップ単体からツリーに変換
	static CChipTree Chip2Tree(CChip *pchip){
		CChipTree tree;

		tree.m_start =
		tree.m_last_g = g_ChipPool.add(pchip);

		return tree;
	}

	// チップ単体からツリーに変換 (Condition chip)
	static CChipTree CondChip2Tree(CChip *pchip){
		CChipTree tree;

		tree.m_start =
		tree.m_last_g =
		tree.m_last_r = g_ChipPool.add(pchip);

		return tree;
	}

	// Tree にチップ追加
	CChipTree& add(CChip *pchip){
		UINT idx = g_ChipPool.add(pchip);		// チップ追加

		if(valid_start()){
			g_ChipPool[m_last_g]->m_next_g = idx;	// リスト後端に追加したチップをつなげる
		}else{
			m_start = idx;
		}
		m_last_g = idx;

		return *this;
	}
};

CChipTree *g_pCurTree;

CChipTree operator&&(const CChipTree& a, const CChipTree& b){
	CChipTree cc = a;

	// condition な tree か判定
	if(!cc.valid_r() || !b.valid_r()){
		throw OkeccError("Invalid use of && operator: Condition chip expected.");
	}

	// this.r -> b.start を指す
	cc.set_r(b.m_start);

	// False 側: GOTO を生成し this.g, b.g, tree.g はそれを指す
	UINT idx = g_ChipPool.add(new CChipGoto);
	cc.set_g(idx);
	g_ChipPool[b.m_last_g]->m_next_g = idx;

	// True 側: tree.r は b.r になる
	cc.m_last_r = b.m_last_r;

	return cc;
}

CChipTree operator&&(CChipCond& chip, const CChipTree& a){
	return CChipTree::CondChip2Tree(&chip) && a;
}

CChipTree operator&&(const CChipTree& a, CChipCond& chip){
	return a && CChipTree::CondChip2Tree(&chip);
}

CChipTree operator&&(CChipCond& a, CChipCond& b){
	return CChipTree::CondChip2Tree(&a) && CChipTree::CondChip2Tree(&b);
}

//////////////////////////////////////////////////////////////////////////////
// NOP

static constexpr UINT NOP_AE = 62;

class CChipNop : public CChip {
public:
	CChipNop(UINT param){
		m_id	= CHIPID_NOP;
		m_param	= param;
	}

	virtual ~CChipNop(){}
	
	virtual std::string GetLayoutText(int line){
		if(line == 0) return
			m_param.get() == 0 ? "NOP" :
			m_param.get() == NOP_AE ? "Wait AE" :
			std::format("Wait {:2d}", m_param.get());
		else return "";
	}
	
	ScaledInt<5,2> m_param;
};

static void wait(
	UINT param,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipNop(param));
}

static void nop    (LastLocationArg){wait(0,      location);}
static void wait_ae(LastLocationArg){wait(NOP_AE, location);}

//////////////////////////////////////////////////////////////////////////////
// move

class CChipMove : public CChip {
public:
	enum {
		FWD,
		BACK,
		LEFT,
		RIGHT,
		STOP,
	};

	CChipMove(UINT param){
		m_id	= CHIPID_MOVE;
		m_param	= param;
	}

	virtual ~CChipMove(){}

	virtual std::string GetLayoutText(int line){
		if(line == 0){
			return
				m_param.get() == FWD	? "前進" :
				m_param.get() == BACK	? "後退" :
				m_param.get() == LEFT	? "左移" :
				m_param.get() == RIGHT	? "右移" :
										  "停止";
		}
		else return "";
	}
	
	ScaledInt<3> m_param;
};

static void put_move_chip(UINT param){
	g_pCurTree->add(new CChipMove(param));
}

static void move_forward (LastLocationArg){LastLocation(); put_move_chip(CChipMove::FWD);}
static void move_backward(LastLocationArg){LastLocation(); put_move_chip(CChipMove::BACK);}
static void move_left    (LastLocationArg){LastLocation(); put_move_chip(CChipMove::LEFT);}
static void move_right   (LastLocationArg){LastLocation(); put_move_chip(CChipMove::RIGHT);}
static void stop         (LastLocationArg){LastLocation(); put_move_chip(CChipMove::STOP);}

//////////////////////////////////////////////////////////////////////////////
// rotate

class CChipTurn : public CChip {
public:
	enum {
		LEFT,
		RIGHT,
	};

	CChipTurn(UINT param){
		m_id	= CHIPID_TURN;
		m_param	= param;
	}

	virtual ~CChipTurn(){}

	virtual std::string GetLayoutText(int line){
		if(line == 0) return m_param.get() == LEFT	? "旋左" : "旋右";
		else return "";
	}
	
	ScaledInt<1> m_param;
};

static void put_turn_chip(UINT param){
	g_pCurTree->add(new CChipTurn(param));
}

static void turn_left (LastLocationArg){LastLocation(); put_turn_chip(CChipTurn::LEFT);}
static void turn_right(LastLocationArg){LastLocation(); put_turn_chip(CChipTurn::RIGHT);}

//////////////////////////////////////////////////////////////////////////////
// Jump

class CChipJump : public CChip {
public:
	enum {
		FWD,
		BACK,
		LEFT,
		RIGHT,
	};

	CChipJump(UINT param){
		m_id	= CHIPID_JUMP;
		m_param	= param;
	}

	virtual ~CChipJump(){}

	virtual std::string GetLayoutText(int line){
		if(line == 0){
			return
				m_param.get() == FWD	? "跳前" :
				m_param.get() == BACK	? "跳後" :
				m_param.get() == LEFT	? "跳左" : "跳右";
		}
		else return "";
	}
	
	ScaledInt<2> m_param;
};

static void put_jump_chip(UINT param){
	g_pCurTree->add(new CChipJump(param));
}

static void jump_forward (LastLocationArg){LastLocation(); put_jump_chip(CChipJump::FWD);}
static void jump_backward(LastLocationArg){LastLocation(); put_jump_chip(CChipJump::BACK);}
static void jump_left    (LastLocationArg){LastLocation(); put_jump_chip(CChipJump::LEFT);}
static void jump_right   (LastLocationArg){LastLocation(); put_jump_chip(CChipJump::RIGHT);}

//////////////////////////////////////////////////////////////////////////////
// Action

class CChipAction : public CChip {
public:
	enum {
		FIGHT,
		CROUCH,
		GUARD,
		ACTION1,
		ACTION2,
		ACTION3,
	};

	CChipAction(UINT param){
		m_id	= CHIPID_ACTION;
		m_param	= param;
	}

	virtual ~CChipAction(){}

	virtual std::string GetLayoutText(int line){
		if(line == 0){
			return
				m_param.get() == FIGHT		? "格闘" :
				m_param.get() == CROUCH		? "伏せ" :
				m_param.get() == GUARD		? "防御" :
				m_param.get() == ACTION1	? "ア1" :
				m_param.get() == ACTION2	? "ア2" : "ア3";
		}
		else return "";
	}
	
	ScaledInt<2> m_param;
};

static void put_action_chip(UINT param){
	g_pCurTree->add(new CChipAction(param));
}

static void fight  (LastLocationArg){LastLocation(); put_action_chip(CChipAction::FIGHT);}
static void crouch (LastLocationArg){LastLocation(); put_action_chip(CChipAction::CROUCH);}
static void guard  (LastLocationArg){LastLocation(); put_action_chip(CChipAction::GUARD);}
static void action1(LastLocationArg){LastLocation(); put_action_chip(CChipAction::ACTION1);}
static void action2(LastLocationArg){LastLocation(); put_action_chip(CChipAction::ACTION2);}
static void action3(LastLocationArg){LastLocation(); put_action_chip(CChipAction::ACTION3);}

//////////////////////////////////////////////////////////////////////////////
// alt

class CChipAlt : public CChip {
public:
	enum {
		HIGH,
		MID,
		LOW,
	};

	CChipAlt(UINT param){
		m_id	= CHIPID_ALTITUDE;
		m_param	= param;
	}

	virtual ~CChipAlt(){}

	ScaledInt<2> m_param;
};

static void put_alt_chip(UINT param){
	g_pCurTree->add(new CChipAlt(param));
}

static void move_high(LastLocationArg){LastLocation(); put_alt_chip(CChipAlt::HIGH);}
static void move_mid (LastLocationArg){LastLocation(); put_alt_chip(CChipAlt::MID);}
static void move_low (LastLocationArg){LastLocation(); put_alt_chip(CChipAlt::LOW);}

//////////////////////////////////////////////////////////////////////////////
// fire

class CChipFireNearest : public CChip {
public:
	CChipFireNearest(
		int angleCenter,
		int angleRange,
		int distance,
		int type,
		int weapon,
		int cnt
	){
		m_id			= CHIPID_FIRE_NEAREST;
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_type			= type;
		m_weapon		= weapon;
		m_cnt			= cnt;
	}

	virtual ~CChipFireNearest(){}

	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<4, 20, 20>	m_distance;
	ScaledInt<3>			m_type;
	ScaledInt<3>			m_weapon;
	ScaledInt<3, 1, 1>		m_cnt;
};

static void fire(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	int weapon,
	int cnt,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipFireNearest(
		angleCenter,
		angleRange,
		distance,
		type,
		weapon,
		cnt
	));
}

class CChipFireFixedDir : public CChip {
public:
	CChipFireFixedDir(
		int direction,
		int elevation,
		int weapon,
		int cnt
	){
		m_id		= CHIPID_FIRE_FIXED_DIR;
		m_direction	= direction;
		m_elevation	= elevation;
		m_weapon	= weapon;
		m_cnt		= cnt;
	}

	virtual ~CChipFireFixedDir(){}

	ScaledInt<5, 16, -240>	m_direction;
	ScaledInt<4, 16, -112>	m_elevation;
	ScaledInt<3>			m_weapon;
	ScaledInt<3, 1, 1>		m_cnt;
};

static void fire(
	int direction,
	int elevation,
	int weapon,
	int cnt,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipFireFixedDir(
		direction,
		elevation,
		weapon,
		cnt
	));
}

//////////////////////////////////////////////////////////////////////////////
// option

class CChipOption : public CChip {
public:
	CChipOption(UINT param){
		m_id	= CHIPID_OPTION;
		m_param	= param;
	}

	virtual ~CChipOption(){}

	ScaledInt<2, 1, 1> m_param;
};

static void put_option_chip(UINT param){
	g_pCurTree->add(new CChipOption(param));
}

static void option1(LastLocationArg){LastLocation(); put_option_chip(1);}
static void option2(LastLocationArg){LastLocation(); put_option_chip(2);}
static void option3(LastLocationArg){LastLocation(); put_option_chip(3);}

//////////////////////////////////////////////////////////////////////////////
// 近くの OKE を探索

class CChipOkeNum : public CChipCond {
public:
	static constexpr int OP_GE		= 0;
	static constexpr int OP_LE		= 1;
	static constexpr int ENEMY		= 0;
	static constexpr int FRIENDLY	= 1;

	CChipOkeNum(CChipOkeNum& src){
		*this = src;
	}

	virtual ~CChipOkeNum(){}

	CChipOkeNum(
		int angleCenter,
		int angleRange,
		int distance,
		int type,
		int enemy
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_type			= type;
		m_enemy			= enemy;
		m_operator		= OP_GE;
		m_num			= 1;
		m_id			= CHIPID_DET_OKE;
	}

	CChipTree operator>=(int num){
		m_num = num;
		m_operator = OP_GE;
		return CChipTree::CondChip2Tree(this);
	}

	CChipTree operator>(int num){
		m_num = num + 1;
		m_operator = OP_GE;
		return CChipTree::CondChip2Tree(this);
	}

	CChipTree operator<=(int num){
		m_num = num;
		m_operator = OP_LE;
		return CChipTree::CondChip2Tree(this);
	}

	CChipTree operator<(int num){
		m_num = num - 1;
		m_operator = OP_LE;
		return CChipTree::CondChip2Tree(this);
	}

	ScaledInt<3>			m_type;
	ScaledInt<1>		 	m_enemy;
	ScaledInt<4, 20, 20>	m_distance;
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<1>			m_operator;
	ScaledInt<2, 1, 1>		m_num;
};

static CChipOkeNum& oke_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	int enemy
){
	return *(new CChipOkeNum(angleCenter, angleRange, distance, type, enemy));
}

static CChipOkeNum& enemy_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return oke_num(angleCenter, angleRange, distance, type, CChipOkeNum::ENEMY);
}

static CChipOkeNum& friendly_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return oke_num(angleCenter, angleRange, distance, type, CChipOkeNum::FRIENDLY);
}

//////////////////////////////////////////////////////////////////////////////
// if - else - endif

std::vector<UINT>	g_BlockStack;

void if_statement(
	CChipTree &&cc,
	LastLocationArg
){
	LastLocation();

	// CurTree に if 条件のツリーを接続
	g_pCurTree->set_g(cc.m_start);

	// cc.r を GOTO に変換
	UINT idx = g_ChipPool.add(new CChipGoto);
	cc.set_r(idx);
	g_pCurTree->m_last_g = idx;

	// false 飛び先を push
	g_BlockStack.push_back(cc.m_last_g);
}

void if_statement(
	CChipCond &chip,
	LastLocationArg
){
	if_statement(CChipTree::CondChip2Tree(&chip), location);
}

void else_statement(
	LastLocationArg
){
	LastLocation();

	if(g_BlockStack.size() < 1){
		throw OkeccError("Unexpected ENDIF");
	}

	UINT idx = g_pCurTree->m_last_g;	// then 節の最後

	// else 節先頭は，BlockStack に積んでいた false 飛び先
	g_pCurTree->m_last_g = g_BlockStack.back(); g_BlockStack.pop_back();

	// then 節の最後を block statck に積む
	g_BlockStack.push_back(idx);
}

void endif_statement(
	LastLocationArg
){
	LastLocation();

	if(g_BlockStack.size() < 1){
		throw OkeccError("Unexpected ENDIF");
	}

	// 合流 GOTO 生成
	UINT merge = g_ChipPool.add(new CChipGoto);
	g_pCurTree->set_g(merge);

	// false 条件の飛び先合流
	UINT idx = g_BlockStack.back(); g_BlockStack.pop_back();
	g_ChipPool[idx]->m_next_g = merge;
}

#define IF(cc)	if_statement(cc);
#define ELSE	else_statement();
#define ENDIF	endif_statement();

//////////////////////////////////////////////////////////////////////////////
// 焼きなまし法

struct Pos {
	int x, y;
	bool operator<(const Pos& other) const {
		if (x != other.x) return x < other.x;
		return y < other.y;
	}
};

class CarnageSA {
    CChipPool& pool;
    int grid_width;
    int grid_height;
    std::vector<Pos> state;
    std::mt19937 gen;

	// 8方向のベクトル定義 (N, NE, E, SE, S, SW, W, NW)
	inline static const int dx[] = { 0,  1,  1,  1,  0, -1, -1, -1};
	inline static const int dy[] = {-1, -1,  0,  1,  1,  1,  0, -1};

public:
    CarnageSA(CChipPool& p, int w, int h)
        : pool(p), grid_width(w), grid_height(h), gen(std::random_device{}()) {
        initialize_zigzag();
    }

    void initialize_zigzag() {
        for (UINT i = 0; i < (UINT)pool.size(); ++i) {
            int row = (int)i / grid_width;
            int col = (int)i % grid_width;
            int x = (row % 2 == 0) ? col : (grid_width - 1 - col);
            int y = grid_height - 1 - (row % grid_height);
            state.push_back({x, y});
        }
    }

	double calculate_energy(const std::vector<Pos>& current_state) {
	    double base_energy = 0.0;
	    double cohesion_energy = 0.0;
	    std::map<Pos, int> occupancy;

	    // 重心計算
	    double avg_x = 0, avg_y = 0;
	    for (const auto& p : current_state) {
	        avg_x += p.x; avg_y += p.y;
	    }
	    avg_x /= current_state.size();
	    avg_y /= current_state.size();

	    for (UINT i = 0; i < (UINT)current_state.size(); ++i) {
	        Pos p = current_state[i];
	        CChip* chip = pool[i];

	        // 1. 物理制約
	        if (p.x < 0 || p.x >= grid_width || p.y < 0 || p.y >= grid_height) base_energy += 5000.0;
	        occupancy[p]++;

	        // 2. 凝集コスト
	        cohesion_energy += (std::abs(p.x - avg_x) + std::abs(p.y - avg_y)) * 0.05;

	        // 3. Startチップ制約
	        if (i == 0 && p.y != 0) base_energy += (double)std::abs(p.y) * 200.0;

	        // 4. 接続コスト
	        auto eval_link = [&](UINT next_idx) {
	            if (next_idx == IDX_NONE) return 0.0;
	            if (next_idx == IDX_EXIT) {
	                int dx = std::min(p.x, (grid_width - 1) - p.x);
	                int dy = std::min(p.y, (grid_height - 1) - p.y);
	                return (double)std::min(dx, dy) * 150.0;
	            }

	            // --- 厳格なインデックスチェック ---
	            if (next_idx >= (UINT)current_state.size()) {
	                fprintf(stderr, "\n[FATAL ERROR] Invalid connection: Index %u tried to connect to non-existent Index %u (Pool Size: %zu)\n",
	                        i, next_idx, current_state.size());
	                // デバッグしやすくするため、アサーションで停止させるか例外を投げる
	                assert(next_idx < current_state.size());
	                throw std::out_of_range("Connection index out of range");
	            }

	            Pos target = current_state[next_idx];
	            int dist = std::max(std::abs(p.x - target.x), std::abs(p.y - target.y));
	            return (double)(dist - 1);
	        };

	        base_energy += eval_link(chip->m_next_g);
	        if (chip->valid_r()) base_energy += eval_link(chip->m_next_r);
	    }

	    for (auto const& [pos, count] : occupancy) {
	        if (count > 1) base_energy += (double)count * 1000.0;
	    }

	    if (base_energy <= 0.0) return 0.0;
	    return base_energy + cohesion_energy;
	}

    void run() {
        double T = 100.0;
        double cooling = 0.99998; // 凝集が入るので少しゆっくり冷やす
        double current_E = calculate_energy(state);
        auto best_state = state;
        double best_E = current_E;

        std::uniform_int_distribution<UINT> dist_idx(0, (UINT)pool.size() - 1);
        std::uniform_int_distribution<int> dist_move(-1, 1);
        std::uniform_real_distribution<double> dist_prob(0.0, 1.0);

        int i;
        for (i = 0; i < 2000000; ++i) { // 試行回数を増やして精度向上
            auto next_state = state;
            UINT target = dist_idx(gen);

            next_state[target].x += dist_move(gen);
            next_state[target].y += dist_move(gen);

			if(i == 209343){
				int a = 0;
			}
            double next_E = calculate_energy(next_state);

            // 早期終了: エネルギーが極めて低い（ほぼ0）なら終了
            if (next_E < 0.0001) {
                current_E = next_E;
                best_state = next_state;
                break;
            }

            double diff = next_E - current_E;
            if (diff < 0 || dist_prob(gen) < std::exp(-diff / T)) {
                state = next_state;
                current_E = next_E;
                if (current_E < best_E) {
                    best_E = current_E;
                    best_state = state;
                }
            }
            T *= cooling;
            if (i % 200000 == 0) printf("Step: %7d | Energy: %5.4f\n", i, current_E);
        }
        state = best_state;
        printf("Step: %7d | Energy: %5.4f\n", i, current_E);
    }

	void print_layout(const std::vector<Pos>& final_state, const CChipPool& pool) {
	    // 1. グリッドの初期化 (座標からチップIDを逆引きできるようにする)
	    std::vector<std::vector<int>> grid(grid_height, std::vector<int>(grid_width, -1));
	    for (int i = 0; i < (int)final_state.size(); ++i) {
	        const Pos& pos = final_state[i];
	        if (pos.x >= 0 && pos.x < grid_width && pos.y >= 0 && pos.y < grid_height) {
	            grid[pos.y][pos.x] = i;
	        }
	    }

	    // 2. 方向記号の取得ロジック
	    auto get_dir_code = [&](Pos current, UINT next_idx) -> std::string {
	        if (next_idx == IDX_NONE) return "  ";
	        if (next_idx == IDX_EXIT) return "EX";

	        // 範囲チェック: 異常なインデックスの場合は "??" を返す
	        if (next_idx >= (UINT)final_state.size()) return "??";

	        Pos target = final_state[next_idx];
	        int dx = target.x - current.x;
	        int dy = target.y - current.y;

	        // 隣接（距離1）だけでなく、離れている場合も方向を計算
	        if (dx > 0 && dy == 0) return "->";
	        if (dx < 0 && dy == 0) return "<-";
	        if (dx == 0 && dy > 0) return "vv";
	        if (dx == 0 && dy < 0) return "^^";
	        if (dx > 0 && dy > 0) return "-v";
	        if (dx > 0 && dy < 0) return "-^";
	        if (dx < 0 && dy > 0) return "v-";
	        if (dx < 0 && dy < 0) return "^-";

	        return ".."; // 同一座標などのイレギュラー
	    };

	    std::cout << "\n=== Carnage Heart Chip Layout (US-ASCII) ===\n";

	    // ヘッダー（列番号）
	    std::cout << "     ";
	    for (int x = 0; x < grid_width; ++x) {
	        printf("  [%02d]  ", x);
	    }
	    std::cout << "\n";

	    auto print_border = [&]() {
	        std::cout << "    +";
	        for (int x = 0; x < grid_width; ++x) std::cout << "-------+";
	        std::cout << "\n";
	    };

	    for (int y = 0; y < grid_height; ++y) {
	        print_border();

	        // 行1: チップID (poolのインデックスを表示)
	        printf("%2d  |", y);
	        for (int x = 0; x < grid_width; ++x) {
	            int idx = grid[y][x];
	            if (idx != -1) printf(" ID:%02d |", idx % 100);
	            else          printf("       |");
	        }
	        std::cout << "\n    |";

	        // 行2: 接続先方向 (Green / Red)
	        for (int x = 0; x < grid_width; ++x) {
	            int idx = grid[y][x];
	            if (idx != -1) {
	                CChip* chip = pool[idx];
	                std::string g = get_dir_code(final_state[idx], chip->m_next_g);
	                std::string r = chip->valid_r() ? get_dir_code(final_state[idx], chip->m_next_r) : "  ";
	                // 厳密に7文字: G(1) + dir(2) + space(1) + R(1) + dir(2) = 7
	                printf("G%s R%s|", g.c_str(), r.c_str());
	            } else {
	                printf("       |");
	            }
	        }
	        std::cout << "\n";
	    }
	    print_border();
	}

	const std::vector<Pos>& get_result() const { return state; }
};

//////////////////////////////////////////////////////////////////////////////

void print_layout(FILE *fp, const std::vector<Pos>& state, const CChipPool& pool){

	std::vector<std::vector<int>> grid;
	int max_x = 0, max_y = 0;

	// grid にチップIDを配置
	for(UINT u = 0; u < state.size(); ++u) {
		const auto& p = state[u];
		if (p.x > max_x) max_x = p.x;
		if (p.y > max_y) max_y = p.y;
		while ((int)grid.size() <= p.y) grid.emplace_back();
		while ((int)grid[p.y].size() <= p.x) grid[p.y].emplace_back(IDX_NONE);
		grid[p.y][p.x] = u;

		// 次チップの方向から方向コードを決定
		auto get_dir_code = [&](UINT NextChip) -> UINT {
			const auto& next_p = state[NextChip];
			if(next_p.y < p.y){
				if     (next_p.x <  p.x) return 7;
				else if(next_p.x == p.x) return 0;
				else                     return 1;
			}
			else if(next_p.y == p.y){
				if     (next_p.x <  p.x) return 6;
				else                     return 2;
			}
			else{
				if     (next_p.x <  p.x) return 5;
				else if(next_p.x == p.x) return 4;
				else                     return 3;
			}
		};

		// Chip.m_raw_g, m_raw_r を 更新
		if(pool[u]->valid_g()) pool[u]->m_raw_g = get_dir_code(pool[u]->m_next_g);
		if(pool[u]->valid_r()) pool[u]->m_raw_r = get_dir_code(pool[u]->m_next_r);
	}
	
	auto get_chip = [&](int x, int y) -> CChip* {
		if(y < 0 || y >= (int)grid.size()) return nullptr;
		if(x < 0 || x >= (int)grid[y].size()) return nullptr;
		int idx = grid[y][x];
		if(idx == IDX_NONE) return nullptr;
		return pool[idx];
	};

	// chip レイアウト出力
	for(int y = -1; y <= max_y; ++y){
		for(int line = 0; line < 5; ++line){
			if(y == -1 && line < 4){
				line = 3;
				continue;
			}

			if(line == 0 || line == 4) fputs(" ", fp);
			
			for(int x = 0; x <= max_x + 1; ++x){
				CChip *pchip = get_chip(x, y);
				
				if(x > max_x && (line < 1 || 3 < line)) continue;
				
				switch(line){
					// 最上行
					case 0:
						// 左上
						if(!pchip) fputs("  ", fp);
						else if(pchip->valid_g() && pchip->m_raw_g.get() == 7) fputs("←", fp);
						else if(pchip->valid_r() && pchip->m_raw_r.get() == 7) fputs("▽", fp);
						else fputs("／", fp);
						fputs("  ", fp);

						// 中上
						if(!pchip) fputs("  ", fp);
						else if(pchip->valid_g() && pchip->m_raw_g.get() == 0) fputs("↑", fp);
						else if(pchip->valid_r() && pchip->m_raw_r.get() == 0) fputs("△", fp);
						else fputs("  ", fp);
						fputs("  ", fp);

						// 右上
						if(!pchip) fputs("  ", fp);
						else if(pchip->valid_g() && pchip->m_raw_g.get() == 1) fputs("→", fp);
						else if(pchip->valid_r() && pchip->m_raw_r.get() == 1) fputs("▽", fp);
						else fputs("＼", fp);
						break;

					case 1:
					case 2:
					case 3: {
						CChip *pchip_l = get_chip(x - 1, y);
						
						if     (line == 2 && pchip   && pchip  ->valid_g() && pchip  ->m_raw_g.get() == 6) fputs("←", fp);
						else if(line == 2 && pchip   && pchip  ->valid_r() && pchip  ->m_raw_r.get() == 6) fputs("＜", fp);
						else if(line == 2 && pchip_l && pchip_l->valid_g() && pchip_l->m_raw_g.get() == 2) fputs("→", fp);
						else if(line == 2 && pchip_l && pchip_l->valid_r() && pchip_l->m_raw_r.get() == 2) fputs("＞", fp);
						else fputs(pchip || pchip_l ? "｜" : "  ", fp);
						
						if(x > max_x) continue;
						
						fputs(pchip ? std::format("{:<8}", pchip->GetLayoutText(line - 1)).c_str() : "        ", fp);
						break;
					}
					
					// 最下行
					case 4:
						CChip *pchip_down = get_chip(x, y + 1);
						
						// 左下
						if(!pchip) fputs("  ", fp);
						else if(pchip->valid_g() && pchip->m_raw_g.get() == 5) fputs("←", fp);
						else if(pchip->valid_r() && pchip->m_raw_r.get() == 5) fputs("△", fp);
						else fputs("＼", fp);
						fputs(pchip || pchip_down ? "＿" : "  ", fp);

						// 中下
						if     (pchip && pchip->valid_g() && pchip->m_raw_g.get() == 4) fputs("↓", fp);
						else if(pchip && pchip->valid_r() && pchip->m_raw_r.get() == 4) fputs("▽", fp);
						else fputs(pchip || pchip_down ? "＿" : "  ", fp);
						fputs(pchip || pchip_down ? "＿" : "  ", fp);

						// 右下
						if(!pchip) fputs("  ", fp);
						else if(pchip->valid_g() && pchip->m_raw_g.get() == 3) fputs("→", fp);
						else if(pchip->valid_r() && pchip->m_raw_r.get() == 3) fputs("△", fp);
						else fputs("／", fp);
						break;
				}
			}
			fputs("\n", fp);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

void chip_main(void){
	jump_forward();
	IF(enemy_num(0, 416, 160, OKE_ALL))
		IF(enemy_num(0, 128, 320, OKE_ALL) && enemy_num(0, 128, 320, OKE_ALL))
			turn_right();
			jump_forward();
		ELSE
			move_forward();
		ENDIF
		action1();
	ENDIF
	guard();
}

//////////////////////////////////////////////////////////////////////////////
int main(void){

	CChipTree tree;
	g_pCurTree = &tree;

	try{
		chip_main();
	}catch(const OkeccError& e){
		puts(e.what());
		exit(0);
	}

	g_pCurTree->set_g(IDX_EXIT);

	/*
	printf("Number of chip(s): %d\n", (UINT)g_ChipPool.m_list.size());
	for(UINT u = 0; u < g_ChipPool.m_list.size(); ++u){
		if(g_ChipPool[u]){
			printf("ID:%d: type:%d g:%d r:%d\n",
				u,
				g_ChipPool[u]->m_id.get(),
				g_ChipPool[u]->m_next_g,
				g_ChipPool[u]->m_next_r
			);
		}
	}
	*/

	// Goto 飛び先解決
	std::vector<UINT> IdxOld2New;
	std::vector<UINT> IdxNew2Old;

	for(UINT u = 0; u < g_ChipPool.m_list.size(); ++u){
		if(g_ChipPool[u]->m_id.get() != CHIPID_GOTO){
			g_ChipPool[u]->m_next_r = g_ChipPool.GetFinalDst(g_ChipPool[u]->m_next_r);
			g_ChipPool[u]->m_next_g = g_ChipPool.GetFinalDst(g_ChipPool[u]->m_next_g);
			IdxNew2Old.push_back(u);
		}else{
			delete g_ChipPool[u];
		}
		IdxOld2New.push_back(IdxNew2Old.size() - 1);
	}

	//Goto 削除
	for(UINT u = 0; u < IdxNew2Old.size(); ++u){
		g_ChipPool[u] = g_ChipPool[IdxNew2Old[u]];
		if(g_ChipPool[u]->valid_g()) g_ChipPool[u]->m_next_g = IdxOld2New[g_ChipPool[u]->m_next_g];
		if(g_ChipPool[u]->valid_r()) g_ChipPool[u]->m_next_r = IdxOld2New[g_ChipPool[u]->m_next_r];
	}
	g_ChipPool.m_list.resize(IdxNew2Old.size());

	printf("Number of chip(s): %d\n", (UINT)g_ChipPool.m_list.size());
	for(UINT u = 0; u < g_ChipPool.m_list.size(); ++u){
		if(g_ChipPool[u]){
			printf("ID:%d: type:%d g:%d r:%d\n",
				u,
				g_ChipPool[u]->m_id.get(),
				g_ChipPool[u]->m_next_g,
				g_ChipPool[u]->m_next_r
			);
		}
	}
	//exit(0);

	CarnageSA sa(g_ChipPool, 15, 15);
	sa.run();
	sa.print_layout(sa.get_result(), g_ChipPool);
	//_setmode(_fileno(stdout), _O_U16TEXT);
	FILE *fp = fopen("chip.txt", "w");
	print_layout(fp, sa.get_result(), g_ChipPool);
	fclose(fp);

	return 0;
}
