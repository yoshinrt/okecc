#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cassert>
#include <climits>
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
#include <string>
#include <vector>
#include <wchar.h>

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
	UINT m_start = IDX_NONE;	// 開始チップの index
	UINT m_width;
	UINT m_height;

	CChipPool(UINT width, UINT height) : m_width(width), m_height(height) {}

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

CChipPool	g_ChipPool(15, 15);

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
				m_param.get() == ACTION1	? "Action1" :
				m_param.get() == ACTION2	? "Action2" : "Action3";
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

#include <map>
#include <algorithm>
#include <vector>

struct PathNode {
    uint32_t id;
    int dist_to_exit; // EXITまでの最大距離
};

// Y座標を指定し、それ以降のチップを一括シフトする
void shift_y_range(std::vector<Pos>& state, int threshold_y, int delta) {
    for (auto& p : state) {
        if (delta > 0 && p.y >= threshold_y) p.y += delta;
        else if (delta < 0 && p.y <= threshold_y) p.y += delta;
    }
}

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
    CarnageSA(CChipPool& p)
        : pool(p), grid_width(p.m_width), grid_height(p.m_height), gen(std::random_device{}()) {
        initialize();
    }
    
	// 座標の正規化：最小値を (0, 0) に合わせる
	void normalize_coordinates() {
	    int min_x = INT_MAX, min_y = INT_MAX;
	    bool has_data = false;
	
	    for (const auto& p : state) {
	        if (p.x != INT_MAX) {
	            min_x = std::min(min_x, p.x);
	            min_y = std::min(min_y, p.y);
	            has_data = true;
	        }
	    }
	
	    if (!has_data) return;
	
	    for (auto& p : state) {
	        if (p.x != INT_MAX) {
	            p.x -= min_x;
	            p.y -= min_y;
	        }
	    }
	}
	
	void initialize(void);
	std::vector<int> calculate_max_distances(void);
	uint32_t find_join_node(uint32_t start_idx, const std::vector<int>& dists);
	
	double calculate_energy(const std::vector<Pos>& current_state) {
	    double base_energy = 0.0;
	    double cohesion_energy = 0.0;
	    
	    // 1. 占有状況の高速検索用マップ
	    std::vector<int> grid_occupancy(grid_width * grid_height, -1);
	    std::map<Pos, int> overlap_count; // 重なりペナルティ用

	    const size_t N = current_state.size();

	    for (UINT i = 0; i < (UINT)current_state.size(); ++i) {
	        Pos p = current_state[i];
	        if (p.x >= 0 && p.x < grid_width && p.y >= 0 && p.y < grid_height) {
	            grid_occupancy[p.y * grid_width + p.x] = (int)i;
	        }
	        overlap_count[p]++;
	    }

	    // 重心計算
	    double avg_x = 0, avg_y = 0;
	    for (const auto& p : current_state) {
	        avg_x += p.x; avg_y += p.y;
	    }
	    avg_x /= current_state.size();
	    avg_y /= current_state.size();

	    // 隣接リスト（outgoing / incoming）構築
	    std::vector<std::vector<UINT>> adj_out(N), adj_in(N);
	    for (UINT u = 0; u < N; ++u) {
	        if (pool[u]->m_next_g != IDX_NONE && pool[u]->m_next_g < N) {
	            adj_out[u].push_back(pool[u]->m_next_g);
	            adj_in[pool[u]->m_next_g].push_back(u);
	        }
	        if (pool[u]->m_next_r != IDX_NONE && pool[u]->m_next_r < N) {
	            adj_out[u].push_back(pool[u]->m_next_r);
	            adj_in[pool[u]->m_next_r].push_back(u);
	        }
	    }

	    // パラメータ（調整可能）
	    const int hop_k = 2;                      // ホップ幅
	    const double weight_out = 0.25;           // outgoing に対する重み
	    const double weight_in  = 0.12;           // incoming に対する重み（小さめ）
	    const double max_local_penalty = 300.0;   // 局所ペナルティ上限

	    for (UINT i = 0; i < (UINT)current_state.size(); ++i) {
	        Pos p = current_state[i];
	        CChip* chip = pool[i];

	        // 物理制約
	        if (p.x < 0 || p.x >= grid_width || p.y < 0 || p.y >= grid_height) base_energy += 5000.0;

	        // 凝集コスト（全体的な重心への寄せ）
	        cohesion_energy += (std::abs(p.x - avg_x) + std::abs(p.y - avg_y)) * 0.05;

	        // Startチップ制約
	        if (i == pool.m_start && p.y != 0) base_energy += (double)std::abs(p.y) * 200.0;

	        // 接続コスト（NOP経路検証付き）
	        auto eval_link = [&](UINT next_idx) {
	            if (next_idx == IDX_NONE) return 0.0;
	            if (next_idx == IDX_EXIT) {
	                int dx_edge = std::min(p.x, (grid_width - 1) - p.x);
	                int dy_edge = std::min(p.y, (grid_height - 1) - p.y);
	                return (double)std::min(dx_edge, dy_edge) * 150.0;
	            }

	            assert(next_idx < current_state.size());
	            Pos target = current_state[next_idx];
	            
	            int diff_x = target.x - p.x;
	            int diff_y = target.y - p.y;
	            int abs_dx = std::abs(diff_x);
	            int abs_dy = std::abs(diff_y);
	            int dist = std::max(abs_dx, abs_dy);

	            if (dist <= 1) return 0.0; // 隣接

	            double route_penalty = 0.0;
	            Pos check = p;

	            // dist 回（NOPの必要数分）ステップを繰り返す
	            for (int step = 1; step < dist; ++step) {
	                bool found_step = false;
	                
	                if (abs_dx == abs_dy) {
	                    check.x += (diff_x > 0) ? 1 : -1;
	                    check.y += (diff_y > 0) ? 1 : -1;
	                    found_step = true;
	                } else {
	                    int main_step_x = 0, main_step_y = 0;
	                    bool x_is_main = (abs_dx > abs_dy);
	                    if (x_is_main) {
	                        main_step_x = (diff_x > 0) ? 1 : -1;
	                    } else {
	                        main_step_y = (diff_y > 0) ? 1 : -1;
	                    }

	                    int sub_dir = x_is_main ? ((diff_y > 0) ? 1 : (diff_y < 0 ? -1 : 0))
	                                            : ((diff_x > 0) ? 1 : (diff_x < 0 ? -1 : 0));
	                    
	                    int candidates[] = { sub_dir, 0, -sub_dir };

	                    for (int c : candidates) {
	                        Pos next_p = check;
	                        if (x_is_main) {
	                            next_p.x += main_step_x;
	                            next_p.y += c;
	                        } else {
	                            next_p.x += c;
	                            next_p.y += main_step_y;
	                        }

	                        // 条件：ターゲットとの残り距離が、ステップ数を超えない（遠ざかりすぎない）
	                        int remaining_dist = std::max(std::abs(target.x - next_p.x), std::abs(target.y - next_p.y));
	                        if (remaining_dist <= (dist - step)) {
	                            // 空き地チェック
	                            if (next_p.x >= 0 && next_p.x < grid_width && next_p.y >= 0 && next_p.y < grid_height) {
	                                int occupant = grid_occupancy[next_p.y * grid_width + next_p.x];
	                                if (occupant == -1) {
	                                    check = next_p;
	                                    found_step = true;
	                                    break;
	                                }
	                            }
	                        }
	                    }

	                    // どこも空いていなければ、仕方なく「近づく」方向へ進みペナルティ
	                    if (!found_step) {
	                        if (x_is_main) {
	                            check.x += main_step_x;
	                            check.y += sub_dir;
	                        } else {
	                            check.x += sub_dir;
	                            check.y += main_step_y;
	                        }
	                        route_penalty += 50.0;
	                    }
	                }

	                // 枠外ペナルティ（found_step で空き地が見つからなかった場合も含む）
	                if (check.x < 0 || check.x >= grid_width || check.y < 0 || check.y >= grid_height) {
	                    route_penalty += 100.0;
	                }
	            }

	            return (double)(dist - 1) + route_penalty;
	        };

	        base_energy += eval_link(chip->m_next_g);
	        if (chip->valid_r()) base_energy += eval_link(chip->m_next_r);

	        // ホップ版局所平均距離を計算（outgoing と incoming を別個に BFS）
	        auto bfs_mean_dist = [&](UINT src, bool use_out, bool use_in) -> double {
	            std::vector<int> dist(N, -1);
	            std::vector<UINT> q;
	            dist[src] = 0;
	            q.push_back(src);
	            size_t qhead = 0;
	            while (qhead < q.size()) {
	                UINT v = q[qhead++];
	                if (dist[v] >= hop_k) continue;
	                if (use_out) {
	                    for (UINT w : adj_out[v]) {
	                        if (dist[w] == -1) { dist[w] = dist[v] + 1; q.push_back(w); }
	                    }
	                }
	                if (use_in) {
	                    for (UINT w : adj_in[v]) {
	                        if (dist[w] == -1) { dist[w] = dist[v] + 1; q.push_back(w); }
	                    }
	                }
	            }
	            double sumd = 0.0;
	            int cnt = 0;
	            for (UINT j = 0; j < N; ++j) {
	                if (j == src) continue;
	                if (dist[j] > 0 && dist[j] <= hop_k) {
	                    sumd += (double)(std::abs(current_state[j].x - p.x) + std::abs(current_state[j].y - p.y)); // Manhattan
	                    ++cnt;
	                }
	            }
	            return cnt > 0 ? (sumd / cnt) : 0.0;
	        };

	        double mean_out = bfs_mean_dist(i, true, false);
	        double mean_in  = bfs_mean_dist(i, false, true);

	        // 局所ペナルティは「遠いほど正のペナルティ」
	        double local_pen_out = (mean_out > 0.0) ? (weight_out * mean_out) : 0.0;
	        double local_pen_in  = (mean_in  > 0.0) ? (weight_in  * mean_in)  : 0.0;
	        double local_pen = local_pen_out + local_pen_in;
	        if (local_pen > max_local_penalty) local_pen = max_local_penalty;

	        base_energy += local_pen;
	    }

	    // 重なりペナルティ（通常は swap により 0 だが防御的に残す）
	    for (auto const& kv : overlap_count) {
	        int count = kv.second;
	        if (count > 1) base_energy += (double)count * 1000.0;
	    }

	    if (base_energy <= 0.0) return 0.0;
	    return base_energy + cohesion_energy;
	}
	
	void run() {
		// 改良版 SA:
		// - 隣接スワップ中心（局所探索）
		// - 低確率で任意2点スワップ（global）とランダムジャンプ
		// - k個のインデックスを回転させる multi-rotate operator
		// - 停滞検出によるリヒート（温度を復元して探索幅を一時拡大）
		double T0 = 100.0;
		double T = T0;
		double cooling = 0.99997; // 既定より少し緩やかに
		double current_E = calculate_energy(state);
		auto best_state = state;
		double best_E = current_E;

		std::uniform_int_distribution<UINT> dist_idx(0, (UINT)pool.size() - 1);
		std::uniform_int_distribution<int> dist_dir(0, 7);
		std::uniform_real_distribution<double> dist_prob(0.0, 1.0);
		std::uniform_int_distribution<int> dist_x(0, grid_width - 1);
		std::uniform_int_distribution<int> dist_y(0, grid_height - 1);

		// occupancy
		std::vector<int> occ(grid_width * grid_height, -1);
		auto rebuild_occ = [&](const std::vector<Pos>& s) {
			std::fill(occ.begin(), occ.end(), -1);
			for (UINT j = 0; j < (UINT)s.size(); ++j) {
				const Pos& pp = s[j];
				if (pp.x >= 0 && pp.x < grid_width && pp.y >= 0 && pp.y < grid_height)
					occ[pp.y * grid_width + pp.x] = (int)j;
			}
			};
		rebuild_occ(state);

		// move probabilities (base)
		double p_random_swap = 0.04; // 任意2点スワップ
		double p_long = 0.01;        // ランダムセルジャンプ
		double p_rotate = 0.02;      // multi-rotate operator
		// neighbor-swap が残りの確率

		// stagnation / reheating
		const int stagnation_limit = 200000;
		int iter_since_improve = 0;
		const double reheating_multiplier = 1.8;
		const int reheating_boost_steps = 50000;
		int reheating_steps_left = 0;

		const int max_iter = 2000000;
		for (int iter = 0; iter < max_iter; ++iter) {
			auto next_state = state;
			UINT a = dist_idx(gen);
			double r = dist_prob(gen);

			bool proposed = false;
			int b = -1; // -1 means move to empty cell

			// Possibly boost exploratory moves during reheating
			double local_p_random_swap = p_random_swap;
			double local_p_long = p_long;
			double local_p_rotate = p_rotate;
			if (reheating_steps_left > 0) {
				local_p_random_swap = std::min(0.5, p_random_swap * 6.0);
				local_p_long = std::min(0.2, p_long * 6.0);
				local_p_rotate = std::min(0.5, p_rotate * 6.0);
				reheating_steps_left--;
			}

			double threshold1 = local_p_random_swap;
			double threshold2 = threshold1 + local_p_long;
			double threshold3 = threshold2 + local_p_rotate;

			if (r < threshold1) {
				// 任意 2 点スワップ (global)
				UINT bb = dist_idx(gen);
				if (bb == a) continue;
				std::swap(next_state[a], next_state[bb]);
				proposed = true;
				b = (int)bb;
			}
			else if (r < threshold2) {
				// ランダムセルへのジャンプ（空きがあれば移動、なければスワップ）
				int nx = dist_x(gen);
				int ny = dist_y(gen);
				int occIdx = occ[ny * grid_width + nx];
				if (occIdx == -1) {
					next_state[a].x = nx;
					next_state[a].y = ny;
					b = -1;
					proposed = true;
				}
				else {
					UINT bb = (UINT)occIdx;
					if (bb == a) continue;
					std::swap(next_state[a], next_state[bb]);
					proposed = true;
					b = (int)bb;
				}
			}
			else if (r < threshold3) {
				// multi-rotate: k 個ランダムインデックスを選び位置を回転させる
				int k = 3 + (gen() % 3); // 3..5
				std::vector<UINT> ids;
				ids.reserve(k);
				// collect distinct indices
				std::uniform_int_distribution<UINT> di(0, (UINT)pool.size() - 1);
				while ((int)ids.size() < k) {
					UINT cid = di(gen);
					// ensure unique
					bool found = false;
					for (UINT v : ids) if (v == cid) { found = true; break; }
					if (!found) ids.push_back(cid);
				}
				// rotate positions among ids (circular right shift)
				std::vector<Pos> tmp(k);
				for (int i = 0; i < k; ++i) tmp[i] = next_state[ids[i]];
				for (int i = 0; i < k; ++i) next_state[ids[(i + 1) % k]] = tmp[i];
				proposed = true;
				b = -2; // special marker
			}
			else {
				// 隣接セルとのスワップ（8方向）
				int dir = dist_dir(gen);
				int nx = state[a].x + dx[dir];
				int ny = state[a].y + dy[dir];
				if (nx < 0 || nx >= grid_width || ny < 0 || ny >= grid_height) continue;
				int occIdx = occ[ny * grid_width + nx];
				if (occIdx == -1) {
					next_state[a].x = nx;
					next_state[a].y = ny;
					b = -1;
					proposed = true;
				}
				else {
					UINT bb = (UINT)occIdx;
					if (bb == a) continue;
					std::swap(next_state[a], next_state[bb]);
					proposed = true;
					b = (int)bb;
				}
			}

			if (!proposed) continue;

			double next_E = calculate_energy(next_state);

			// 早期最適性判定
			if (next_E < 0.0001) {
				state = next_state;
				current_E = next_E;
				best_state = next_state;
				best_E = next_E;
				break;
			}

			double diff = next_E - current_E;
			if (diff < 0 || dist_prob(gen) < std::exp(-diff / T)) {
				// accept
				state = next_state;
				// rebuild occ (safer than incremental updates for multiple operators)
				rebuild_occ(state);
				current_E = next_E;
				if (current_E < best_E - 1e-9) {
					best_E = current_E;
					best_state = state;
					iter_since_improve = 0;
				}
				else {
					++iter_since_improve;
				}
			}
			else {
				++iter_since_improve;
			}

			// cooling
			T *= cooling;

			// stagnation -> reheating
			if (iter_since_improve >= stagnation_limit) {
				// reheat
				T = std::max(T, T0) * reheating_multiplier;
				reheating_steps_left = reheating_boost_steps;
				iter_since_improve = 0;
				// optionally print a message
				printf("Reheat at iter %d : T=%.4f\n", iter, T);
			}

			if (iter % 200000 == 0) printf("Step: %7d | Energy: %5.4f | Best: %5.4f\n", iter, current_E, best_E);
		}

		state = best_state;
		printf("Step: %7d | Energy: %5.4f\n", max_iter, best_E);
	}

	const std::vector<Pos>& get_result() const { return state; }
};

std::vector<int> CarnageSA::calculate_max_distances() {
    const int num_chips = (int)pool.size();
    // -1 は「まだ EXIT までの距離が不明」または「到達不能」を意味する
    std::vector<int> dists(num_chips, -1);

    // 1. 初期状態：EXIT に直接つながっているチップの距離を 1 とする
    for (int i = 0; i < num_chips; ++i) {
        if (pool[i]->m_next_g == IDX_EXIT || 
           (pool[i]->valid_r() && pool[i]->m_next_r == IDX_EXIT)) {
            dists[i] = 1;
        }
    }

    // 2. 最大 chip 数分だけ反復して距離を更新する（ベルマン・フォード法）
    // 最長パスを求めるため、dists[next] + 1 が現在の dists[i] より大きければ更新
    for (int iter = 0; iter < num_chips; ++iter) {
        bool changed = false;
        for (int i = 0; i < num_chips; ++i) {
            auto update = [&](uint32_t next) {
                if (next < (uint32_t)num_chips && dists[next] != -1) {
                    if (dists[i] < dists[next] + 1) {
                        dists[i] = dists[next] + 1;
                        changed = true;
                    }
                }
            };

            update(pool[i]->m_next_g);
            if (pool[i]->valid_r()) {
                update(pool[i]->m_next_r);
            }
        }
        // 更新がなくなれば、すべての最短ではない「最長パス」が確定したことになる
        if (!changed) break;
    }

    return dists;
}

void CarnageSA::initialize() {
    // --- 0. 準備 ---
    const int num_chips = (int)pool.m_list.size();
    state.assign(num_chips, {INT_MAX, INT_MAX});
    std::vector<int> vertical_lines_x;

    // --- 1. ヘルパー関数の定義 (ラムダ式) ---

    // EXIT までの最長距離を計算 (Bellman-Ford法)
    auto calculate_max_distances = [&]() {
        std::vector<int> dists(num_chips, -1);
        for (int i = 0; i < num_chips; ++i) {
            if (pool.m_list[i]->m_next_g == IDX_EXIT || 
               (pool.m_list[i]->valid_r() && pool.m_list[i]->m_next_r == IDX_EXIT)) {
                dists[i] = 1;
            }
        }
        for (int iter = 0; iter < num_chips; ++iter) {
            bool changed = false;
            for (int i = 0; i < num_chips; ++i) {
                auto update = [&](uint32_t from_idx, uint32_t next) {
                    if (next < (uint32_t)num_chips && dists[next] != -1) {
                        if (dists[from_idx] < dists[next] + 1) {
                            dists[from_idx] = dists[next] + 1;
                            changed = true;
                        }
                    }
                };
                update(i, pool.m_list[i]->m_next_g);
                if (pool.m_list[i]->valid_r()) update(i, pool.m_list[i]->m_next_r);
            }
            if (!changed) break;
        }
        return dists;
    };
    std::vector<int> dists = calculate_max_distances();

    // 距離リストから安全に値を取得
    auto get_d = [&](uint32_t idx) -> int {
        if (idx == IDX_EXIT) return 0;
        if (idx == IDX_NONE || idx >= (uint32_t)dists.size()) return -100;
        return dists[idx];
    };

    // 未配置の分岐先を特定
    auto get_unplaced_branch = [&](uint32_t node_idx) -> uint32_t {
        if (node_idx >= (uint32_t)pool.m_list.size()) return IDX_NONE;
        CChip* chip = pool.m_list[node_idx];
        if (chip->m_next_g != IDX_EXIT && chip->m_next_g != IDX_NONE && state[chip->m_next_g].x == INT_MAX) return chip->m_next_g;
        if (chip->valid_r() && chip->m_next_r != IDX_EXIT && chip->m_next_r != IDX_NONE && state[chip->m_next_r].x == INT_MAX) return chip->m_next_r;
        return IDX_NONE;
    };

    // 合流先を探索 (最長パス優先)
    auto find_join_node = [&](uint32_t start_idx) -> uint32_t {
        uint32_t curr = start_idx;
        while (curr != IDX_EXIT && curr != IDX_NONE) {
            if (curr < (uint32_t)state.size() && state[curr].x != INT_MAX) return curr;
            bool has_r = (curr < (uint32_t)pool.m_list.size()) ? pool.m_list[curr]->valid_r() : false;
            int dg = (curr < (uint32_t)pool.m_list.size()) ? get_d(pool.m_list[curr]->m_next_g) : -100;
            int dr = (curr < (uint32_t)pool.m_list.size() && has_r) ? get_d(pool.m_list[curr]->m_next_r) : -100;
            curr = (dg >= dr) ? pool.m_list[curr]->m_next_g : pool.m_list[curr]->m_next_r;
        }
        return IDX_NONE;
    };

    // 接続の書き換え
    auto modify_next = [&](uint32_t from, uint32_t old_to, uint32_t new_to) {
        if (from >= (uint32_t)pool.m_list.size()) return;
        if (pool.m_list[from]->m_next_g == old_to) pool.m_list[from]->m_next_g = new_to;
        else if (pool.m_list[from]->valid_r() && pool.m_list[from]->m_next_r == old_to) pool.m_list[from]->m_next_r = new_to;
    };

    // GoToチップの動的追加
    auto add_goto_chip = [&](int x, int y) -> uint32_t {
        uint32_t new_id = (uint32_t)pool.m_list.size();
        pool.m_list.push_back(new CChipGoto()); 
        state.push_back({x, y});
        return new_id;
    };

    // 論理的なチップ数カウント
    auto count_path_chips = [&](uint32_t start, uint32_t goal) -> int {
        if (start == goal) return 0;
        int count = 0;
        uint32_t curr = start;
        while (curr != goal && curr != IDX_EXIT && curr != IDX_NONE && count < 256) {
            count++;
            bool has_r = (curr < (uint32_t)pool.m_list.size()) ? pool.m_list[curr]->valid_r() : false;
            int dg = (curr < (uint32_t)pool.m_list.size()) ? get_d(pool.m_list[curr]->m_next_g) : -100;
            int dr = (curr < (uint32_t)pool.m_list.size() && has_r) ? get_d(pool.m_list[curr]->m_next_r) : -100;
            curr = (dg >= dr) ? pool.m_list[curr]->m_next_g : pool.m_list[curr]->m_next_r;
        }
        return count;
    };

    // --- 2. パス配置再帰関数 ---
    auto place_path = [&](auto self, uint32_t start_idx, int start_x, int start_y) -> void {
        std::vector<uint32_t> current_path_chips;
        uint32_t curr = start_idx;
        int cur_x = start_x;

        while (curr != IDX_EXIT && curr != IDX_NONE) {
            if (curr < (uint32_t)state.size() && state[curr].x != INT_MAX) {
                vertical_lines_x.push_back(cur_x - 1);
                break;
            }
            if (curr >= (uint32_t)state.size()) state.resize(curr + 1, {INT_MAX, INT_MAX});
            state[curr] = {cur_x, start_y};
            current_path_chips.push_back(curr);

            uint32_t next_node = IDX_NONE;
            if (curr < (uint32_t)num_chips) {
                bool has_r = pool.m_list[curr]->valid_r();
                int dg = get_d(pool.m_list[curr]->m_next_g);
                int dr = has_r ? get_d(pool.m_list[curr]->m_next_r) : -100;
                next_node = (dg >= dr) ? pool.m_list[curr]->m_next_g : pool.m_list[curr]->m_next_r;
            } else {
                next_node = pool.m_list[curr]->m_next_g;
            }
            curr = next_node;
            cur_x++;
        }

        for (uint32_t node_idx : current_path_chips) {
            if (node_idx >= (uint32_t)num_chips) continue;
            uint32_t branch_target = get_unplaced_branch(node_idx);
            if (branch_target == IDX_NONE || branch_target == IDX_EXIT) continue;

            uint32_t join_node = find_join_node(branch_target);
            if (join_node == IDX_NONE) continue;

            int bx = state[node_idx].x + 1;
            bool move_up = (std::find(vertical_lines_x.begin(), vertical_lines_x.end(), bx) != vertical_lines_x.end()) ? true : (rand() % 2 == 0);
            int target_y = move_up ? state[node_idx].y - 1 : state[node_idx].y + 1;

            shift_y_range(state, target_y, move_up ? -1 : 1);

            int target_bx = state[join_node].x - 1;
            uint32_t final_branch_start = branch_target;
            int path_len = count_path_chips(branch_target, join_node);

            if (path_len == 0) {
                // 0 chip コの字型迂回
                uint32_t g1 = add_goto_chip(bx, target_y);
                uint32_t g2 = add_goto_chip(std::max(bx, target_bx), target_y);
                modify_next(node_idx, branch_target, g1);
                pool.m_list[g1]->m_next_g = g2;
                pool.m_list[g2]->m_next_g = join_node;
                continue; 
            } else if (path_len == 1) {
                // 1 chip 延長
                uint32_t g1 = add_goto_chip(std::max(bx + 1, target_bx), target_y);
                pool.m_list[g1]->m_next_g = join_node;
                modify_next(branch_target, join_node, g1);
                final_branch_start = branch_target;
            }

            self(self, final_branch_start, bx, target_y);
        }
    };

    // --- 3. 実行 ---
    place_path(place_path, 0, 0, 0);
    normalize_coordinates();
}

uint32_t CarnageSA::find_join_node(uint32_t start_idx, const std::vector<int>& dists) {
    uint32_t curr = start_idx;
    const int num_chips = (int)pool.size();

    while (curr != IDX_EXIT && curr != IDX_NONE) {
        if (state[curr].x != INT_MAX) return curr; // 合流点発見

        bool has_r = pool[curr]->valid_r();
        int d_g = (pool[curr]->m_next_g == IDX_EXIT) ? 0 : 
                  (pool[curr]->m_next_g < num_chips ? dists[pool[curr]->m_next_g] : -100);
        int d_r = has_r ? ((pool[curr]->m_next_r == IDX_EXIT) ? 0 : 
                  (pool[curr]->m_next_r < num_chips ? dists[pool[curr]->m_next_r] : -100)) : -100;

        curr = (d_g >= d_r) ? pool[curr]->m_next_g : pool[curr]->m_next_r;
    }
    return IDX_NONE;
}

//////////////////////////////////////////////////////////////////////////////

void print_layout_svg(FILE *fp, const std::vector<Pos>& state, const CChipPool& pool) {
    const int CELL_SIZE = 80;    // 1セルの大きさ
    const int CHIP_SIZE = 50;    // チップ（箱）の大きさ
    const int OFFSET = (CELL_SIZE - CHIP_SIZE) / 2;
    const int VIEW_WIDTH = 800;  // ブラウザ等での表示幅
    const int VIEW_HEIGHT = 800; // ブラウザ等での表示高さ

    // 1. 全チップを包含する矩形を計算 (viewBox用)
    int min_x = INT_MAX, min_y = INT_MAX;
    int max_x = INT_MIN, max_y = INT_MIN;

    for (const auto& p : state) {
        if (p.x < min_x) min_x = p.x;
        if (p.y < min_y) min_y = p.y;
        if (p.x > max_x) max_x = p.x;
        if (p.y > max_y) max_y = p.y;
    }
    
    // データが空の場合のガード
    if (state.empty()) {
        min_x = min_y = 0;
        max_x = max_y = 5;
    }

    int grid_w = (max_x - min_x + 1);
    int grid_h = (max_y - min_y + 1);
    int vb_width = grid_w * CELL_SIZE + 40;
    int vb_height = grid_h * CELL_SIZE + 40;
    int vb_x = min_x * CELL_SIZE - 20;
    int vb_y = min_y * CELL_SIZE - 20;

    // 2. ヘッダー出力
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<svg width=\"%d\" height=\"%d\" viewBox=\"%d %d %d %d\" xmlns=\"http://www.w3.org/2000/svg\">\n", 
            VIEW_WIDTH, VIEW_HEIGHT, vb_x, vb_y, vb_width, vb_height);
    
    // 矢印の定義 (marker)
    fprintf(fp, "  <defs>\n");
    fprintf(fp, "    <marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"9\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\">\n");
    fprintf(fp, "      <path d=\"M0,0 L0,6 L9,3 z\" fill=\"context-stroke\" />\n");
    fprintf(fp, "    </marker>\n");
    fprintf(fp, "  </defs>\n");

    // 背景
    fprintf(fp, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#f8f9fa\" />\n", vb_x, vb_y, vb_width, vb_height);

    // 3. 接続線（エッジ）の描画
    auto draw_edge = [&](int from_idx, uint32_t to_idx, const char* color) {
        if (to_idx == IDX_NONE) return;
        
        Pos p1 = state[from_idx];
        double x1 = p1.x * CELL_SIZE + CELL_SIZE / 2.0;
        double y1 = p1.y * CELL_SIZE + CELL_SIZE / 2.0;

        if (to_idx == IDX_EXIT) {
            // EXITは右側にダッシュ線で表現
            fprintf(fp, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"%s\" stroke-width=\"2\" stroke-dasharray=\"4\" marker-end=\"url(#arrow)\" />\n",
                    x1 + (CHIP_SIZE/2.0), y1, x1 + (CHIP_SIZE/2.0) + 20, y1, color);
            return;
        }

        Pos p2 = state[to_idx];
        double x2 = p2.x * CELL_SIZE + CELL_SIZE / 2.0;
        double y2 = p2.y * CELL_SIZE + CELL_SIZE / 2.0;

        double dx = x2 - x1;
        double dy = y2 - y1;
        double dist = sqrt(dx*dx + dy*dy);
        
        if (dist < 0.1) return; // 同一座標

        // チップの縁までのオフセット（相似比を利用）
        double ratio = (CHIP_SIZE / 2.0) / dist;
        double ox = dx * ratio;
        double oy = dy * ratio;

        fprintf(fp, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"%s\" stroke-width=\"2\" marker-end=\"url(#arrow)\" />\n",
                x1 + ox, y1 + oy, x2 - ox, y2 - oy, color);
    };

    for (int i = 0; i < (int)state.size(); ++i) {
        CChip* chip = pool[i];
        draw_edge(i, chip->m_next_g, "#28a745"); // Green
        if (chip->valid_r()) {
            draw_edge(i, chip->m_next_r, "#dc3545"); // Red
        }
    }

    // 4. チップ（箱）の描画
    for (int i = 0; i < (int)state.size(); ++i) {
        int x = state[i].x * CELL_SIZE + OFFSET;
        int y = state[i].y * CELL_SIZE + OFFSET;

        // 箱
        fprintf(fp, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"4\" fill=\"white\" stroke=\"#343a40\" stroke-width=\"2\" />\n",
                x, y, CHIP_SIZE, CHIP_SIZE);
        
        // テキスト (Type と ID)
        fprintf(fp, "  <text x=\"%d\" y=\"%d\" font-family=\"Consolas, monospace\" font-size=\"10\" text-anchor=\"middle\" fill=\"#6c757d\">Type:%u</text>\n",
                x + CHIP_SIZE / 2, y + 20, pool[i]->m_id.get());
        fprintf(fp, "  <text x=\"%d\" y=\"%d\" font-family=\"Consolas, monospace\" font-size=\"11\" text-anchor=\"middle\" font-weight=\"bold\">ID:%d</text>\n",
                x + CHIP_SIZE / 2, y + 38, i);
    }

    fprintf(fp, "</svg>\n");
}

//////////////////////////////////////////////////////////////////////////////

void chip_main(void){
	for(int i = 0; i < 1; ++i){
		IF(enemy_num(0, 416, 160, OKE_ALL))
			IF(enemy_num(0, 128, 320, OKE_ALL) && enemy_num(0, 128, 320, OKE_ALL))
				turn_right();
				jump_backward();
			ELSE
				move_forward();
			ENDIF
			action1();
		ELSE
		ENDIF
		guard();
	}
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
		IdxOld2New.push_back((UINT)IdxNew2Old.size() - 1);
	}

	//Goto 削除
	for(UINT u = 0; u < IdxNew2Old.size(); ++u){
		g_ChipPool[u] = g_ChipPool[IdxNew2Old[u]];
		if(g_ChipPool[u]->valid_g()) g_ChipPool[u]->m_next_g = IdxOld2New[g_ChipPool[u]->m_next_g];
		if(g_ChipPool[u]->valid_r()) g_ChipPool[u]->m_next_r = IdxOld2New[g_ChipPool[u]->m_next_r];
	}
	g_ChipPool.m_start = IdxOld2New[tree.m_start];
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

	CarnageSA sa(g_ChipPool);
	//sa.run();
	FILE *fp = fopen("chip.svg", "w");
	print_layout_svg(fp, sa.get_result(), g_ChipPool);
	fclose(fp);

	return 0;
}
